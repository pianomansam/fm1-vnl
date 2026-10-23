/* SPDX-License-Identifier: MPL-2.0 */
/* Formant filters adapted from AMB-1 (fm1-amb), MPL-2.0. FM bell adapted from AMB-1 (fm1-amb), MPL-2.0. */
/* Voices and mix (see voices.h). The pitched roles are pools of voices: chords 4, lead 2, candy 2 and the bass, a
   mono pool of 1 whose level slews (AMP_K) and whose M_BASS retriggers are legato. Each voice keeps the patch it was
   struck with, and voice_sample() dispatches on the patch's model: keys (M_EP), plucked strings (M_PLUCK), pads
   (M_PAD), bells (M_BELL), winds (M_WIND) and basses (M_BASS). A pitched
   note's level is the patch envelope: a natural decay (nat, from 1 toward sus) times a gate (g: a linear rise, a
   smoothed fall), so a note-off, a steal or vx_release_all fades instead of clicking. Drums are a noise burst or a swept sine. Then the mix: a 5 kHz one-pole,
   the dark low-pass, the song gain and the soft clip. Per-sample work is bounded by the fixed voice count. */
#include <stdint.h>
#include <string.h>
#include "fastmath.h"
#include "library.h"
#include "voices.h"

#define QN 64
#define NCH 4 /* chord voices */
#define NLD 2 /* lead voices */
#define NCD 2 /* candy voices */
#define NFM (NCH + NLD + NCD)
#define VB NFM       /* the bass voice: fv[VB] */
#define NV (NFM + 1) /* every pitched voice */
#define INV_SR (1.0f / 44100.0f)
#define SR 44100.0f
#define T60 6.9078f  /* ln 1000: time constants in a -60 dB time */
#define OFF_THR 1e-3f
#define MIX_LP 0.509f /* one-pole low-pass at 5 kHz on the mix */
#define G_STEAL 0.02f /* gate fall per sample for a steal: below STEAL_THR in about 4 ms */
#define STEAL_THR 0.02f
#define AMP_K 0.01f /* bass level slew per sample (about 2 ms) */
#define CLIP_T 0.4f /* soft clip: linear below, then a knee that approaches CLIP_L and never reaches it */
#define CLIP_L 0.70f
#define DARK_A 0.192f            /* the dark one-pole's coefficient at d = 1: 1.5 kHz */
#define DARK_STEP (1.0f / 132300.0f) /* the dark ramp per sample: 0 to 1 in 3 s */
#define KR_STOP (T60 / (0.04f * SR)) /* vx_release_all's release: 40 ms to -60 dB */
#define TINE_R 14.0f   /* the EP tine: a high partial, fading with the index */
#define STRIKE_T60 0.25f /* the EP strike's fade */
#define BARK_T60 0.04f   /* the Wurli bark's fade */
#define BARK_LP 0.3f     /* the bark's one-pole on the noise: about 2.5 kHz */
#define IDX_K 0.02f      /* the electric bass's index slew per sample (about 1 ms): a legato restrike doesn't step */
#define WIND_CR 32       /* the winds' control rate: the vibrato updates every 32 samples */
#define WIND_LP_LO 0.3f  /* a wind's low-pass at envelope 0, as a fraction of its cutoff at full */
#define VIB_RATE 5.0f    /* the winds' vibrato: 5 Hz, waiting 0.3 s, then fading in over 0.6 s */
#define VIB_DELAY 0.3f
#define VIB_FADE 0.6f
#define BREATH_K 1.4f    /* the breath's band-pass damping (1/Q): a wide band */
#define CHIFF 1.5f       /* the breath's extra at the onset (the chiff), gone in CHIFF_T60 */
#define CHIFF_T60 0.08f
#define MUTE_DRY 0.2f    /* a muted wind keeps this much of the open tone under the mute's band */

/* big zero-initialised buffers go in the device's .pool RAM region (fm1-base's PLAT_POOL; a Mach-O host has no such
   section) */
#if defined PLAT_HOST || defined __APPLE__
#define VX_POOL
#else
#define VX_POOL __attribute__((section(".pool")))
#endif

/* The pool roles: their voices fv[first .. first+count) and level. */
typedef struct {
    uint8_t first, count;
    float gain;
} role_t;
static const role_t ROLES[V_CANDY + 1] = {
    [V_CHORD] = {0, NCH, 0.07f},
    [V_BASS] = {VB, 1, 0.17f},
    [V_LEAD] = {NCH, NLD, 0.075f},
    [V_CANDY] = {NCH + NLD, NCD, 0.055f},
};

/* A pooled voice. The envelope fields are shared by every model; m is the model's own state. */
typedef struct {
    const vx_patch_t *pt; /* the patch it was struck with (kept through its release) */
    float nat, nk, sus, g, ka, kr, amp;        /* envelope: nat *= nk; g rises by ka, falls by g * kr */
    float amp_t;                               /* the bass: amp slews to amp_t times the natural decay */
    union {
        struct { /* M_EP: phases, increments, index and its decay, tine */
            float pc, pm, ic, im, idx, idec, tine;
            float hit, hk, bark, bk, by; /* EP: strike and its decay, bark level, decay, noise state */
            float la, ly;                /* EP: the tone one-pole's coefficient (0: none) and state */
        } ep;
        struct { /* M_PAD: two saws, then one SVF (a low-pass) or two (the formants) */
            float p1, p2, i1, i2, l2;
            float c1[4], c2[4], s[4]; /* SVF coefficients (a1, a2, a3, k) and states */
            uint8_t vox;
        } pad;
        struct { /* M_PLUCK: a Karplus-Strong line (this voice's ks[] row) and the upright's thump */
            float *buf;
            float fr, xp, ly, c, rho;  /* read fraction, the averager's last input, the damping one-pole, loop gain */
            float tp, ti, th, tk;      /* thump: phase, increment, level, decay */
            uint16_t wp, li;           /* write index, the read delay's whole samples */
        } pl;
        struct { /* M_BELL: AMB-1's 2-op FM bell (phases, increments, the decaying index) and a tremolo */
            float pc, pm, ic, im, idx, idec, tp, ti, td;
        } bl;
        struct { /* M_WIND: an oscillator blend, a two-pole low-pass following the envelope, a mute, breath, vibrato */
            float ph, inc, f0, pw, shp, a0, a1, l1, l2;
            float bc[4], bs[2], br, ch, chk; /* the breath's band-pass (coefficients, states), level, chiff */
            float mc[4], ms[2];             /* the mute's band-pass (mc[3] = 0: no mute) */
            float vp, vd;                   /* vibrato phase (turns) and depth (a fraction of f0) */
            uint32_t vt;                    /* samples since the onset, in control steps */
            uint8_t cc;
        } wd;
        struct { /* M_BASS: 2-op FM (the index's excess ix over the floor i0 slewing to the struck it, which falls)
                    plus a sub-octave sine */
            float pc, pm, ps, ic, im, is, ix, it, i0, ik, sub;
        } bs;
    } m;
    uint32_t gate, age;
    uint8_t on, held, role;
    uint8_t pend; /* a steal in progress: fade out, then start pe on ppt */
    const vx_patch_t *ppt;
    song_ev_t pe;
} voice_t;

typedef struct {
    float env, dec, amp, a, b, ph, pe; /* kick: ph, pe; noise lanes: a, b are filter states */
    uint8_t on;
} drum_t;

typedef struct {
    song_ev_t e;
    uint32_t rem;
} qe_t;

static qe_t q[QN];
static int qn;
static voice_t fv[NV];
static float ks[NV][VX_KS_N] VX_POOL; /* the Karplus-Strong lines, one per voice */
static drum_t dr[V_N]; /* indexed by voice id; the pitched slots are unused */
static uint32_t age_ctr, rng = 0x1234567u;
static float lp_y, gain, gain_tgt, gain_step;
static uint32_t gain_n;
static vx_config_t cfg;
static const vx_patch_t *role_pt[V_CANDY + 1]; /* the configured patch per pitched role, NULL for none */
static float dark, dark_tgt, dk_y;

static float noise(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (float)(int32_t)rng * (1.0f / 2147483648.0f);
}

static float note_inc(int note)
{
    return 440.0f * fm_exp2f(((float)note - 69.0f) * (1.0f / 12.0f)) * INV_SR;
}

/* per-sample coefficients for a -60 dB time t, and the linear rise for an attack time (one sample at least) */
static float k_t60(float t) { return t * SR > T60 ? T60 / (t * SR) : 1.0f; }
static float k_att(float t) { return t * SR > 1.0f ? 1.0f / (t * SR) : 1.0f; }

void vx_init(void)
{
    memset(q, 0, sizeof q);
    memset(fv, 0, sizeof fv);
    memset(dr, 0, sizeof dr);
    qn = 0;
    age_ctr = 0;
    rng = 0x1234567u;
    lp_y = 0.0f;
    gain = gain_tgt = 1.0f;
    gain_step = 0.0f;
    gain_n = 0;
    dark = dark_tgt = dk_y = 0.0f;
    memset(&cfg, 0, sizeof cfg);
    vx_assign(V_CHORD, INS_RHODES);
    vx_assign(V_BASS, INS_SUB);
    vx_assign(V_LEAD, INS_VIBES);
    vx_assign(V_CANDY, INS_MUSIC_BOX);
    cfg.kit = KIT_DUSTY;
    cfg.flavor = FL_LOFI;
}

void vx_assign(int voice, int ins)
{
    if (voice < V_CHORD || voice > V_CANDY)
        return;
    role_pt[voice] = vx_patch(ins);
    ins = role_pt[voice] ? ins : INS_NONE;
    switch (voice) {
    case V_CHORD: cfg.chords = (uint8_t)ins; break;
    case V_BASS: cfg.bass = (uint8_t)ins; break;
    case V_LEAD: cfg.lead = (uint8_t)ins; break;
    default: cfg.candy = (uint8_t)ins; break;
    }
}

void vx_configure(const song_roll_t *r)
{
    const lib_set_t *s = lib_set_by_id((uint8_t)r->v[RF_SET]);
    if (s) {
        vx_assign(V_CHORD, s->chords);
        vx_assign(V_BASS, s->bass);
        vx_assign(V_LEAD, s->lead);
        vx_assign(V_CANDY, s->candy);
        cfg.kit = s->kit;
        cfg.flavor = s->flavor;
    }
    cfg.dust = (uint8_t)fm_clampf((float)r->v[RF_DUST], 0.0f, 3.0f);
    cfg.noise = (uint8_t)fm_clampf((float)r->v[RF_NOISE], 0.0f, 2.0f);
    cfg.ambience = (uint8_t)fm_clampf((float)r->v[RF_AMBIENCE], 0.0f, 1.0f);
    /* Task 7: fx_configure(cfg.dust, cfg.noise, cfg.ambience, cfg.flavor) */
}

const vx_config_t *vx_config(void)
{
    return &cfg;
}

void vx_event(const song_ev_t *e)
{
    if (qn == QN) {
        memmove(&q[0], &q[1], (QN - 1) * sizeof q[0]);
        qn--;
    }
    q[qn].e = *e;
    q[qn].rem = e->delay;
    qn++;
}

/* The envelope's start, shared by every model: nat back to 1, the gate rising from where it is. The bass's level
   slews from where it is on a legato retrigger, else starts at its target. */
static void env_start(voice_t *v, const vx_patch_t *p, const song_ev_t *e, int legato)
{
    v->pt = p;
    v->nat = 1.0f;
    v->nk = 1.0f - k_t60(p->dec);
    v->sus = p->sus;
    v->ka = k_att(p->att);
    v->kr = k_t60(p->rel);
    v->amp_t = ROLES[v->role].gain * p->level * (float)e->vel * (1.0f / 127.0f);
    if (!legato)
        v->amp = v->amp_t;
    v->gate = e->len;
    v->held = 1;
    v->on = 1;
    v->pend = 0;
}

/* M_EP's 2-op FM core: p[0..4] as patch.c documents. */
static void fm_start(voice_t *v, const song_ev_t *e)
{
    const vx_patch_t *p = v->pt;
    float f = note_inc(e->note), v01 = (float)e->vel * (1.0f / 127.0f);
    v->m.ep.ic = f;
    v->m.ep.im = f * p->p[0];
    v->m.ep.idx = p->p[1] + p->p[2] * v01;
    v->m.ep.idec = 1.0f - k_t60(p->p[3]);
    v->m.ep.tine = p->p[4] * v01;
}

/* one-pole low-pass coefficient for a cutoff in Hz (note-on only: it calls fm_expf) */
static float k_lp1(float hz) { return 1.0f - fm_expf(-FM_TWO_PI * fm_clampf(hz, 20.0f, 18000.0f) * INV_SR); }

/* M_EP: the 2-op FM (velocity into the index, the index decaying: the tine's bell into a round body) plus
   a strike that fades fast, a noise bark and a tone one-pole, as patch.c lays out p[]. The modulator restarts at
   the carrier's phase, so every note of a patch has the same attack. */
static void ep_start(voice_t *v, const song_ev_t *e)
{
    const vx_patch_t *p = v->pt;
    float v01 = (float)e->vel * (1.0f / 127.0f);
    fm_start(v, e);
    v->m.ep.pm = v->m.ep.pc;
    v->m.ep.hit = p->p[5] * v01;
    v->m.ep.hk = 1.0f - k_t60(STRIKE_T60);
    v->m.ep.bark = p->p[6] * v01;
    v->m.ep.bk = 1.0f - k_t60(BARK_T60);
    v->m.ep.by = v->m.ep.ly = 0.0f; /* the union may hold another model's state */
    v->m.ep.la = p->p[7] > 0.0f ? k_lp1(p->p[7] * (0.6f + 0.4f * v01)) : 0.0f;
}

/* The TPT state-variable filter's coefficients (Zavalishin), as AMB-1's svf_coefs: f in Hz, k the damping (1/Q). */
static void svf_coefs(float *c, float f, float k)
{
    float g = fm_tanf(FM_PI * fm_clampf(f, 40.0f, 16000.0f) * INV_SR);
    c[0] = 1.0f / (1.0f + g * (g + k));
    c[1] = g * c[0];
    c[2] = g * c[1];
    c[3] = k;
}

/* AMB-1's vowels (u, o, a, e): the first two formants in Hz */
static const float VOWEL[4][2] = {{320.0f, 800.0f}, {480.0f, 900.0f}, {730.0f, 1090.0f}, {530.0f, 1840.0f}};

/* M_PAD: two saws detuned apart, the second at a ratio; a pad into one low-pass SVF, a vox pad into AMB-1's two
   formant filters at a vowel. The filters don't move during a note: their coefficients are set here. */
static void pad_start(voice_t *v, const song_ev_t *e)
{
    const vx_patch_t *p = v->pt;
    float f = note_inc(e->note), dt = fm_exp2f(p->p[0] * (1.0f / 1200.0f));
    v->m.pad.i1 = f / dt;
    v->m.pad.i2 = f * dt * p->p[1];
    v->m.pad.p1 = 0.0f;
    v->m.pad.p2 = 0.5f;
    v->m.pad.l2 = p->p[2];
    v->m.pad.vox = p->p[7] > 0.0f;
    memset(v->m.pad.s, 0, sizeof v->m.pad.s);
    if (v->m.pad.vox) {
        float vw = fm_clampf(p->p[6], 0.0f, 2.999f), f1, f2;
        int a = (int)vw;
        vw -= (float)a;
        f1 = VOWEL[a][0] + (VOWEL[a + 1][0] - VOWEL[a][0]) * vw;
        f2 = VOWEL[a][1] + (VOWEL[a + 1][1] - VOWEL[a][1]) * vw;
        svf_coefs(v->m.pad.c1, f1, 0.45f);
        svf_coefs(v->m.pad.c2, f2, 0.3f);
    } else
        svf_coefs(v->m.pad.c1, p->p[3] + p->p[4] * f * SR, p->p[5]);
}

/* M_BELL: AMB-1's bell (bell_voice): a sine carrier, its modulator at a ratio, the index (velocity into it) falling
   on its own -60 dB time; the envelope's decay scaled by the key (higher bells ring shorter); the vibes' tremolo. */
static void bell_start(voice_t *v, const song_ev_t *e)
{
    const vx_patch_t *p = v->pt;
    float f = note_inc(e->note), v01 = (float)e->vel * (1.0f / 127.0f);
    v->m.bl.pc = v->m.bl.pm = 0.0f;
    v->m.bl.ic = f;
    v->m.bl.im = f * p->p[0];
    v->m.bl.idx = (p->p[1] + p->p[2] * v01) * (1.0f / FM_TWO_PI); /* radians, as turns */
    v->m.bl.idec = 1.0f - k_t60(p->p[3]);
    v->m.bl.td = fm_clampf(p->p[4], 0.0f, 1.0f);
    v->m.bl.ti = p->p[5] * INV_SR;
    v->m.bl.tp = 0.0f;
    v->nk = 1.0f - k_t60(p->dec * fm_exp2f(-((float)e->note - 60.0f) * (1.0f / 12.0f) * p->p[6]));
}

/* M_WIND: one phase drives the blend (sine, triangle, pulse); a two-pole low-pass opens with the envelope; a muted
   wind goes through a narrow band-pass; a band of breath noise rides under it, with a chiff at the onset; the
   vibrato waits, then fades in. */
static void wind_start(voice_t *v, const song_ev_t *e)
{
    const vx_patch_t *p = v->pt;
    float f = note_inc(e->note), v01 = (float)e->vel * (1.0f / 127.0f);
    float hi = k_lp1(p->p[2] * f * SR * (0.6f + 0.4f * v01));
    v->m.wd.ph = 0.0f;
    v->m.wd.inc = v->m.wd.f0 = f;
    v->m.wd.pw = fm_clampf(p->p[1], 0.05f, 0.5f);
    v->m.wd.shp = fm_clampf(p->p[0], 0.0f, 2.0f);
    v->m.wd.a0 = k_lp1(WIND_LP_LO * p->p[2] * f * SR);
    v->m.wd.a1 = hi - v->m.wd.a0;
    v->m.wd.l1 = v->m.wd.l2 = 0.0f;
    svf_coefs(v->m.wd.bc, p->p[4], BREATH_K);
    v->m.wd.bs[0] = v->m.wd.bs[1] = 0.0f;
    v->m.wd.br = p->p[3];
    v->m.wd.ch = CHIFF;
    v->m.wd.chk = 1.0f - k_t60(CHIFF_T60);
    if (p->p[6] > 0.0f)
        svf_coefs(v->m.wd.mc, p->p[6], p->p[7]);
    else
        v->m.wd.mc[3] = 0.0f;
    v->m.wd.ms[0] = v->m.wd.ms[1] = 0.0f;
    v->m.wd.vp = 0.0f;
    v->m.wd.vd = p->p[5] * (FM_LN2 / 1200.0f); /* cents, as a fraction of the frequency */
    v->m.wd.vt = 0;
    v->m.wd.cc = 0;
}

/* atan(y / x) for x > 0, y >= 0 (note-on only): a polynomial on [0, 1], folded; about 1e-5 rad */
static float atan_pos(float y, float x)
{
    int fold = y > x;
    float t = fold ? x / y : y / x, t2 = t * t, a;
    a = t * (0.9998660f + t2 * (-0.3302995f + t2 * (0.1801410f + t2 * (-0.0851330f + t2 * 0.0208351f))));
    return fold ? 0.5f * FM_PI - a : a;
}

/* M_PLUCK: Karplus-Strong. A burst of low-passed noise (zero mean, unit RMS) fills the line; the loop is the line
   (a fractional delay, linear interpolation), a two-point averager and a damping one-pole, then a gain that sets the
   fundamental's -60 dB time. The loop's delay is the period: the averager's half sample and the one-pole's phase
   delay at the fundamental come off the line. The upright adds a thump: a sine at the fundamental, gone fast. */
static void pluck_start(voice_t *v, const song_ev_t *e)
{
    const vx_patch_t *p = v->pt;
    float f = note_inc(e->note), v01 = (float)e->vel * (1.0f / 127.0f), w = FM_TWO_PI * f, cw = fm_cosf(w);
    float a, len, bc, by = 0.0f, sum = 0.0f, sq = 0.0f, mean, sc, mag;
    int i, n;
    v->m.pl.buf = ks[v - fv];
    v->m.pl.c = k_lp1((p->p[1] + p->p[5] * f * SR) * (0.6f + 0.4f * v01));
    a = 1.0f - v->m.pl.c;
    len = 1.0f / f - 0.5f - atan_pos(a * fm_sinf(w), 1.0f - a * cw) / w;
    len = fm_clampf(len, 2.0f, (float)(VX_KS_N - 1) - 0.001f);
    v->m.pl.li = (uint16_t)len;
    v->m.pl.fr = len - (float)v->m.pl.li;
    /* the fundamental's loss per period, less what the averager and the one-pole already take there */
    mag = fm_cosf(0.5f * w) * v->m.pl.c * fm_rsqrtf(1.0f - 2.0f * a * cw + a * a);
    v->m.pl.rho = fm_minf(fm_expf(-T60 / (p->p[0] * SR * f)) / mag, 0.99999f);
    /* the burst: ages 1 .. li + 1 (buf[li] .. buf[0]) are all the loop reads before it writes its own */
    n = v->m.pl.li + 1;
    bc = k_lp1(p->p[2] * (0.6f + 0.4f * v01));
    for (i = 0; i < n; i++) {
        by += bc * (noise() - by);
        v->m.pl.buf[i] = by;
        sum += by;
        sq += by * by;
    }
    mean = sum / (float)n;
    sc = fm_rsqrtf(fm_maxf(sq / (float)n - mean * mean, 1e-12f));
    for (i = 0; i < n; i++)
        v->m.pl.buf[i] = (v->m.pl.buf[i] - mean) * sc;
    v->m.pl.wp = (uint16_t)(n == VX_KS_N ? 0 : n);
    v->m.pl.xp = v->m.pl.buf[0];
    v->m.pl.ly = 0.0f;
    v->m.pl.tp = 0.0f;
    v->m.pl.ti = f;
    v->m.pl.th = p->p[3] * v01;
    v->m.pl.tk = 1.0f - k_t60(p->p[4]);
}

/* M_BASS: 2-op FM whose index slews to a struck target falling to its floor, plus a sine an octave down. A legato
   retrigger keeps the phases and slews the index, so it doesn't step; a fresh note starts every phase at 0. */
static void bass_start(voice_t *v, const song_ev_t *e, int legato)
{
    const vx_patch_t *p = v->pt;
    float f = note_inc(e->note), v01 = (float)e->vel * (1.0f / 127.0f);
    if (!legato) {
        v->m.bs.pc = v->m.bs.pm = v->m.bs.ps = 0.0f;
        v->m.bs.ix = p->p[2] * v01;
    }
    v->m.bs.ic = f;
    v->m.bs.im = f * p->p[0];
    v->m.bs.is = f * 0.5f;
    v->m.bs.i0 = p->p[1];
    v->m.bs.it = p->p[2] * v01;
    v->m.bs.ik = 1.0f - k_t60(p->p[3]);
    v->m.bs.sub = p->p[4];
}

static void voice_start(voice_t *v, const vx_patch_t *p, const song_ev_t *e, int legato)
{
    env_start(v, p, e, legato);
    switch (p->model) {
    case M_EP: ep_start(v, e); break;
    case M_PLUCK: pluck_start(v, e); break;
    case M_PAD: pad_start(v, e); break;
    case M_BELL: bell_start(v, e); break;
    case M_WIND: wind_start(v, e); break;
    default: bass_start(v, e, legato); break; /* M_BASS */
    }
}

/* A free voice starts at once. Otherwise the victim is the quietest released voice, else the oldest held one, and it
   fades out over about 4 ms (G_STEAL) before the new note starts: the new note begins up to ~5 ms after its
   sample-accurate time, with no step in the output. */
static void pool_on(const song_ev_t *e, int role)
{
    int i, pick = -1, first = ROLES[role].first, count = ROLES[role].count;
    const vx_patch_t *p = role_pt[role];
    voice_t *v;
    if (!p)
        return; /* no instrument on this role */
    for (i = first; i < first + count; i++)
        if (!fv[i].on) {
            pick = i;
            break;
        }
    if (pick < 0)
        for (i = first; i < first + count; i++)
            if (!fv[i].held && !fv[i].pend && (pick < 0 || fv[i].g < fv[pick].g))
                pick = i;
    if (pick < 0)
        for (i = first; i < first + count; i++)
            if (pick < 0 || fv[i].age < fv[pick].age)
                pick = i;
    v = &fv[pick];
    v->age = ++age_ctr;
    if (!v->on) {
        v->g = 0.0f;
        v->role = (uint8_t)role;
        voice_start(v, p, e, 0);
    } else if (role == V_BASS && !v->pend && v->pt == p && p->model == M_BASS) {
        voice_start(v, p, e, 1); /* legato: phases, gate and level carry over */
    } else { /* a steal; on the bass, a re-pluck or a new patch */
        v->pe = *e;
        v->ppt = p;
        v->pend = 1;
    }
}

static void drum_on(const song_ev_t *e)
{
    drum_t *d = &dr[e->voice];
    float v01 = (float)e->vel * (1.0f / 127.0f);
    d->env = 1.0f;
    d->on = 1;
    switch (e->voice) {
    case V_KICK:
        d->amp = 0.4f * v01;
        d->dec = 0.99985f;
        d->ph = 0.25f; /* cosine start: the click is the first sample */
        d->pe = 1.0f;
        break;
    case V_SNARE:
        d->amp = 0.2f * v01;
        d->dec = 0.9998f;
        break;
    case V_RIM:
        d->amp = 0.12f * v01;
        d->dec = 0.9985f;
        break;
    case V_HAT:
        d->amp = 0.07f * v01;
        d->dec = 0.9992f;
        break;
    case V_OHAT:
        d->amp = 0.06f * v01;
        d->dec = 0.99985f;
        break;
    default: /* V_SHAKER */
        d->amp = 0.06f * v01;
        d->dec = 0.9995f;
        break;
    }
}

static void fire(const song_ev_t *e)
{
    switch (e->voice) {
    case V_CHORD:
    case V_LEAD:
    case V_CANDY:
    case V_BASS: pool_on(e, e->voice); break;
    default:
        if (e->voice < V_N)
            drum_on(e);
        break;
    }
}

/* M_EP's FM core: the raw sample (unit level). */
static float fm_sample(voice_t *v)
{
    float s, mod;
    v->m.ep.pc += v->m.ep.ic;
    if (v->m.ep.pc >= 1.0f)
        v->m.ep.pc -= 1.0f;
    v->m.ep.pm += v->m.ep.im;
    if (v->m.ep.pm >= 1.0f)
        v->m.ep.pm -= 1.0f;
    mod = fm_sin_turns(v->m.ep.pm) * v->m.ep.idx;
    s = fm_sin_turns(v->m.ep.pc + mod * 0.159155f);
    if (v->m.ep.tine > 0.0f) /* the tine fades with the index */
        s += v->m.ep.tine * fm_sin_turns(v->m.ep.pc * TINE_R) * v->m.ep.idx;
    v->m.ep.idx = fm_flush(v->m.ep.idx * v->m.ep.idec); /* decays: flushed before it goes denormal */
    return s;
}

/* M_EP's raw sample: the FM and tine, the strike, the bark, the tone. */
static float ep_sample(voice_t *v)
{
    float s = fm_sample(v) * (1.0f + v->m.ep.hit);
    v->m.ep.hit = fm_flush(v->m.ep.hit * v->m.ep.hk);
    if (v->m.ep.bark > 1e-4f) { /* band-limited noise, gated by the tone: a reed's bark */
        v->m.ep.by += BARK_LP * (noise() - v->m.ep.by);
        s += v->m.ep.bark * v->m.ep.by * (s < 0.0f ? -s : s);
        v->m.ep.bark *= v->m.ep.bk;
    }
    if (v->m.ep.la > 0.0f) {
        v->m.ep.ly = fm_flush(v->m.ep.ly + v->m.ep.la * (s - v->m.ep.ly));
        s = v->m.ep.ly;
    }
    return s;
}

/* a naive saw with a polyBLEP at the wrap: t the phase in turns, dt its increment */
static float saw_blep(float t, float dt)
{
    float y = 2.0f * t - 1.0f;
    if (t < dt) {
        float x = t / dt;
        y -= x + x - x * x - 1.0f;
    } else if (t > 1.0f - dt) {
        float x = (t - 1.0f) / dt;
        y -= x * x + x + x + 1.0f;
    }
    return y;
}

/* one TPT SVF step on x: the band-pass (unscaled) in *bp, the low-pass returned; s[0..1] its states */
static float svf_step(const float *c, float *s, float x, float *bp)
{
    float v3 = x - s[1], v1 = c[0] * s[0] + c[1] * v3, v2 = s[1] + c[1] * s[0] + c[2] * v3;
    s[0] = fm_flush(2.0f * v1 - s[0]);
    s[1] = fm_flush(2.0f * v2 - s[1]);
    *bp = v1;
    return v2;
}

/* M_PAD's raw sample: the two saws, then the low-pass or the formants (AMB-1's mix: b1 + b2 / 2 + 0.45 l1). */
static float pad_sample(voice_t *v)
{
    float x, l1, b1, b2;
    x = saw_blep(v->m.pad.p1, v->m.pad.i1) + v->m.pad.l2 * saw_blep(v->m.pad.p2, v->m.pad.i2);
    v->m.pad.p1 += v->m.pad.i1;
    if (v->m.pad.p1 >= 1.0f)
        v->m.pad.p1 -= 1.0f;
    v->m.pad.p2 += v->m.pad.i2;
    if (v->m.pad.p2 >= 1.0f)
        v->m.pad.p2 -= 1.0f;
    l1 = svf_step(v->m.pad.c1, v->m.pad.s, x, &b1);
    if (!v->m.pad.vox)
        return l1;
    svf_step(v->m.pad.c2, v->m.pad.s + 2, x, &b2);
    return b1 * v->m.pad.c1[3] + 0.5f * b2 * v->m.pad.c2[3] + 0.45f * l1;
}

/* M_PLUCK's raw sample: the line's output through the averager and the damping, back into the line; the thump. */
static float pluck_sample(voice_t *v)
{
    float *b = v->m.pl.buf, x, y;
    int i0 = (int)v->m.pl.wp - (int)v->m.pl.li, i1;
    if (i0 < 0)
        i0 += VX_KS_N;
    i1 = i0 ? i0 - 1 : VX_KS_N - 1;
    x = b[i0] + v->m.pl.fr * (b[i1] - b[i0]);
    v->m.pl.ly = fm_flush(v->m.pl.ly + v->m.pl.c * (0.5f * (x + v->m.pl.xp) - v->m.pl.ly));
    v->m.pl.xp = x;
    y = v->m.pl.ly * v->m.pl.rho;
    b[v->m.pl.wp] = y;
    if (++v->m.pl.wp == VX_KS_N)
        v->m.pl.wp = 0;
    if (v->m.pl.th > 0.0f) {
        y += v->m.pl.th * fm_sin_turns(v->m.pl.tp);
        v->m.pl.tp += v->m.pl.ti;
        if (v->m.pl.tp >= 1.0f)
            v->m.pl.tp -= 1.0f;
        v->m.pl.th = fm_flush(v->m.pl.th * v->m.pl.tk);
    }
    return y;
}

/* M_BELL's raw sample: AMB-1's render_bells (the FM, the index falling), times the tremolo (1 at the onset, down by
   the depth at its trough). */
static float bell_sample(voice_t *v)
{
    float s = fm_sin_turns(v->m.bl.pc + fm_sin_turns(v->m.bl.pm) * v->m.bl.idx);
    v->m.bl.pc += v->m.bl.ic;
    if (v->m.bl.pc >= 1.0f)
        v->m.bl.pc -= 1.0f;
    v->m.bl.pm += v->m.bl.im;
    if (v->m.bl.pm >= 1.0f)
        v->m.bl.pm -= 1.0f;
    v->m.bl.idx = fm_flush(v->m.bl.idx * v->m.bl.idec); /* decays: flushed before it goes denormal */
    if (v->m.bl.td > 0.0f) {
        s *= 1.0f - v->m.bl.td * (0.5f - 0.5f * fm_sin_turns(v->m.bl.tp + 0.25f));
        v->m.bl.tp += v->m.bl.ti;
        if (v->m.bl.tp >= 1.0f)
            v->m.bl.tp -= 1.0f;
    }
    return s;
}

/* M_WIND's raw sample. The vibrato runs at the control rate (WIND_CR). */
static float wind_sample(voice_t *v)
{
    float t = v->m.wd.ph, s = v->m.wd.shp, x, e, a, bp;
    if (++v->m.wd.cc == WIND_CR) {
        float r = ((float)(++v->m.wd.vt) * (WIND_CR * INV_SR) - VIB_DELAY) * (1.0f / VIB_FADE);
        v->m.wd.cc = 0;
        if (r > 0.0f) {
            v->m.wd.inc = v->m.wd.f0 * (1.0f + fm_minf(r, 1.0f) * v->m.wd.vd * fm_sin_turns(v->m.wd.vp));
            v->m.wd.vp += VIB_RATE * WIND_CR * INV_SR;
            if (v->m.wd.vp >= 1.0f)
                v->m.wd.vp -= 1.0f;
        }
    }
    if (s < 1.0f) /* sine to triangle */
        x = (1.0f - s) * fm_sin_turns(t) + s * (1.0f - 4.0f * fm_fabsf(t - 0.5f));
    else { /* triangle to pulse: a saw less a saw a width later, each with its polyBLEP */
        float t2 = t + v->m.wd.pw;
        if (t2 >= 1.0f)
            t2 -= 1.0f;
        x = (2.0f - s) * (1.0f - 4.0f * fm_fabsf(t - 0.5f)) +
            (s - 1.0f) * (saw_blep(t, v->m.wd.inc) - saw_blep(t2, v->m.wd.inc));
    }
    v->m.wd.ph = t + v->m.wd.inc;
    if (v->m.wd.ph >= 1.0f)
        v->m.wd.ph -= 1.0f;
    e = v->g * (v->sus + (1.0f - v->sus) * v->nat); /* the envelope opens the low-pass */
    a = v->m.wd.a0 + v->m.wd.a1 * e;
    v->m.wd.l1 = fm_flush(v->m.wd.l1 + a * (x - v->m.wd.l1));
    v->m.wd.l2 = fm_flush(v->m.wd.l2 + a * (v->m.wd.l1 - v->m.wd.l2));
    x = v->m.wd.l2;
    if (v->m.wd.mc[3] > 0.0f) { /* the mute: a narrow band, unity at its center */
        svf_step(v->m.wd.mc, v->m.wd.ms, x, &bp);
        x = MUTE_DRY * x + bp * v->m.wd.mc[3];
    }
    svf_step(v->m.wd.bc, v->m.wd.bs, noise(), &bp);
    x += v->m.wd.br * (1.0f + v->m.wd.ch) * bp * BREATH_K;
    v->m.wd.ch = fm_flush(v->m.wd.ch * v->m.wd.chk);
    return x;
}

/* M_BASS's raw sample: the FM and the sub-octave sine. */
static float bass_sample(voice_t *v)
{
    float s, idx;
    v->m.bs.pc += v->m.bs.ic;
    if (v->m.bs.pc >= 1.0f)
        v->m.bs.pc -= 1.0f;
    v->m.bs.pm += v->m.bs.im;
    if (v->m.bs.pm >= 1.0f)
        v->m.bs.pm -= 1.0f;
    if (v->m.bs.it > 0.0f || v->m.bs.ix > 0.0f) { /* the strike: both flush to exactly 0 once it has settled */
        v->m.bs.it = fm_flush(v->m.bs.it * v->m.bs.ik);
        v->m.bs.ix = fm_flush(v->m.bs.ix + IDX_K * (v->m.bs.it - v->m.bs.ix));
    }
    idx = v->m.bs.i0 + v->m.bs.ix;
    s = idx > 0.0f ? fm_sin_turns(v->m.bs.pc + fm_sin_turns(v->m.bs.pm) * idx * 0.159155f)
                   : fm_sin_turns(v->m.bs.pc); /* the sub: no modulator */
    if (v->m.bs.sub > 0.0f) {
        v->m.bs.ps += v->m.bs.is;
        if (v->m.bs.ps >= 1.0f)
            v->m.bs.ps -= 1.0f;
        s += v->m.bs.sub * fm_sin_turns(v->m.bs.ps);
    }
    return s;
}

/* One sample of a pooled voice: the model's raw sample times the envelope (on the bass, its slewed level). */
static float voice_sample(voice_t *v)
{
    float s, o, lv;
    switch (v->pt->model) {
    case M_EP: s = ep_sample(v); break;
    case M_PLUCK: s = pluck_sample(v); break;
    case M_PAD: s = pad_sample(v); break;
    case M_BELL: s = bell_sample(v); break;
    case M_WIND: s = wind_sample(v); break;
    default: s = bass_sample(v); break; /* M_BASS */
    }
    v->nat = fm_flush(v->nat * v->nk); /* a note held past ~50 s would reach denormals */
    lv = v->sus + (1.0f - v->sus) * v->nat;
    if (v->role == V_BASS) {
        v->amp = fm_flush(v->amp + (v->amp_t * lv - v->amp) * AMP_K);
        o = s * v->amp * v->g;
    } else
        o = s * v->amp * lv * v->g;
    if (v->pend) { /* fade the victim out, then start the queued note */
        v->g -= v->g * G_STEAL;
        if (v->g < STEAL_THR)
            voice_start(v, v->ppt, &v->pe, 0);
        return o;
    }
    if (v->held && v->gate && --v->gate == 0)
        v->held = 0;
    if (v->held) {
        v->g += v->ka;
        if (v->g > 1.0f)
            v->g = 1.0f;
    } else {
        v->g -= v->g * v->kr;
        if (v->g < OFF_THR)
            v->on = 0;
    }
    return o;
}

/* One sample of a drum lane; env falls by dec each sample and the lane is off below the threshold. */
static float drum_sample(int id)
{
    drum_t *d = &dr[id];
    float n, s;
    switch (id) {
    case V_KICK:
        d->ph += (45.0f + 105.0f * d->pe) * INV_SR;
        if (d->ph >= 1.0f)
            d->ph -= 1.0f;
        d->pe *= 0.99924f; /* about 30 ms constant: the drop is done in 120 ms */
        s = fm_sin_turns(d->ph);
        break;
    case V_SNARE:
    case V_RIM:
        n = noise();
        d->a += 0.4f * (n - d->a);          /* low-pass near 4 kHz */
        d->b += (id == V_SNARE ? 0.03f : 0.12f) * (d->a - d->b); /* high-pass below: tracks the low end */
        s = (d->a - d->b) * 1.4f;
        break;
    default: /* hats, shaker: high-passed noise */
        n = noise();
        d->a += (id == V_SHAKER ? 0.45f : 0.7f) * (n - d->a);
        s = n - d->a;
        break;
    }
    s *= d->amp * d->env;
    d->env *= d->dec;
    if (d->env < OFF_THR)
        d->on = 0;
    return s;
}

static void render_seg(float *l, float *r, uint32_t n)
{
    uint32_t i;
    int j, d;
    for (i = 0; i < n; i++) {
        float x = 0.0f, g;
        for (j = 0; j < NV; j++)
            if (fv[j].on)
                x += voice_sample(&fv[j]);
        for (d = V_KICK; d < V_N; d++)
            if (dr[d].on)
                x += drum_sample(d);
        lp_y += MIX_LP * (x - lp_y);
        lp_y = fm_flush(lp_y);
        if (dark != dark_tgt) /* the ramp: linear, landing exactly */
            dark = dark < dark_tgt ? fm_minf(dark + DARK_STEP, dark_tgt) : fm_maxf(dark - DARK_STEP, dark_tgt);
        if (dark > 0.0f) { /* the dark one-pole: open (a = 1) at 0, 1.5 kHz at 1 */
            dk_y += (1.0f - dark * (1.0f - DARK_A)) * (lp_y - dk_y);
            dk_y = fm_flush(dk_y);
        } else
            dk_y = lp_y;
        if (gain_n) {
            gain += gain_step;
            if (--gain_n == 0)
                gain = gain_tgt;
        }
        g = gain;
        x = dk_y < 0.0f ? -dk_y : dk_y; /* soft clip: |y| < CLIP_L whatever the sum */
        if (x > CLIP_T) {
            x -= CLIP_T;
            x = CLIP_T + (CLIP_L - CLIP_T) * x / (x + (CLIP_L - CLIP_T));
        }
        if (dk_y < 0.0f)
            x = -x;
        l[i] += x * g;
        r[i] += x * g;
    }
}

void vx_render(float *l, float *r, uint32_t n)
{
    uint32_t pos = 0;
    while (pos < n) {
        int i, w = 0;
        uint32_t seg = n - pos;
        for (i = 0; i < qn; i++) { /* start what is due, keeping the order of the rest */
            if (q[i].rem == 0)
                fire(&q[i].e);
            else
                q[w++] = q[i];
        }
        qn = w;
        for (i = 0; i < qn; i++)
            if (q[i].rem < seg)
                seg = q[i].rem;
        render_seg(l + pos, r + pos, seg);
        for (i = 0; i < qn; i++)
            q[i].rem -= seg;
        pos += seg;
    }
}

void vx_release_all(void)
{
    int i;
    for (i = 0; i < NV; i++) { /* a long release (a pad's) shortened to 40 ms */
        fv[i].held = 0;
        fv[i].pend = 0;
        fv[i].kr = fm_maxf(fv[i].kr, KR_STOP);
    }
    for (i = V_KICK; i < V_N; i++)
        if (dr[i].on)
            dr[i].dec = dr[i].dec < 0.996f ? dr[i].dec : 0.996f; /* under 40 ms to the threshold */
    qn = 0;
}

void vx_set_gain(float g)
{
    gain_tgt = g < 0.0f ? 0.0f : g > 1.0f ? 1.0f : g;
    gain_step = (gain_tgt - gain) * (1.0f / 882.0f);
    gain_n = 882;
}

int vx_active(void)
{
    int i, c = 0;
    for (i = 0; i < NV; i++)
        c += fv[i].on;
    for (i = V_KICK; i < V_N; i++)
        c += dr[i].on;
    return c;
}

void vx_set_dark(float d)
{
    dark_tgt = fm_clampf(d, 0.0f, 1.0f);
    if (!vx_active()) /* a ramp nobody hears */
        dark = dark_tgt;
}

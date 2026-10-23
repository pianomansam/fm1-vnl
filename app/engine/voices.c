/* SPDX-License-Identifier: MPL-2.0 */
/* Voices and mix (see voices.h). The pitched roles are pools of voices (chords 4, lead 2, candy 2) plus the mono
   bass; each voice keeps the patch it was struck with, and voice_sample() dispatches on the patch's model (every model
   falls back to the FM voice for now: Tasks 3-5 add a case each). A pitched note's level is the patch envelope: a
   natural decay (nat, from 1 toward sus) times a gate (g: a linear rise, a smoothed fall), so a note-off, a steal or
   vx_release_all fades instead of clicking. Drums are a noise burst or a swept sine. Then the mix: a 5 kHz one-pole,
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

/* The pool roles: their voices fv[first .. first+count) and level. The bass is its own mono voice (BASS_GAIN). */
typedef struct {
    uint8_t first, count;
    float gain;
} role_t;
static const role_t ROLES[V_CANDY + 1] = {
    [V_CHORD] = {0, NCH, 0.07f},
    [V_LEAD] = {NCH, NLD, 0.075f},
    [V_CANDY] = {NCH + NLD, NCD, 0.055f},
};
#define BASS_GAIN 0.17f

/* A pooled voice. The envelope fields are shared by every model; the FM fields are the fallback model's. */
typedef struct {
    const vx_patch_t *pt; /* the patch it was struck with (kept through its release) */
    float nat, nk, sus, g, ka, kr, amp;        /* envelope: nat *= nk; g rises by ka, falls by g * kr */
    float pc, pm, ic, im, idx, idec, tine;     /* fallback FM */
    uint32_t gate, age;
    uint8_t on, held, role;
    uint8_t pend; /* a steal in progress: fade out, then start pe on ppt */
    const vx_patch_t *ppt;
    song_ev_t pe;
} voice_t;

typedef struct {
    const vx_patch_t *pt;
    float nat, nk, sus, g, ka, kr;
    float p1, p2, i1, i2, amp, amp_t;
    uint32_t gate;
    uint8_t on, held;
} bass_t;

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
static voice_t fv[NFM];
static bass_t bs;
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
    memset(&bs, 0, sizeof bs);
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

/* The envelope's start, shared by every model: nat back to 1, the gate rising from where it is. */
static void env_start(voice_t *v, const vx_patch_t *p, const song_ev_t *e)
{
    v->pt = p;
    v->nat = 1.0f;
    v->nk = 1.0f - k_t60(p->dec);
    v->sus = p->sus;
    v->ka = k_att(p->att);
    v->kr = k_t60(p->rel);
    v->amp = ROLES[v->role].gain * p->level * (float)e->vel * (1.0f / 127.0f);
    v->gate = e->len;
    v->held = 1;
    v->on = 1;
    v->pend = 0;
}

/* The fallback FM (every model, for now): p[] as patch.c documents. */
static void fm_start(voice_t *v, const song_ev_t *e)
{
    const vx_patch_t *p = v->pt;
    float f = note_inc(e->note), v01 = (float)e->vel * (1.0f / 127.0f);
    v->ic = f;
    v->im = f * p->p[0];
    v->idx = p->p[1] + p->p[2] * v01;
    v->idec = 1.0f - k_t60(p->p[3]);
    v->tine = p->p[4] * v01;
}

static void voice_start(voice_t *v, const vx_patch_t *p, const song_ev_t *e)
{
    env_start(v, p, e);
    switch (p->model) {
    default: fm_start(v, e); break; /* M_EP ... M_BASS: Tasks 3-5 */
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
        voice_start(v, p, e);
    } else {
        v->pe = *e;
        v->ppt = p;
        v->pend = 1;
    }
}

/* The sine bass (every bass-role model, for now), with the patch's envelope. A retrigger is continuous: phases and
   gate carry over, and the level (nat included) slews. */
static void bass_on(const song_ev_t *e)
{
    const vx_patch_t *p = role_pt[V_BASS];
    float f = note_inc(e->note);
    if (!p)
        return;
    bs.pt = p;
    bs.nat = 1.0f;
    bs.nk = 1.0f - k_t60(p->dec);
    bs.sus = p->sus;
    bs.ka = k_att(p->att);
    bs.kr = k_t60(p->rel);
    bs.i1 = f;
    bs.i2 = f * 0.5f;
    bs.amp_t = BASS_GAIN * p->level * (float)e->vel * (1.0f / 127.0f);
    bs.gate = e->len;
    bs.held = 1;
    bs.on = 1; /* phases and gate level carry over: a retrigger is continuous */
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
    case V_CANDY: pool_on(e, e->voice); break;
    case V_BASS: bass_on(e); break;
    default:
        if (e->voice < V_N)
            drum_on(e);
        break;
    }
}

/* The fallback FM's raw sample (unit level). */
static float fm_sample(voice_t *v)
{
    float s, mod;
    v->pc += v->ic;
    if (v->pc >= 1.0f)
        v->pc -= 1.0f;
    v->pm += v->im;
    if (v->pm >= 1.0f)
        v->pm -= 1.0f;
    mod = fm_sin_turns(v->pm) * v->idx;
    s = fm_sin_turns(v->pc + mod * 0.159155f);
    if (v->tine > 0.0f) /* the tine fades with the index */
        s += v->tine * fm_sin_turns(v->pc * 14.0f) * v->idx;
    v->idx *= v->idec;
    return s;
}

/* One sample of a pooled voice: the model's raw sample times the envelope. */
static float voice_sample(voice_t *v)
{
    float s, o;
    switch (v->pt->model) {
    default: s = fm_sample(v); break; /* M_EP ... M_BASS: Tasks 3-5 */
    }
    v->nat *= v->nk;
    o = s * v->amp * (v->sus + (1.0f - v->sus) * v->nat) * v->g;
    if (v->pend) { /* fade the victim out, then start the queued note on the same phases */
        v->g -= v->g * G_STEAL;
        if (v->g < STEAL_THR)
            voice_start(v, v->ppt, &v->pe);
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

static float bass_sample(void)
{
    float s;
    bs.p1 += bs.i1;
    if (bs.p1 >= 1.0f)
        bs.p1 -= 1.0f;
    bs.p2 += bs.i2;
    if (bs.p2 >= 1.0f)
        bs.p2 -= 1.0f;
    bs.nat *= bs.nk;
    bs.amp += (bs.amp_t * (bs.sus + (1.0f - bs.sus) * bs.nat) - bs.amp) * AMP_K;
    s = fm_sin_turns(bs.p1) + 0.6f * fm_sin_turns(bs.p2);
    if (bs.held && bs.gate && --bs.gate == 0)
        bs.held = 0;
    if (bs.held) {
        bs.g += bs.ka;
        if (bs.g > 1.0f)
            bs.g = 1.0f;
    } else {
        bs.g -= bs.g * bs.kr;
        if (bs.g < OFF_THR)
            bs.on = 0;
    }
    return s * bs.amp * bs.g;
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
        for (j = 0; j < NFM; j++)
            if (fv[j].on)
                x += voice_sample(&fv[j]);
        if (bs.on)
            x += bass_sample();
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
    for (i = 0; i < NFM; i++) {
        fv[i].held = 0;
        fv[i].pend = 0;
    }
    bs.held = 0;
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
    int i, c = bs.on;
    for (i = 0; i < NFM; i++)
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

/* SPDX-License-Identifier: MPL-2.0 */
/* Placeholder voices and mix (see voices.h). Every voice is a pair of phase accumulators (turns, [0,1)) or a noise
   burst; a natural decay (nat) times a smoothed gate (g) shapes pitched notes, so a note-off, a steal or
   vx_release_all fades instead of clicking. Per-sample work is bounded by the fixed voice count. */
#include <stdint.h>
#include <string.h>
#include "fastmath.h"
#include "voices.h"

#define QN 64
#define NCH 4 /* chord voices */
#define NLD 2 /* lead voices */
#define NCD 2 /* candy voices */
#define NFM (NCH + NLD + NCD)
#define INV_SR (1.0f / 44100.0f)
#define G_ATT 0.02f  /* gate rise per sample (about 1 ms) */
#define G_REL 0.006f /* gate fall per sample: below 1e-3 in about 26 ms */
#define OFF_THR 1e-3f
#define MIX_LP 0.509f /* one-pole low-pass at 5 kHz on the mix */

enum { K_CHORD, K_LEAD, K_CANDY };

typedef struct {
    float pc, ic, im, nat, dec, idx, idec, g, amp, tine;
    uint32_t gate, age;
    uint8_t on, held, kind;
} fm_t;

typedef struct {
    float p1, p2, i1, i2, g, amp;
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
static fm_t fv[NFM];
static bass_t bs;
static drum_t dr[V_N]; /* indexed by voice id; the pitched slots are unused */
static uint32_t age_ctr, rng = 0x1234567u;
static float lp_y, gain, gain_step;
static uint32_t gain_n;

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
    gain = 1.0f;
    gain_step = 0.0f;
    gain_n = 0;
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

static void fm_on(const song_ev_t *e, int first, int count, uint8_t kind)
{
    int i, pick = first;
    fm_t *v;
    float f, v01 = (float)e->vel * (1.0f / 127.0f);
    for (i = first; i < first + count; i++) { /* a free voice, else the oldest */
        if (!fv[i].on) {
            pick = i;
            break;
        }
        if (fv[i].age < fv[pick].age)
            pick = i;
    }
    v = &fv[pick];
    f = note_inc(e->note);
    v->ic = f;
    v->kind = kind;
    v->nat = 1.0f;
    v->age = ++age_ctr;
    v->gate = e->len;
    v->held = 1;
    v->on = 1;
    if (kind == K_CHORD) { /* EP: 1:1 body, 1:14 tine, low index */
        v->im = f;
        v->idx = 0.35f + 0.9f * v01;
        v->idec = 0.99992f;
        v->dec = 0.99997f;
        v->amp = 0.07f * v01;
        v->tine = 0.12f * v01;
    } else { /* bell: 1:3.5 */
        v->im = f * 3.5f;
        v->idx = 0.6f + 1.4f * v01;
        v->idec = kind == K_LEAD ? 0.99985f : 0.99975f;
        v->dec = kind == K_LEAD ? 0.99992f : 0.99985f;
        v->amp = (kind == K_LEAD ? 0.075f : 0.055f) * v01;
        v->tine = 0.0f;
    }
}

static void bass_on(const song_ev_t *e)
{
    float f = note_inc(e->note);
    bs.i1 = f;
    bs.i2 = f * 0.5f;
    bs.amp = 0.17f * (float)e->vel * (1.0f / 127.0f);
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
    case V_CHORD: fm_on(e, 0, NCH, K_CHORD); break;
    case V_LEAD: fm_on(e, NCH, NLD, K_LEAD); break;
    case V_CANDY: fm_on(e, NCH + NLD, NCD, K_CANDY); break;
    case V_BASS: bass_on(e); break;
    default:
        if (e->voice < V_N)
            drum_on(e);
        break;
    }
}

static float fm_sample(fm_t *v)
{
    float s, mod, tg;
    v->pc += v->ic;
    if (v->pc >= 1.0f)
        v->pc -= 1.0f;
    mod = fm_sin_turns(v->pc * (v->kind == K_CHORD ? 1.0f : 3.5f)) * v->idx;
    s = fm_sin_turns(v->pc + mod * 0.159155f);
    if (v->kind == K_CHORD) /* the tine fades with the index */
        s += v->tine * fm_sin_turns(v->pc * 14.0f) * v->idx;
    v->idx *= v->idec;
    v->nat *= v->dec;
    if (v->held && v->gate && --v->gate == 0)
        v->held = 0;
    tg = v->held ? 1.0f : 0.0f;
    v->g += (tg - v->g) * (v->held ? G_ATT : G_REL);
    if (!v->held && v->g < OFF_THR)
        v->on = 0;
    return s * v->amp * v->nat * v->g;
}

static float bass_sample(void)
{
    float s, tg;
    bs.p1 += bs.i1;
    if (bs.p1 >= 1.0f)
        bs.p1 -= 1.0f;
    bs.p2 += bs.i2;
    if (bs.p2 >= 1.0f)
        bs.p2 -= 1.0f;
    s = fm_sin_turns(bs.p1) + 0.6f * fm_sin_turns(bs.p2);
    if (bs.held && bs.gate && --bs.gate == 0)
        bs.held = 0;
    tg = bs.held ? 1.0f : 0.0f;
    bs.g += (tg - bs.g) * (bs.held ? G_ATT : G_REL);
    if (!bs.held && bs.g < OFF_THR)
        bs.on = 0;
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
                x += fm_sample(&fv[j]);
        if (bs.on)
            x += bass_sample();
        for (d = V_KICK; d < V_N; d++)
            if (dr[d].on)
                x += drum_sample(d);
        lp_y += MIX_LP * (x - lp_y);
        lp_y = fm_flush(lp_y);
        if (gain_n) {
            gain += gain_step;
            gain_n--;
        }
        g = gain;
        l[i] += lp_y * g;
        r[i] += lp_y * g;
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
    for (i = 0; i < NFM; i++)
        fv[i].held = 0;
    bs.held = 0;
    for (i = V_KICK; i < V_N; i++)
        if (dr[i].on)
            dr[i].dec = dr[i].dec < 0.996f ? dr[i].dec : 0.996f; /* under 40 ms to the threshold */
    qn = 0;
}

void vx_set_gain(float g)
{
    gain_step = (g - gain) * (1.0f / 882.0f);
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

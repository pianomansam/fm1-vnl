/* SPDX-License-Identifier: MPL-2.0 */
/* Drum kits (see drums.h). Each voice is a lane: a kick is a sine with a falling pitch and a falling level; a snare
   is band-passed noise plus an optional tone; the hats are high-passed noise; the shaker and the rim are band-passed
   noise. Per-kit parameters live in a table (times in seconds, converted once when the kit is set). A lane ends when
   its level is below OFF_THR, so the cost is nothing for a drum that is not sounding. */
#include <stdint.h>
#include <string.h>
#include "fastmath.h"
#include "library.h"
#include "drums.h"

#define SR 44100.0f
#define INV_SR (1.0f / 44100.0f)
#define T60 6.9078f /* ln 1000 */
#define OFF_THR 1e-3f
#define ND 6 /* V_KICK .. V_RIM */

/* one voice of one kit */
typedef struct {
    float amp;       /* level at velocity 127 */
    float t60;       /* the level's -60 dB time */
    float f0, f1;    /* kick: the sweep from f0 to f1 Hz ... */
    float ptau;      /* ... with this time constant (s); the snare's tone uses f0 as its pitch */
    float tamp, t60t; /* snare: tone level against the noise, and the tone's -60 dB time */
    float lp, hp;    /* noise: low-pass coefficient; band-pass low edge (a second one-pole); 0 hp: high-pass noise */
} dk_t;

/* rows: kick, snare, closed hat, open hat, shaker, rim */
static const dk_t kits[4][ND] = {
    { /* KIT_DUSTY: boom-bap, punchy but soft */
      { 0.40f, 0.70f, 150.0f, 50.0f, 0.030f, 0, 0, 0, 0 },
      { 0.22f, 0.35f, 185.0f, 0, 0, 0.55f, 0.12f, 0.40f, 0.03f },
      { 0.55f, 0.12f, 0, 0, 0, 0, 0, 0.70f, 0 },
      { 0.50f, 0.80f, 0, 0, 0, 0, 0, 0.70f, 0 },
      { 0.11f, 0.20f, 0, 0, 0, 0, 0, 0.45f, 0.02f },
      { 0.16f, 0.10f, 0, 0, 0, 0, 0, 0.40f, 0.12f } },
    { /* KIT_SOFT: gentle */
      { 0.34f, 0.50f, 110.0f, 55.0f, 0.020f, 0, 0, 0, 0 },
      { 0.16f, 0.30f, 170.0f, 0, 0, 0.35f, 0.10f, 0.30f, 0.04f },
      { 0.30f, 0.10f, 0, 0, 0, 0, 0, 0.35f, 0 },
      { 0.26f, 0.60f, 0, 0, 0, 0, 0, 0.35f, 0 },
      { 0.13f, 0.25f, 0, 0, 0, 0, 0, 0.35f, 0.02f },
      { 0.16f, 0.10f, 0, 0, 0, 0, 0, 0.30f, 0.10f } },
    { /* KIT_808: a long sub kick, crisp hats */
      { 0.50f, 2.20f, 130.0f, 40.0f, 0.080f, 0, 0, 0, 0 },
      { 0.22f, 0.40f, 200.0f, 0, 0, 0.40f, 0.14f, 0.50f, 0.03f },
      { 0.30f, 0.06f, 0, 0, 0, 0, 0, 0.60f, 0 },
      { 0.26f, 0.50f, 0, 0, 0, 0, 0, 0.60f, 0 },
      { 0.10f, 0.12f, 0, 0, 0, 0, 0, 0.60f, 0.04f },
      { 0.16f, 0.06f, 0, 0, 0, 0, 0, 0.55f, 0.18f } },
    { /* KIT_BRUSHES: a swishy snare with no body, a soft kick */
      { 0.30f, 0.35f, 100.0f, 52.0f, 0.020f, 0, 0, 0, 0 },
      { 0.16f, 0.45f, 0, 0, 0, 0, 0, 0.30f, 0.06f },
      { 0.30f, 0.15f, 0, 0, 0, 0, 0, 0.35f, 0 },
      { 0.26f, 0.70f, 0, 0, 0, 0, 0, 0.35f, 0 },
      { 0.10f, 0.30f, 0, 0, 0, 0, 0, 0.40f, 0.03f },
      { 0.20f, 0.08f, 0, 0, 0, 0, 0, 0.30f, 0.10f } },
};

/* the kit's decays as per-sample multipliers */
typedef struct { float dec, pdec, tdec; } dk_rt_t;

typedef struct {
    float env, dec, amp, lvl; /* level, its decay, the peak level, velocity / 127 */
    float a, b;               /* noise filter states */
    float ph, pe;             /* kick (and snare tone) phase in turns, kick pitch excess */
    float te;                 /* snare tone level */
    uint8_t on;
} lane_t;

static const dk_t *kit_p = kits[0];
static dk_rt_t rt[ND];
static lane_t ln[ND];
static uint32_t rng = 0x2545F491u;

static float noise(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (float)(int32_t)rng * (1.0f / 2147483648.0f);
}

static float dec_of(float t60) { return t60 > 0.0f ? fm_expf(-T60 / (t60 * SR)) : 0.0f; }

void drums_set_kit(int kit)
{
    int i;
    kit_p = kits[kit >= 0 && kit < 4 ? kit : 0];
    for (i = 0; i < ND; i++) {
        rt[i].dec = dec_of(kit_p[i].t60);
        rt[i].pdec = kit_p[i].ptau > 0.0f ? fm_expf(-INV_SR / kit_p[i].ptau) : 0.0f;
        rt[i].tdec = dec_of(kit_p[i].t60t);
    }
}

void drums_init(void)
{
    memset(ln, 0, sizeof ln);
    rng = 0x2545F491u;
    drums_set_kit(KIT_DUSTY);
}

void drums_on(const song_ev_t *e)
{
    int i = (int)e->voice - V_KICK;
    lane_t *d;
    const dk_t *p;
    if (i < 0 || i >= ND)
        return;
    d = &ln[i];
    p = &kit_p[i];
    d->lvl = (float)e->vel * (1.0f / 127.0f);
    d->amp = p->amp * d->lvl;
    d->env = 1.0f;
    d->dec = rt[i].dec;
    d->a = d->b = 0.0f;
    d->te = 1.0f;
    d->pe = 1.0f;
    d->ph = 0.25f; /* cosine start: the kick's click is the first sample */
    d->on = 1;
}

/* One sample of lane i. */
static float lane_sample(int i)
{
    lane_t *d = &ln[i];
    const dk_t *p = &kit_p[i];
    float n, s;
    if (i == 0) { /* kick */
        d->ph += (p->f1 + (p->f0 - p->f1) * d->pe) * INV_SR;
        if (d->ph >= 1.0f)
            d->ph -= 1.0f;
        d->pe = fm_flush(d->pe * rt[0].pdec);
        s = fm_sin_turns(d->ph);
    } else if (p->hp > 0.0f) { /* snare, shaker, rim: band-passed noise (one-pole low-pass minus its own low end) */
        n = noise();
        d->a += p->lp * (n - d->a);
        d->b += p->hp * (d->a - d->b);
        s = (d->a - d->b) * 1.4f;
        if (p->tamp > 0.0f) { /* the snare's tone, its own faster fall */
            d->ph += p->f0 * INV_SR;
            if (d->ph >= 1.0f)
                d->ph -= 1.0f;
            s += p->tamp * d->te * fm_sin_turns(d->ph);
            d->te = fm_flush(d->te * rt[i].tdec);
        }
    } else { /* hats: high-passed noise */
        n = noise();
        d->a += p->lp * (n - d->a);
        s = n - d->a;
    }
    s *= d->amp * d->env;
    d->env = fm_flush(d->env * d->dec);
    if (d->env < OFF_THR) {
        d->on = 0;
        d->env = 0.0f;
    }
    return s;
}

float drums_sample(void)
{
    float x = 0.0f;
    int i;
    for (i = 0; i < ND; i++)
        if (ln[i].on)
            x += lane_sample(i);
    return x;
}

void drums_release(void)
{
    int i;
    for (i = 0; i < ND; i++)
        if (ln[i].on)
            ln[i].dec = ln[i].dec < 0.996f ? ln[i].dec : 0.996f; /* under 40 ms to the threshold */
}

int drums_active(void)
{
    int i, c = 0;
    for (i = 0; i < ND; i++)
        c += ln[i].on;
    return c;
}

float drums_kick_env(void)
{
    return ln[0].on ? ln[0].env * ln[0].lvl : 0.0f;
}

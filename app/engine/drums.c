/* SPDX-License-Identifier: GPL-3.0-or-later */
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
    float amp;       /* level at velocity 127 (times the kit's KIT_LV) */
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
      { 0.45f, 0.12f, 0, 0, 0, 0, 0, 0.70f, 0 },
      { 0.31f, 0.80f, 0, 0, 0, 0, 0, 0.70f, 0 },
      { 0.11f, 0.20f, 0, 0, 0, 0, 0, 0.45f, 0.02f },
      { 0.25f, 0.10f, 0, 0, 0, 0, 0, 0.40f, 0.12f } },
    { /* KIT_SOFT: gentle */
      { 0.34f, 0.50f, 110.0f, 55.0f, 0.020f, 0, 0, 0, 0 },
      { 0.25f, 0.30f, 170.0f, 0, 0, 0.35f, 0.10f, 0.30f, 0.04f },
      { 0.18f, 0.10f, 0, 0, 0, 0, 0, 0.35f, 0 },
      { 0.125f, 0.60f, 0, 0, 0, 0, 0, 0.35f, 0 },
      { 0.08f, 0.25f, 0, 0, 0, 0, 0, 0.35f, 0.02f },
      { 0.21f, 0.10f, 0, 0, 0, 0, 0, 0.30f, 0.10f } },
    { /* KIT_808: a long sub kick, crisp hats */
      { 0.50f, 2.20f, 130.0f, 40.0f, 0.080f, 0, 0, 0, 0 },
      { 0.26f, 0.40f, 200.0f, 0, 0, 0.40f, 0.14f, 0.50f, 0.03f },
      { 0.52f, 0.06f, 0, 0, 0, 0, 0, 0.60f, 0 },
      { 0.31f, 0.50f, 0, 0, 0, 0, 0, 0.60f, 0 },
      { 0.135f, 0.12f, 0, 0, 0, 0, 0, 0.60f, 0.04f },
      { 0.33f, 0.06f, 0, 0, 0, 0, 0, 0.55f, 0.18f } },
    { /* KIT_BRUSHES: a swishy snare with no body, a soft kick */
      { 0.30f, 0.35f, 100.0f, 52.0f, 0.020f, 0, 0, 0, 0 },
      { 0.31f, 0.45f, 0, 0, 0, 0, 0, 0.30f, 0.06f },
      { 0.17f, 0.15f, 0, 0, 0, 0, 0, 0.35f, 0 },
      { 0.135f, 0.70f, 0, 0, 0, 0, 0, 0.35f, 0 },
      { 0.08f, 0.30f, 0, 0, 0, 0, 0, 0.40f, 0.03f },
      { 0.28f, 0.08f, 0, 0, 0, 0, 0, 0.30f, 0.10f } },
};

/* Each kit's level in the mix (Task 8: the drums under the keys and the lead, every kit alike; the 808's sub sits a
   little hotter): the rows above balance a kit's lanes against its snare (hats about 10 dB under it per hit after the
   chain's 5 kHz low-pass, the open hat 5, the shaker 10, the rim 8), this sets the kit. */
static const float KIT_LV[4] = {0.56f, 0.62f, 0.40f, 1.26f};
static float kit_lv = 0.56f;
static int sampled; /* the kit is a mixed sampled one (KIT_S_*): their hits went with the kits (2026-11-10), so new
                       hits are silent (a shipped kit's lanes, wb_drums.c, play over one) */

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
    sampled = lib_kit_sampled(kit);
    if (sampled) /* the synth lanes keep their kit (hits sounding finish on it) */
        return;
    kit = kit >= 0 && kit < 4 ? kit : 0;
    kit_p = kits[kit];
    kit_lv = KIT_LV[kit];
    for (i = 0; i < ND; i++) {
        rt[i].dec = dec_of(kit_p[i].t60);
        rt[i].pdec = kit_p[i].ptau > 0.0f ? fm_expf(-INV_SR / kit_p[i].ptau) : 0.0f;
        rt[i].tdec = dec_of(kit_p[i].t60t);
    }
}

void drums_song(uint32_t seed)
{
    memset(ln, 0, sizeof ln);
    rng = seed * 2654435761u ^ 0x2545F491u;
    if (!rng)
        rng = 0x2545F491u;
}

void drums_init(void)
{
    drums_song(0u);
    drums_set_kit(KIT_DUSTY);
}

static const drums_hook_t *hook;

void drums_hook(const drums_hook_t *h) { hook = h; }

void drums_on(const song_ev_t *e)
{
    if (hook) {
        hook->on(e);
        return;
    }
    int i = (int)e->voice - V_KICK;
    lane_t *d;
    const dk_t *p;
    if (sampled || i < 0 || i >= ND)
        return;
    d = &ln[i];
    p = &kit_p[i];
    d->lvl = (float)e->vel * (1.0f / 127.0f);
    d->amp = p->amp * kit_lv * d->lvl;
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

void drums_render(float *x, uint32_t n)
{
    if (hook) {
        hook->render(x, n);
        return;
    }
    uint32_t s;
    int i, any = 0;
    for (i = 0; i < ND; i++)
        any |= ln[i].on;
    if (any) /* the synth lanes a sample at a time (their noise is one stream, drawn in lane order) */
        for (s = 0; s < n; s++) {
            float y = 0.0f;
            for (i = 0; i < ND; i++)
                if (ln[i].on)
                    y += lane_sample(i);
            x[s] = y;
        }
    else
        memset(x, 0, n * sizeof(float));
}

float drums_sample(void)
{
    float x;
    drums_render(&x, 1u);
    return x;
}

void drums_release(void)
{
    if (hook) {
        hook->release();
        return;
    }
    int i;
    for (i = 0; i < ND; i++)
        if (ln[i].on)
            ln[i].dec = ln[i].dec < 0.996f ? ln[i].dec : 0.996f; /* under 40 ms to the threshold */
}

int drums_active(void)
{
    if (hook)
        return hook->active();
    int i, c = 0;
    for (i = 0; i < ND; i++)
        c += ln[i].on;
    return c;
}

float drums_kick_env(void)
{
    if (hook)
        return hook->kick_env();
    return ln[0].on ? ln[0].env * ln[0].lvl : 0.0f;
}

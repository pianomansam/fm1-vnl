/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The string synth (M_STR): the instrument spike's string section (2a, branch spike/instruments: st_*) as a mono
   lead. Three polyBLEP saws a note (-9, 0 and +7 cents), each drifting up to 6 cents, with a delayed vibrato (5.4 Hz,
   9 cents, after 0.35 s, faded in over 0.6 s); a one-pole high-pass, then a low-pass and two band-pass formants (TPT
   state-variable filters) mixed; then the Ensemble (adapted from Mutable Instruments Plaits, MIT, Emilie Gillet: one
   line read at three taps swayed by a slow and a fast LFO, 120 degrees apart), mono. Pitch and the Ensemble's LFOs
   move at a control rate (every 16 samples of 44.1 kHz). A legato note keeps the phases, the filters and the line and
   glides to its pitch. p[] as patch.c documents. Float only; the Ensemble's line is the caller's (STR_LINE floats a
   voice); ISR-safe; state per voice about 200 B. */
#ifndef ENGINE_STRSYN_H
#define ENGINE_STRSYN_H
#include <stdint.h>
#include "patch.h"
#include "fastmath.h"

#define STR_LINE 512u /* the Ensemble's line in samples (a power of two): its longest tap at the full rate is 273 */
#define STR_CR 16u    /* the control rate, in samples at 44.1 kHz */

typedef struct {
    float a1, a2, a3, ic1, ic2; /* a TPT state-variable filter (the spike's svf_t): coefficients, states */
} str_svf_t;

typedef struct {
    float ph[3], dt[3];           /* the saws' phases and increments (cycles per voice sample) */
    float f, ft, gk;              /* the note's increment, gliding by gk a control step to ft */
    float drift[3], dtgt[3];      /* each saw's drift (cents) easing to its target */
    float vph, vt;                /* the vibrato's phase (turns), seconds since the onset */
    float hpz, hpk;               /* the high-pass's one-pole: state, coefficient */
    str_svf_t lp, b1, b2;         /* the low-pass and the two formants */
    float mlo, mb1, mb2, amount;  /* the mix (p[6..8]) and the Ensemble's amount (p[9]) */
    float *line;                  /* the Ensemble's line (STR_LINE floats) */
    float ep1, ep2, ed[3];        /* its LFOs' phases (turns) and the taps' delays (voice samples) */
    float dv;                     /* 44.1 kHz samples a voice sample: 1 at the full rate, 2 at half */
    uint32_t w, rng, gn;          /* the line's write index (masked); the drift's noise; glide steps left */
    uint8_t cc, cn, dc;           /* voice samples to the next control step; voice samples a step; steps to the
                                     next drift target */
} str_voice_t;

/* A note on: fresh (legato 0: phases, filters, the line cleared) or legato (the voice's state kept, gliding to note
   over p[10] ms). dv 1 or 2 (half rate); line the voice's STR_LINE floats; seed the drift's noise (fresh notes). */
void str_start(str_voice_t *v, const vx_patch_t *p, float note, float dv, int legato, float *line, uint32_t seed);
/* the control step: the glide, the vibrato and drift into the saws' increments, the Ensemble's taps */
void str_ctl(str_voice_t *v);

static inline float str_saw(float *ph, float dt)
{
    float t = *ph + dt, y;
    if (t >= 1.0f)
        t -= 1.0f;
    *ph = t;
    y = 2.0f * t - 1.0f;
    if (t < dt) {
        t /= dt;
        y -= t + t - t * t - 1.0f;
    } else if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        y -= t * t + t + t + 1.0f;
    }
    return y;
}

/* the filter's step: the low-pass out, *bp the band-pass */
static inline float str_svf(str_svf_t *s, float x, float *bp)
{
    float v3 = x - s->ic2, v1 = s->a1 * s->ic1 + s->a2 * v3, v2 = s->ic2 + s->a2 * s->ic1 + s->a3 * v3;
    s->ic1 = 2.0f * v1 - s->ic1;
    s->ic2 = 2.0f * v2 - s->ic2;
    *bp = v1;
    return v2;
}

/* one sample at the voice's rate (unit level, before the envelope) */
static inline float str_sample(str_voice_t *v)
{
    float s, y, lo, bp1, bp2, wet = 0.0f, *ln = v->line;
    uint32_t w = v->w;
    int k;
    if (v->cc == 0) {
        str_ctl(v);
        v->cc = v->cn;
    }
    v->cc--;
    s = str_saw(&v->ph[0], v->dt[0]) + str_saw(&v->ph[1], v->dt[1]) + str_saw(&v->ph[2], v->dt[2]);
    v->hpz += (s - v->hpz) * v->hpk;
    y = s - v->hpz;
    lo = str_svf(&v->lp, y, &bp1);
    str_svf(&v->b1, y, &bp1);
    str_svf(&v->b2, y, &bp2);
    y = (v->mlo * lo + v->mb1 * bp1 + v->mb2 * bp2) * 0.25f;
    ln[w] = y;
    for (k = 0; k < 3; k++) { /* (the read point kept positive: a truncation is its floor) */
        float rp = (float)(w + STR_LINE) - v->ed[k], y0, y1, fr;
        uint32_t i = (uint32_t)rp;
        fr = rp - (float)i;
        y0 = ln[i & (STR_LINE - 1u)];
        y1 = ln[(i + 1u) & (STR_LINE - 1u)];
        wet += y0 + (y1 - y0) * fr;
    }
    v->w = (w + 1u) & (STR_LINE - 1u);
    return y * (1.0f - 0.5f * v->amount) + wet * (1.0f / 3.0f) * v->amount;
}

#endif

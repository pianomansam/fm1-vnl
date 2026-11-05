/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The modeled vibraphone (M_MODAL; the vibes lead, in place of the VCSL sample from 2026-11-03, Sam's A/B).
   A struck aluminum bar as three modes, each a decaying sine at an exact ratio (the bar's 1 : 4 : ~10 tuning), set
   ringing at once by the strike (each a two-pole recursion started at phase 0: no click); a soft mallet, a low-pass on
   the strike's spectrum (its contact time), so the upper modes speak less on a soft or a high note; and the resonator
   tube, which carries the fundamental and builds up over tens of ms after the strike (the bloom). No motor (Sam could
   not hear it: the vibes play still). The approach and values after Ravel_FM1's TINE marimba (a bank of decaying
   modes, a mallet whose hardness sets the strike's length; GPL-3.0, approach and values only, no code).
   p[] as patch.c documents. Float only, no tables; ISR-safe; state per voice 76 B. */
#ifndef ENGINE_MODAL_H
#define ENGINE_MODAL_H
#include <stdint.h>
#include "patch.h"
#include "fastmath.h"

#define MD_N 3     /* the modes */
#define MD_CR 16u  /* the control rate (samples): the fundamental's bloom */

typedef struct {
    float c[MD_N], r2[MD_N], s1[MD_N], s2[MD_N]; /* per mode: 2 r cos w, r^2, the last two outputs */
    float g1, g1t, bl, blk;    /* the fundamental's gain now and its step; the bloom (1 - the tube's share now), its
                                  fall per control step */
    float a0, tube;            /* the bar's share of the fundamental at the strike; the tube's level (1 - a0) */
    float tph, tinc, td;       /* the motor's tremolo (the vibes on the keys): phase (turns), its step per control step,
                                  depth (0: still) */
    uint32_t cc;
} md_voice_t;

/* A note on: note (MIDI), vel 0-1. */
void md_start(md_voice_t *v, const vx_patch_t *p, float note, float vel);

/* The motor on a note just started: its discs over the tube, an amplitude tremolo of depth on the tube's share of
   the fundamental (the bar's own modes unmodulated) at rate Hz, ph the motor's phase now (turns: one motor for every
   note). The vibes on the keys only (Sam, 2026-11-05); the lead's vibes stay still. */
static inline void md_motor(md_voice_t *v, float depth, float rate, float ph)
{
    v->td = fm_clampf(depth, 0.0f, 1.0f);
    v->tinc = rate * (float)MD_CR * (1.0f / 44100.0f);
    v->tph = ph - (float)(int)ph;
}

static inline float md_sample(md_voice_t *v)
{
    float y, s = 0.0f;
    int k;
    if (++v->cc >= MD_CR) { /* the bloom, at the control rate */
        v->cc = 0;
        v->bl = v->bl > 1e-6f ? v->bl * v->blk : 0.0f; /* (done: 0, never denormal) */
        if (v->td > 0.0f) { /* the motor */
            float tr = 1.0f - v->td * (0.5f - 0.5f * fm_sin_turns(v->tph));
            v->tph += v->tinc;
            if (v->tph >= 1.0f)
                v->tph -= 1.0f;
            v->g1t = (v->a0 + v->tube * (1.0f - v->bl) * tr - v->g1) * (1.0f / (float)MD_CR);
        } else
            v->g1t = (v->a0 + v->tube * (1.0f - v->bl) - v->g1) * (1.0f / (float)MD_CR); /* a ramp to it */
        for (k = 0; k < MD_N; k++) /* a finished mode's state to 0 before it goes denormal */
            if (fm_fabsf(v->s1[k]) + fm_fabsf(v->s2[k]) < 1e-12f)
                v->s1[k] = v->s2[k] = 0.0f;
    }
    v->g1 += v->g1t;
    for (k = 0; k < MD_N; k++) {
        y = v->c[k] * v->s1[k] - v->r2[k] * v->s2[k];
        v->s2[k] = v->s1[k];
        v->s1[k] = y;
        s += k ? y : y * v->g1;
    }
    return s;
}

#endif

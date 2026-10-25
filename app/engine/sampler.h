/* SPDX-License-Identifier: MPL-2.0 */
/* The sampler voice (M_SAMP): every sampled pitched instrument (the Knight, the nylon, the Emilyguitar) on one model.
   A note plays its bank's nearest zone (samples.h) retuned by the step: ADPCM decoded as it goes, 4-point Hermite
   between the decoded samples, a one-pole tone (velocity darkens it), its own envelope (a linear attack, a decay to
   the sustain and a release, the patch's times to -60 dB) and an optional delayed vibrato. p[] as patch.c documents.
   Float only, no tables of its own but the IMA ADPCM steps; ISR-safe; state per voice about 120 B. */
#ifndef ENGINE_SAMPLER_H
#define ENGINE_SAMPLER_H
#include <stdint.h>
#include "patch.h"
#include "samples.h"

/* An ADPCM stream over one zone (samples.h's format): the next sample's index, the predictor and step index, and
   those at the loop's start (restored at each wrap). */
typedef struct {
    const smp_zone_t *z;
    uint32_t ni;
    int32_t pred, idx, lpred, lidx;
} smp_dec_t;

void smp_dec_init(smp_dec_t *d, const smp_zone_t *z);
/* The zone's next sample, -32768..32767: a looped zone wraps forever; a one-shot gives 0 once past its end (ni then
   stays at len). */
int smp_dec_next(smp_dec_t *d);

typedef struct {
    smp_dec_t d;
    float h[4], fr, step;         /* the last four decoded samples (h[3] newest), the read fraction, the step */
    float gain;                   /* the zone's gain over 32768 */
    float lp, lc, lce, lcf;       /* the tone one-pole's state and coefficient (lc 1: none), the coefficient it falls
                                     to and its fall's factor per TONE_CR samples */
    float env, ainc, dm, sus, rm; /* the envelope: the attack's rise per sample, the decay's and release's factors */
    float vt, vdt, vamt, vph;     /* the vibrato: seconds since the onset, seconds a sample, its depth, phase (turns) */
    float dv;                     /* output samples a call: 1 at the full rate, 2 at half (set by the caller) */
    uint32_t tc;                  /* samples since the tone last moved */
    uint8_t stage, on, ended;     /* 0 attack, 1 decay, 2 release; sounding; the decoder's samples after a one-shot */
} smp_voice_t;

/* The zone a note plays: the bank's nearest by root (a tie goes to the higher zone, played down). */
const smp_zone_t *samp_zone(const smp_bank_t *b, float note);
/* A note on (re)starts the voice; v->dv is read (1 or 2; anything else is 1) and kept. vel 0-1. */
void samp_start(smp_voice_t *v, const vx_patch_t *p, float note, float vel);
float samp_sample(smp_voice_t *v); /* one sample at the voice's rate; 0 when done (v->on 0) */
void samp_release(smp_voice_t *v);

#endif

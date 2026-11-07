/* SPDX-License-Identifier: GPL-3.0-or-later */
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
#include "fastmath.h"

/* An ADPCM stream over one zone (samples.h's format): the next sample's index, the predictor and step index, and
   those at the loop's start (restored at each wrap). */
typedef struct {
    const smp_zone_t *z;
    const uint8_t *p;             /* the zone's bytes */
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
                                     to and its fall's factor per SMP_TONE_CR samples */
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
void samp_release(smp_voice_t *v);

/* The per-sample path, inline (its caller renders a voice a sample at a time: a call per sample cost the Knight's keys
   about a tenth of their render). */
#define SMP_ENV_OFF 1e-4f    /* the envelope's floor (-80 dB): the voice is done under it */
#define SMP_TONE_CR 32u      /* the tone's fall: its coefficient moves every SMP_TONE_CR samples */
#define SMP_VIB_RATE 5.0f    /* the vibrato as the winds': 5 Hz, waiting 0.3 s, then fading in over 0.6 s */
#define SMP_VIB_DELAY 0.3f
#define SMP_VIB_FADE 0.6f
extern const int16_t SMP_STEP[89]; /* the IMA ADPCM steps and index moves */
extern const int8_t SMP_IDXT[8];

static inline int smp_dec_fast(smp_dec_t *d)
{
    const smp_zone_t *z = d->z;
    int code, step, diff;
    if (z->loop_len) {
        if (d->ni >= z->loop_start + z->loop_len) { /* the wrap: the loop's start state again */
            d->ni = z->loop_start;
            d->pred = d->lpred;
            d->idx = d->lidx;
        } else if (d->ni == z->loop_start) { /* (the same state at every pass) */
            d->lpred = d->pred;
            d->lidx = d->idx;
        }
    } else if (d->ni >= z->len)
        return 0;
    code = (d->p[d->ni >> 1] >> ((d->ni & 1u) * 4u)) & 15;
    step = SMP_STEP[d->idx];
    diff = step >> 3;
    if (code & 4)
        diff += step;
    if (code & 2)
        diff += step >> 1;
    if (code & 1)
        diff += step >> 2;
    d->pred += code & 8 ? -diff : diff;
    d->pred = d->pred < -32768 ? -32768 : d->pred > 32767 ? 32767 : d->pred;
    d->idx += SMP_IDXT[code & 7];
    d->idx = d->idx < 0 ? 0 : d->idx > 88 ? 88 : d->idx;
    d->ni++;
    return (int)d->pred;
}

/* one sample at the voice's rate; 0 when done (v->on 0) */
static inline float samp_sample(smp_voice_t *v)
{
    float f, s = v->step, x, a, b, c, d;
    if (!v->on)
        return 0.0f;
    if (v->vamt > 0.0f) { /* the delayed vibrato: the step bent by up to p[4] cents */
        float dep = fm_clampf((v->vt - SMP_VIB_DELAY) * (1.0f / SMP_VIB_FADE), 0.0f, 1.0f) * v->vamt;
        if (dep > 0.0f) {
            s *= 1.0f + dep * fm_sin_turns(v->vph);
            v->vph += SMP_VIB_RATE * v->vdt;
            if (v->vph >= 1.0f)
                v->vph -= 1.0f;
        }
        v->vt += v->vdt;
    }
    f = v->fr + s;
    while (f >= 1.0f) {
        f -= 1.0f;
        v->h[0] = v->h[1];
        v->h[1] = v->h[2];
        v->h[2] = v->h[3];
        v->h[3] = (float)smp_dec_fast(&v->d);
        if (!v->d.z->loop_len && v->d.ni >= v->d.z->len && ++v->ended > 4u) { /* a one-shot played out */
            v->on = 0;
            return 0.0f;
        }
    }
    v->fr = f;
    a = v->h[0];
    b = v->h[1];
    c = v->h[2];
    d = v->h[3];
    {
        float c1 = 0.5f * (c - a), c2 = a - 2.5f * b + 2.0f * c - 0.5f * d, c3 = 0.5f * (d - a) + 1.5f * (b - c);
        x = (((c3 * f + c2) * f + c1) * f + b) * v->gain;
    }
    if (v->lc < 1.0f) {
        x = v->lp = fm_flush(v->lp + (x - v->lp) * v->lc);
        if (v->lcf > 0.0f && ++v->tc >= SMP_TONE_CR) {
            v->tc = 0;
            v->lc += (v->lce - v->lc) * v->lcf;
        }
    }
    switch (v->stage) {
    case 0:
        v->env += v->ainc;
        if (v->env >= 1.0f) {
            v->env = 1.0f;
            v->stage = 1;
        }
        break;
    case 1:
        v->env = v->sus + (v->env - v->sus) * v->dm;
        if (v->sus <= 0.0f && v->env < SMP_ENV_OFF)
            v->on = 0;
        break;
    default:
        v->env *= v->rm;
        if (v->env < SMP_ENV_OFF)
            v->on = 0;
    }
    return v->on ? x * v->env : 0.0f;
}

#endif

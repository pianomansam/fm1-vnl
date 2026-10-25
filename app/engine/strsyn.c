/* SPDX-License-Identifier: MPL-2.0 */
/* The string synth (see strsyn.h), from the instrument spike's st_* (branch spike/instruments, its 2a and 2c demos).
   The Ensemble adapted from Mutable Instruments Plaits, fx/ensemble.h, MIT, Copyright 2014 Emilie Gillet (CREDITS.md):
   mono in and out, the delays scaled from 48 to 44.1 kHz (and to the voice's rate). */
#include <string.h>
#include "fastmath.h"
#include "strsyn.h"

#define SR 44100.0f
#define CENT 0.000578f              /* a cent's step, as the spike's */
#define LP_Q 0.6f                   /* the low-pass's Q */
#define VIB_RATE 5.4f               /* the vibrato: 5.4 Hz, 9 cents, after 0.35 s, faded in over 0.6 s */
#define VIB_CENTS 9.0f
#define VIB_DELAY 0.35f
#define VIB_FADE 0.6f
/* each saw's drift: a new target within +-6 cents every DRIFT_STEPS steps (the spike's noise ran -1..3, so its
   targets spanned -3..9 cents: the same 12-cent spread here, centered, without its 3 cents sharp) */
#define DRIFT_CENTS 6.0f
#define DRIFT_STEPS 64u
#define DRIFT_K 0.0032f             /* its ease a step */
#define ENS_DEPTH 0.6f              /* the Ensemble: depth as the spike's; the slow and fast LFOs in Hz */
#define ENS_SLOW 0.75f
#define ENS_FAST 6.57f
#define ENS_BASE 176.0f             /* its taps' delay at 44.1 kHz: 176 samples, swayed by 147 and 14.7 times depth */
#define ENS_A 147.0f
#define ENS_B 14.7f

static const float DET[3] = {-9.0f, 0.0f, 7.0f};

static void svf_set(str_svf_t *s, float hz, float q, float sr)
{
    float g = fm_tanf(FM_PI * fm_clampf(hz, 10.0f, 0.45f * sr) / sr), k = 1.0f / q;
    s->a1 = 1.0f / (1.0f + g * (g + k));
    s->a2 = g * s->a1;
    s->a3 = g * s->a2;
    s->ic1 = s->ic2 = 0.0f;
}

static float frand(uint32_t *s) /* -1..1 */
{
    *s = *s * 1664525u + 1013904223u;
    return (float)(*s >> 8) * (1.0f / 8388608.0f) - 1.0f;
}

void str_start(str_voice_t *v, const vx_patch_t *p, float note, float dv, int legato, float *line, uint32_t seed)
{
    float sr, f;
    int k;
    dv = dv == 2.0f ? 2.0f : 1.0f;
    f = 440.0f * fm_exp2f((note - 69.0f) * (1.0f / 12.0f)) * dv / SR;
    if (legato && v->line == line && v->dv == dv) { /* the glide over p[10] ms, evenly in pitch */
        float n = p->p[10] * 0.001f * SR / (float)STR_CR;
        v->ft = f;
        if (n >= 1.0f && f != v->f) {
            v->gn = (uint32_t)n;
            v->gk = fm_exp2f(fm_log2f(f / v->f) / (float)v->gn);
        } else {
            v->gn = 0u;
            v->f = f;
        }
        return;
    }
    memset(v, 0, sizeof *v);
    sr = SR / dv;
    v->dv = dv;
    v->f = v->ft = f;
    for (k = 0; k < 3; k++) {
        v->ph[k] = 0.33f * (float)k;
        v->dt[k] = f;
    }
    v->hpk = FM_TWO_PI * p->p[0] / sr;
    svf_set(&v->lp, p->p[1], LP_Q, sr);
    svf_set(&v->b1, p->p[2], p->p[3], sr);
    svf_set(&v->b2, p->p[4], p->p[5], sr);
    v->mlo = p->p[6];
    v->mb1 = p->p[7];
    v->mb2 = p->p[8];
    v->amount = fm_clampf(p->p[9], 0.0f, 1.0f);
    v->line = line;
    memset(line, 0, STR_LINE * sizeof line[0]);
    v->rng = seed | 1u;
    v->cn = (uint8_t)(STR_CR / (uint32_t)dv);
}

void str_ctl(str_voice_t *v)
{
    float vib, a = ENS_DEPTH * ENS_A, b = ENS_DEPTH * ENS_B, idv = 1.0f / v->dv;
    int k;
    if (v->gn) /* the glide: even steps in pitch, landing exactly */
        v->f = --v->gn ? v->f * v->gk : v->ft;
    v->vt += (float)STR_CR / SR;
    v->vph += (float)STR_CR * VIB_RATE / SR;
    if (v->vph >= 1.0f)
        v->vph -= 1.0f;
    vib = fm_clampf((v->vt - VIB_DELAY) * (1.0f / VIB_FADE), 0.0f, 1.0f) * VIB_CENTS * fm_sin_turns(v->vph);
    if (v->dc == 0) {
        for (k = 0; k < 3; k++)
            v->dtgt[k] = frand(&v->rng) * DRIFT_CENTS;
        v->dc = DRIFT_STEPS;
    }
    v->dc--;
    for (k = 0; k < 3; k++) {
        v->drift[k] += (v->dtgt[k] - v->drift[k]) * DRIFT_K;
        v->dt[k] = v->f * (1.0f + CENT * (DET[k] + vib + v->drift[k]));
    }
    v->ep1 += (float)STR_CR * ENS_SLOW / SR;
    v->ep2 += (float)STR_CR * ENS_FAST / SR;
    v->ep1 -= (float)(int)v->ep1;
    v->ep2 -= (float)(int)v->ep2;
    for (k = 0; k < 3; k++) {
        float o = (float)k * (1.0f / 3.0f);
        v->ed[k] = (ENS_BASE + a * fm_sin_turns(v->ep1 + o) + b * fm_sin_turns(v->ep2 + o)) * idv;
    }
    /* (the filters' states held clear of denormals at the control rate) */
    v->hpz = fm_flush(v->hpz);
    v->lp.ic1 = fm_flush(v->lp.ic1);
    v->lp.ic2 = fm_flush(v->lp.ic2);
    v->b1.ic1 = fm_flush(v->b1.ic1);
    v->b1.ic2 = fm_flush(v->b1.ic2);
    v->b2.ic1 = fm_flush(v->b2.ic1);
    v->b2.ic2 = fm_flush(v->b2.ic2);
}

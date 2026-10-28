/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Copyright (C) 2026 the Optimist contributors */
/* What drum_synth.c (Optimist 915fd8a, w0ts/optimist) takes from the rest of Optimist's firmware, copied for VNL-1's
 * workbench: the build switches and macros (core/core.h, system/cpuguard.h at CG_LEVEL 0), the integer DSP primitives
 * it calls (dsp/dsp_common.h, dsp/dsp.c; sine_i is its C form, sine_i_c), pow2_q16 (engines/sample/eng_sample.c) and
 * rng (system/libc.c), with the generated tables (felucca_tables.h). Bodies as in those files. */
#ifndef WB_OPT_DS_DEPS_H
#define WB_OPT_DS_DEPS_H
#include <stdint.h>
#include <string.h>
#include "felucca_tables.h"

#define FS 44100
#define CTL 32
#define CTL_LOG2 5
#define AINL static inline __attribute__((always_inline))
#define HOT
#define FELUCCA_DS_KAWA 0
#define FELUCCA_DS_PLAITS 0
#define CG_DTAIL 8                       /* (cpuguard.h: CG_LEVEL 0) */

/* dsp_common.h */
AINL uint32_t xorshift32(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}
AINL uint32_t lfsr15(uint32_t l) { return (l >> 1) | (((l ^ (l >> 1)) & 1u) << 14); }
AINL int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

/* dsp.c */
AINL int32_t mulq15(int32_t a, int32_t b) { return (a * b) >> 15; }
AINL int32_t sine_i(uint32_t ph)
{
    uint32_t i = ph >> 22;
    int32_t a = SINE[i], b = SINE[(i + 1u) & 1023u];
    return a + (((b - a) * (int32_t)((ph >> 7) & 0x7FFFu)) >> 15);
}
AINL int32_t osc_tri(uint32_t ph)
{
    int32_t s = (int32_t)(ph >> 15);
    return s < 65536 ? s - 32768 : 98303 - s;
}
AINL int32_t softclip(int32_t x)
{
    int32_t a = x < 0 ? -x : x, i = a >> 8, y;
    if (i >= 256)
        y = TANH_Q15[256];
    else
        y = TANH_Q15[i] + (((TANH_Q15[i + 1] - TANH_Q15[i]) * (a & 255)) >> 8);
    return x < 0 ? -y : y;
}
AINL uint32_t noise32(int32_t *st) { return xorshift32((uint32_t *)st); }
typedef struct { int32_t a1, a2, a3; } tsvf_t;
AINL int32_t tsvf_tick(const tsvf_t *c, int32_t in, int32_t *ic1, int32_t *ic2, int32_t *bp)
{
    int32_t v3 = in - *ic2;
    int32_t v1 = (c->a1 * *ic1 + c->a2 * v3) >> 13;
    int32_t v2 = *ic2 + ((c->a2 * *ic1 + c->a3 * v3) >> 13);
    *ic1 = clamp(2 * v1 - *ic1, -150000, 150000);
    *ic2 = clamp(2 * v2 - *ic2, -150000, 150000);
    *bp = v1;
    return v2;
}
AINL int32_t decay_q16(int32_t x, uint32_t k) { return (int32_t)(((uint32_t)x * k) >> 16); }

/* eng_sample.c */
AINL uint32_t pow2_q16(int32_t d16)
{
    uint32_t u = (uint32_t)(d16 + 192 * 16), oct = u / 192u;
    uint32_t r = pitch_inc(1600 + u % 192u) / (pitch_inc(1600) >> 16);
    return oct >= 16u ? r << (oct - 16u) : r >> (16u - oct);
}

/* libc.c (its state here; wb_opt.c reseeds it) */
static uint32_t rng_state = 0x1234567u;
AINL uint32_t rng(void) { return xorshift32(&rng_state); }

#endif

/* SPDX-License-Identifier: MIT */
/* Adapted from Mutable Instruments Rings, MIT, Emilie Gillet: rings/dsp/fx/reverb.h, chorus.h and fx_engine.h.

   Copyright 2014 Emilie Gillet.

   Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
   documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
   rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all copies or substantial portions of
   the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
   COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
/* Rings' reverb (the Griesinger topology from Dattorro's paper: four allpass diffusers, then two loops of two
   allpasses and a delay, crossed, the long delays read with a slow LFO) and its chorus (one line, two taps a side in
   quadrature on two slow LFOs), as C99 for VNL-1. As Rings: 16-bit delay memory (stored as int16, scaled by 32768,
   truncated and clipped), all lines in one ring whose write position falls one a step. Changed for VNL-1: the delay
   lengths and LFO rates rescaled from Rings' 48 kHz to the rate each runs at here (the reverb at our plate's 11.025
   kHz, x 0.2297; the chorus at our chorus's 7.35 kHz, x 0.1531); the LFOs as rotating phasors, every step; the
   reverb's mono send in and its two loops out (no dry mix), its settings fixed (below); the chorus's mono input and
   wet-only output. */
#include <stdint.h>
#include <string.h>
#include "fastmath.h"
#include "rings_fx.h"
#if RFX_ON

#if defined PLAT_HOST || defined __APPLE__
#define RFX_POOL
#else
#define RFX_POOL __attribute__((section(".pool")))
#endif

#define FS_RV 11025.0f /* the reverb's rate here */
#define FS_CH 7350.0f  /* the chorus's */

/* ------------------------------------------------------------------------------------- 16-bit memory --- */
static inline int16_t q16(float x)
{
    return (int16_t)(int32_t)(fm_clampf(x, -1.0f, 32767.0f / 32768.0f) * 32768.0f);
}
static inline float f16(int16_t v) { return (float)v * (1.0f / 32768.0f); }

/* -------------------------------------------------------------------------------------------- reverb --- */
/* Rings' lengths at 48 kHz (150, 214, 319, 527, 2182, 2690, 4501, 2525, 2197, 6312) x 11025 / 48000 */
enum { AP1, AP2, AP3, AP4, DAP1A, DAP1B, DEL1, DAP2A, DAP2B, DEL2, RV_NL };
static const int RV_LEN[RV_NL] = {34, 49, 73, 121, 501, 618, 1034, 580, 505, 1450};
#define RV_N 8192u                 /* a power of two above the lines' sum (4975) */
#define RV_DEL2_AT (6261.0f * FS_RV / 48000.0f) /* loop 1 reads del2 here, swinging RV_DEL2_MOD further */
#define RV_DEL2_MOD (50.0f * FS_RV / 48000.0f)
#define RV_DEL1_AT (4460.0f * FS_RV / 48000.0f)
#define RV_DEL1_MOD (40.0f * FS_RV / 48000.0f)
#define RV_LFO1 (FM_TWO_PI * 0.5f / FS_RV) /* LFO 1 (0.5 Hz) on loop 2's read, LFO 2 (0.3 Hz) on loop 1's */
#define RV_LFO2 (FM_TWO_PI * 0.3f / FS_RV)
/* the settings (Rings' Part sets them per patch; here fixed, for the plate's slot): the diffusion Rings uses, its
   FX_REVERB damping (0.3 at 48 kHz: about 2.7 kHz) at this rate, a reverb time that matches the plate's -60 dB time
   (1.7 s; in Rings' FX_REVERB_2 range, 0.3-0.9), an input gain that puts our small sends (about 0.05) where Rings'
   0.2 puts its full-scale input in the 16-bit memory, and the return's level matching the plate's tail energy */
#define RV_KAP 0.625f
#define RV_KLP 0.788f
#define RV_KRT 0.485f
#define RV_GAIN 4.0f
#define RV_OUT 0.1375f
static int16_t rv_buf[RV_N] RFX_POOL;
static uint32_t rv_base[RV_NL], rv_w;
static float rv_lp1, rv_lp2, l1s, l1c = 1.0f, l2s, l2c = 1.0f;
static uint8_t rv_ready;

static void rv_bases(void)
{
    uint32_t b = 0;
    int k;
    for (k = 0; k < RV_NL; k++) {
        rv_base[k] = b;
        b += (uint32_t)RV_LEN[k] + 1u;
    }
    rv_ready = 1;
}

/* a line's sample `at` steps old (its tail: its length less one) */
static inline float rv_rd(int k, uint32_t at) { return f16(rv_buf[(rv_w + rv_base[k] + at) & (RV_N - 1u)]); }
static inline void rv_wr(int k, float x) { rv_buf[(rv_w + rv_base[k]) & (RV_N - 1u)] = q16(x); }
static inline float rv_interp(int k, float at)
{
    uint32_t i = (uint32_t)at;
    float f = at - (float)i, a = rv_rd(k, i);
    return a + (rv_rd(k, i + 1u) - a) * f;
}
/* Rings' Read(tail, g) then WriteAllPass(-g): acc += g z; write acc; acc = -g acc + z */
static inline float rv_ap(int k, float acc, float g)
{
    float z = rv_rd(k, (uint32_t)RV_LEN[k] - 1u);
    acc += g * z;
    rv_wr(k, acc);
    return -g * acc + z;
}

void rfx_reverb_clear(void)
{
    memset(rv_buf, 0, sizeof rv_buf);
    rv_w = 0;
    rv_lp1 = rv_lp2 = 0.0f;
    if (!rv_ready)
        rv_bases();
    l1s = l2s = 0.0f;
    l1c = l2c = 1.0f;
}

void rfx_reverb(float x, float *l, float *r)
{
    float acc, apout, u1, u2;
    if (!rv_ready)
        rv_bases();
    rv_w = (rv_w - 1u) & (RV_N - 1u);
    { /* the LFOs, unipolar as Rings' (0.5 + 0.5 cos) */
        float s = l1s + RV_LFO1 * l1c, c = l1c - RV_LFO1 * s;
        l1s = s;
        l1c = c;
        s = l2s + RV_LFO2 * l2c;
        c = l2c - RV_LFO2 * s;
        l2s = s;
        l2c = c;
    }
    u1 = 0.5f + 0.5f * l1c;
    u2 = 0.5f + 0.5f * l2c;
    acc = x * RV_GAIN; /* diffused through four allpasses */
    acc = rv_ap(AP1, acc, RV_KAP);
    acc = rv_ap(AP2, acc, RV_KAP);
    acc = rv_ap(AP3, acc, RV_KAP);
    acc = rv_ap(AP4, acc, RV_KAP);
    apout = acc;
    /* loop 1: del2 back in, damped, two allpasses, into del1; its output the left */
    acc = apout + RV_KRT * rv_interp(DEL2, RV_DEL2_AT + RV_DEL2_MOD * u2);
    rv_lp1 = fm_flush(rv_lp1 + RV_KLP * (acc - rv_lp1));
    acc = rv_ap(DAP1A, rv_lp1, -RV_KAP);
    acc = rv_ap(DAP1B, acc, RV_KAP);
    rv_wr(DEL1, acc);
    *l = 2.0f * RV_OUT * acc;
    /* loop 2: del1 back in, into del2; the right */
    acc = apout + RV_KRT * rv_interp(DEL1, RV_DEL1_AT + RV_DEL1_MOD * u1);
    rv_lp2 = fm_flush(rv_lp2 + RV_KLP * (acc - rv_lp2));
    acc = rv_ap(DAP2A, rv_lp2, RV_KAP);
    acc = rv_ap(DAP2B, acc, -RV_KAP);
    rv_wr(DEL2, acc);
    *r = 2.0f * RV_OUT * acc;
}

/* -------------------------------------------------------------------------------------------- chorus --- */
/* Rings' line (2047 at 48 kHz) and taps (1200 and 800, swinging by depth x 384) x 7350 / 48000 */
#define CH_N 512u
#define CH_T1 (1200.0f * FS_CH / 48000.0f)
#define CH_T2 (800.0f * FS_CH / 48000.0f)
#define CH_DEPTH (384.0f * FS_CH / 48000.0f)
#define CH_LFO1 (FM_TWO_PI * 4.17e-06f * 48000.0f / FS_CH) /* Rings' phase steps a sample: 0.2 Hz and 0.26 Hz */
#define CH_LFO2 (FM_TWO_PI * 5.417e-06f * 48000.0f / FS_CH)
static int16_t ch_buf[CH_N] RFX_POOL;
static uint32_t ch_w;
static float c1s, c1c = 1.0f, c2s, c2c = 1.0f;

static inline float ch_interp(float at)
{
    uint32_t i = (uint32_t)at;
    float f = at - (float)i, a = f16(ch_buf[(ch_w + i) & (CH_N - 1u)]);
    return a + (f16(ch_buf[(ch_w + i + 1u) & (CH_N - 1u)]) - a) * f;
}

void rfx_chorus_clear(void)
{
    memset(ch_buf, 0, sizeof ch_buf);
    ch_w = 0;
    c1s = c2s = 0.0f;
    c1c = c2c = 1.0f;
}

void rfx_lfo_restart(void)
{
    l1s = l2s = c1s = c2s = 0.0f;
    l1c = l2c = c1c = c2c = 1.0f;
}

void rfx_chorus(float x, float p, float *l, float *r)
{
    float d = (0.15f + 0.5f * p) * CH_DEPTH, a = 0.5f * p; /* the amount p; each side the mean of its two taps */
    ch_w = (ch_w - 1u) & (CH_N - 1u);
    {
        float s = c1s + CH_LFO1 * c1c, c = c1c - CH_LFO1 * s;
        c1s = s;
        c1c = c;
        s = c2s + CH_LFO2 * c2c;
        c = c2c - CH_LFO2 * s;
        c2s = s;
        c2c = c;
    }
    ch_buf[ch_w] = q16(x);
    *l = a * (ch_interp(c1s * d + CH_T1) + ch_interp(c2s * d + CH_T2));
    *r = a * (ch_interp(c1c * d + CH_T2) + ch_interp(c2c * d + CH_T1));
}

float rfx_chorus_dry(float p)
{
    return 1.0f - 0.5f * p;
}

int rfx_tiny(void)
{
    float st[] = {rv_lp1, rv_lp2};
    int i, c = 0;
    for (i = 0; i < 2; i++)
        c += st[i] != 0.0f && fm_fabsf(st[i]) < 1e-20f;
    return c;
}
#else
typedef int rfx_off_t; /* (not built for the device) */
#endif

/* SPDX-License-Identifier: MPL-2.0 */
/* Plate reverb adapted from AMB-1 (fm1-amb, app/dsp/amb.c), after Dattorro 1997, MPL-2.0. */
/* The lo-fi chain and the bed (see fx.h). Per call, in this order:
     the music (mono, already through the dark filter and the song gain, voices.c)
       -> wow and flutter: a short delay, its length moved by two LFOs (vinyl: the 1.8 s rotation and a 5.3 Hz
          flutter; tape: 0.9 Hz with a slower drift, and a 7 Hz flutter), read with linear interpolation
       -> soft saturation: a cubic soft clip, sat(k x) / k
       -> grit (dust 2-3): a sample-and-hold at a fractional rate and a quantizer, mixed in
       -> the dust low-pass: a one-pole at 5 / 3.5 / 2.5 / 1.5 kHz (dust 0's is the old 5 kHz mix filter)
       -> the kick ducking: the kick's envelope above 0.4, squared, smoothed (5 ms in, 150 ms out): light, on the
          kick's body only, never its long 808 tail
       -> the soft clip (below 0.7)
     + the plate's stereo return (its input: the voices' per-role sends, voices.c; not dusted, so the dust's wobble
       does not smear twice, and over the clip, which it rarely reaches)
     + the bed: vinyl (hiss, sparse clicks, a rotation tick every 1.8 s) or tape hiss, and rain (a low-passed wash
       under slow gusts, droplet pings). The bed is neither dusted, darkened nor faded by the song gain: it has its
       own open/close ramp (FX_RAMP_S) and each layer its own level ramp, so a change crossfades. Together, surface
       and rain each sit 2.5 dB lower (the pairing rule: the bed's level stays put, it does not thicken into mud).
   Cost: the plate and the bed run at a third of the rate (14.7 kHz; the plate's highs are damped anyway, the hiss
   tops out near 7 kHz), the plate in one masked ring; the dust's parameters and LFOs move at a control rate of CR samples. Dust changes ramp over FX_RAMP_S
   through a continuous dust value (the parameters interpolate between the levels' tables). Noise is a seeded
   xorshift; every decaying state is flushed. */
#include <stdint.h>
#include <string.h>
#include "fastmath.h"
#include "library.h"
#include "fx.h"

#if defined PLAT_HOST || defined __APPLE__
#define FX_POOL
#else
#define FX_POOL __attribute__((section(".pool")))
#endif

#define SR 44100.0f
#define CR 16                              /* control rate: the dust's parameters and LFOs every 16 samples */
#define RAMP (1.0f / (FX_RAMP_S * SR))      /* a level ramp's step per sample: 0 to 1 in FX_RAMP_S */
#define CLIP_T 0.4f                        /* soft clip: linear below, a knee approaching CLIP_L */
#define CLIP_L 0.70f
#define STOP_STEP (1.0f / (0.04f * SR)) /* fx_stop's fade: 40 ms */
#define CENTS_K (1731.234f * FM_TWO_PI / SR) /* cents of pitch per (sample of delay depth x Hz of LFO) */

/* ---------------------------------------------------------------------------------------------- dust --- */
/* per dust level 0-3; the tape flavor scales the wow and the flutter */
static const float D_LP[4] = {0.509f, 0.392f, 0.300f, 0.192f};   /* one-pole: 5, 3.5, 2.5, 1.5 kHz */
static const float D_WOW[4] = {0.0f, 3.5f, 5.5f, 7.5f};          /* cents peak (vinyl) */
static const float D_FLUT[4] = {0.0f, 0.5f, 0.8f, 1.1f};
static const float D_DRIVE[4] = {0.5f, 1.4f, 2.0f, 2.8f};         /* saturation: sat(k x) / k */
static const float D_DUCK[4] = {0.12f, 0.15f, 0.18f, 0.21f};      /* ducking depth at a full kick body */
#define TAPE_WOW 1.5f       /* the wobbly tape: half as much wow again, twice the flutter */
#define TAPE_FLUT 2.0f
#define WOW_VINYL (1.0f / 1.8f) /* the record's rotation (33 1/3 rpm) */
#define WOW_TAPE 0.9f
#define WOW_DRIFT 0.37f     /* tape: a second, slower wow, 40% deep */
#define FLUT_VINYL 5.3f
#define FLUT_TAPE 7.0f
#define WL_N 256            /* the wow delay line (a power of two); its depth tops out near 60 samples */
#define DUCK_THR 0.4f       /* the kick envelope's body: above 0.4 of full scale */
#define DUCK_ATT 0.0045f    /* per sample (applied per control step): 5 ms */
#define DUCK_REL 0.00015f   /* 150 ms */

static float dust, dust_t, dust_s, flav, flav_t, flav_s; /* the continuous dust and flavor (0 vinyl, 1 tape) */
static float p_lp, p_wd, p_fd, p_drive, p_idrive, p_grit, p_rate, p_q, p_duck; /* the current parameters */
static float wl[WL_N];
static uint32_t wl_w;
static float wow_ph, drift_ph, flut_ph, del, del_step;
static uint32_t cr_n;
static float sh_ph, sh_y, lp_y, duck_s, dg = 1.0f, dg_step;

/* ----------------------------------------------------------------------------------------------- bed --- */
/* The bed runs at a third of the rate (BR, 14.7 kHz: its hiss tops out near 7 kHz), each step's output interpolated
   over three samples. */
#define BDIV 3u
#define BR (SR / (float)BDIV)
#define TICK_N 26460u       /* the rotation tick: every 1.8 s (in bed steps) */
#define CLICK_P 3.0f        /* random clicks a second (the rotation's tick besides) */
#define CLICK_DEC 0.17f     /* a click's fall per step: about 0.1 ms */
#define TICK_A 0.33f        /* the tick's peak; random clicks 0.3-0.7 of it */
#define HISS_V 0.0203f      /* vinyl hiss level (white, before its band-pass) */
#define HISS_T 0.0130f      /* tape hiss level */
#define RAIN_W 0.049f       /* rain wash level (white, before its low-pass) */
#define DROP_A 0.0037f      /* droplet ping level */
#define DROP_P 14.0f        /* droplets a second */
#define DROP_R 0.977f       /* a droplet's ring: about 20 ms to -60 dB */
#define PAIR 0.75f          /* surface and rain together: each a little lower, the bed's sum stays put */
#define GUST_K (1.0f / (1.2f * BR)) /* the gusts' smoother, per step: 1.2 s, twice */
#define BCR 16              /* the gusts update every 16 steps */

static float bo, bo_t;                  /* the bed's open level and target */
static float lv, lv_t, lt, lt_t, lr, lr_t; /* layer levels: vinyl, tape, rain */
static uint32_t brng = 0x9E3779B9u;
static float hv_l[2], hv_r[2], ht_l[2], ht_r[2]; /* hiss filter states (low-pass, high-pass) */
static float clk_l, clk_r;
static uint32_t tick_n, clk_n = 1, drop_n = 1, bstep_n;
static float rw_l[2], rw_r[2], gust, gust1, gust_t;
static uint32_t gust_n;
static float b_l0, b_l1, b_r0, b_r1; /* the last two steps' outputs */
static uint8_t b_ph;
static float dp_y1, dp_y2, dp_a1, dp_pl, dp_pr;

/* --------------------------------------------------------------------------------------------- plate --- */
/* AMB-1's Dattorro plate at 14.7 kHz: the paper's lengths (for 29761 Hz) scaled by HS(), a 10 ms predelay. */
#define HS(n) (((n) * 4939 + 5000) / 10000)  /* 14700 / 29761 */
#define PRE_N 147
#define EXC HS(16)
enum { L_PRE, L_IN1, L_IN2, L_IN3, L_IN4, L_APL, L_D1L, L_AP2L, L_D2L, L_APR, L_D1R, L_AP2R, L_D2R, L_N };
static const int DL_LEN[L_N] = {PRE_N, HS(142), HS(107), HS(379), HS(277), HS(672) + EXC + 2, HS(4453), HS(1800),
                                HS(3720), HS(908) + EXC + 2, HS(4217), HS(2656), HS(3163)};
/* One ring for every line (as Mutable Instruments' FxEngine): a single write position pw, falling one per step; line
   k lives at offsets [pl_base[k], pl_base[k] + DL_LEN[k] + 1) from it, so a read or a write is an add and a mask. */
#define PL_N 16384u          /* a power of two above the lines' sum (11.3 k) */
#define PL_DECAY 0.5f        /* a small plate: about 1.8 s to -60 dB */
#define PL_DD2 0.5f
#define PL_OUT_4 0.65f       /* the return over four taps a side (AMB-1: 0.6 over seven) */
#define PL_QUIET 1e-5f       /* -100 dB: below this the tail is over */
static float pl_buf[PL_N] FX_POOL;
static uint32_t pl_base[L_N], pw;
static float pl_bw, pl_damp_l, pl_damp_r, lfo_s, lfo_c = 1.0f;
static float pl_acc, pl_wl0, pl_wl1, pl_wr0, pl_wr1, pl_peak;
static uint8_t pl_ph, pl_open;
static float og = 1.0f; /* the plate's return level: 1, or falling to 0 after fx_stop */
static uint8_t stopping;

static uint32_t xs(void)
{
    brng ^= brng << 13;
    brng ^= brng >> 17;
    brng ^= brng << 5;
    return brng;
}
static float xs_u(void) { return (float)(xs() >> 8) * (1.0f / 16777216.0f); }      /* 0..1 */

static inline float tap(int k, int d)
{
    return pl_buf[(pw + pl_base[k] + (uint32_t)d) & (PL_N - 1u)];
}
static inline void push(int k, float x)
{
    pl_buf[(pw + pl_base[k]) & (PL_N - 1u)] = x;
}
static inline float allpass(int k, int len, float x, float g)
{
    float z = tap(k, len), v = x - g * z;
    push(k, fm_flush(v));
    return z + g * v;
}
static inline float allpass_mod(int k, float len, float x, float g)
{
    int i = (int)len;
    float f = len - (float)i, z0 = tap(k, i), z = z0 + (tap(k, i + 1) - z0) * f, v = x - g * z;
    push(k, fm_flush(v));
    return z + g * v;
}

/* one step of the plate (AMB-1's reverb(), its decay fixed, its filters retuned for 14.7 kHz) */
static void plate(float in, float *ol, float *or_)
{
    float x, a, b, fb_l = tap(L_D2R, HS(3163)), fb_r = tap(L_D2L, HS(3720));
    push(L_PRE, in);
    x = tap(L_PRE, PRE_N);
    pl_bw = fm_flush(pl_bw + 0.77f * (x - pl_bw)); /* about 3.4 kHz */
    x = allpass(L_IN1, HS(142), pl_bw, 0.75f);
    x = allpass(L_IN2, HS(107), x, 0.75f);
    x = allpass(L_IN3, HS(379), x, 0.625f);
    x = allpass(L_IN4, HS(277), x, 0.625f);
    {
        float s = lfo_s + 0.0003f * lfo_c, c = lfo_c - 0.0003f * s; /* about 0.7 Hz */
        lfo_s = s;
        lfo_c = c;
    }
    a = allpass_mod(L_APL, (float)HS(672) + (float)EXC * lfo_s, x + PL_DECAY * fb_l, -0.7f);
    push(L_D1L, a);
    b = tap(L_D1L, HS(4453));
    pl_damp_l = fm_flush(pl_damp_l + 0.61f * (b - pl_damp_l)); /* about 2.2 kHz */
    b = allpass(L_AP2L, HS(1800), pl_damp_l * PL_DECAY, PL_DD2);
    push(L_D2L, b);
    a = allpass_mod(L_APR, (float)HS(908) + (float)EXC * lfo_c, x + PL_DECAY * fb_r, -0.7f);
    push(L_D1R, a);
    b = tap(L_D1R, HS(4217));
    pl_damp_r = fm_flush(pl_damp_r + 0.61f * (b - pl_damp_r));
    b = allpass(L_AP2R, HS(2656), pl_damp_r * PL_DECAY, PL_DD2);
    push(L_D2R, b);
    /* four of Dattorro's seven taps a side (the cost; the small plate keeps its density from the tank) */
    *ol = PL_OUT_4 * (tap(L_D1R, HS(266)) + tap(L_D1R, HS(2974)) - tap(L_AP2R, HS(1913)) - tap(L_D1L, HS(1990)));
    *or_ = PL_OUT_4 * (tap(L_D1L, HS(353)) + tap(L_D1L, HS(3627)) - tap(L_AP2L, HS(1228)) - tap(L_D1R, HS(2111)));
    pw = (pw - 1u) & (PL_N - 1u);
}

/* the dust's parameters at the continuous dust and flavor values */
static void dust_params(void)
{
    int i = (int)dust;
    float f = dust - (float)i, w, fl, wr;
    if (i >= 3) {
        i = 2;
        f = 1.0f;
    }
#define LERP(t) ((t)[i] + ((t)[i + 1] - (t)[i]) * f)
    p_lp = LERP(D_LP);
    w = LERP(D_WOW) * (1.0f + (TAPE_WOW - 1.0f) * flav);
    fl = LERP(D_FLUT) * (1.0f + (TAPE_FLUT - 1.0f) * flav);
    p_drive = LERP(D_DRIVE);
    p_duck = LERP(D_DUCK);
#undef LERP
    p_idrive = 1.0f / p_drive;
    wr = WOW_VINYL + (WOW_TAPE - WOW_VINYL) * flav;
    p_wd = w / (CENTS_K * (wr + 0.4f * flav * WOW_DRIFT)); /* depth in samples: the wow and the tape's drift at peak */
    p_fd = fl / (CENTS_K * (FLUT_VINYL + (FLUT_TAPE - FLUT_VINYL) * flav));
    p_grit = fm_clampf(dust - 1.0f, 0.0f, 1.0f);
    p_rate = dust <= 2.0f ? 0.5f : 0.5f - (dust - 2.0f) * (0.5f - 1.0f / 3.0f); /* hold 2 samples, then 3 */
    p_q = fm_exp2f(11.0f - 2.0f * fm_clampf(dust - 2.0f, 0.0f, 1.0f));          /* 12 bits, then 10 */
}

void fx_init(void)
{
    uint32_t b;
    int k;
    dust = dust_t = 0.0f;
    dust_s = flav_s = 0.0f;
    flav = flav_t = 0.0f;
    memset(wl, 0, sizeof wl);
    wl_w = 0;
    wow_ph = drift_ph = flut_ph = 0.0f;
    del = del_step = 0.0f;
    cr_n = 0;
    sh_ph = sh_y = lp_y = duck_s = dg_step = 0.0f;
    dg = 1.0f;
    dust_params();
    bo = bo_t = lv = lv_t = lt = lt_t = lr = lr_t = 0.0f;
    brng = 0x9E3779B9u;
    memset(hv_l, 0, sizeof hv_l);
    memset(hv_r, 0, sizeof hv_r);
    memset(ht_l, 0, sizeof ht_l);
    memset(ht_r, 0, sizeof ht_r);
    clk_l = clk_r = 0.0f;
    tick_n = bstep_n = 0;
    b_l0 = b_l1 = b_r0 = b_r1 = 0.0f;
    b_ph = 0;
    clk_n = drop_n = 1;
    memset(rw_l, 0, sizeof rw_l);
    memset(rw_r, 0, sizeof rw_r);
    gust = gust1 = gust_t = 0.7f;
    gust_n = 0;
    dp_y1 = dp_y2 = dp_a1 = dp_pl = dp_pr = 0.0f;
    for (k = 0, b = 0; k < L_N; k++) {
        pl_base[k] = b;
        b += (uint32_t)DL_LEN[k] + 1u;
    }
    pw = 0;
    memset(pl_buf, 0, sizeof pl_buf);
    pl_bw = pl_damp_l = pl_damp_r = 0.0f;
    lfo_s = 0.0f;
    lfo_c = 1.0f;
    pl_acc = pl_wl0 = pl_wl1 = pl_wr0 = pl_wr1 = pl_peak = 0.0f;
    pl_ph = pl_open = 0;
    og = 1.0f;
    stopping = 0;
}

void fx_configure(int dust_lv, int noise, int ambience, int flavor)
{
    float pair;
    dust_t = fm_clampf((float)dust_lv, 0.0f, 3.0f);
    flav_t = noise == 2 || flavor == FL_CHILLWAVE ? 1.0f : 0.0f;
    dust_s = fm_fabsf(dust_t - dust) * RAMP * (float)CR;    /* each ramp lands in FX_RAMP_S */
    flav_s = fm_fabsf(flav_t - flav) * RAMP * (float)CR;
    pair = (noise == 1 || noise == 2) && ambience == 1 ? PAIR : 1.0f;
    lv_t = noise == 1 ? pair : 0.0f;
    lt_t = noise == 2 ? pair : 0.0f;
    lr_t = ambience == 1 ? pair : 0.0f;
    if (bo == 0.0f) { /* the bed was closed: its layers land, the open ramp is the fade-in */
        lv = lv_t;
        lt = lt_t;
        lr = lr_t;
    }
    bo_t = 1.0f;
    pl_open = 1;
    og = 1.0f;
    stopping = 0;
}

void fx_land(void)
{
    dust = dust_t;
    flav = flav_t;
    dust_params();
}

void fx_close(void)
{
    bo_t = 0.0f;
}

void fx_stop(void)
{
    bo_t = 0.0f;
    stopping = 1;
}

int fx_active(void)
{
    return bo > 0.0f || bo_t > 0.0f || pl_peak > PL_QUIET;
}

static int tiny(const float *x, int n)
{
    int i, c = 0;
    for (i = 0; i < n; i++)
        c += x[i] != 0.0f && fm_fabsf(x[i]) < 1e-20f;
    return c;
}

int fx_tiny(void)
{
    float st[] = {lp_y, sh_y, duck_s, clk_l, clk_r, dp_y1, dp_y2, b_l0, b_l1, b_r0, b_r1, pl_bw, pl_damp_l, pl_damp_r, pl_acc, pl_wl0, pl_wl1,
                  pl_wr0, pl_wr1};
    return tiny(pl_buf, (int)PL_N) + tiny(wl, WL_N) + tiny(st, (int)(sizeof st / sizeof st[0])) + tiny(hv_l, 2) +
           tiny(hv_r, 2) + tiny(ht_l, 2) + tiny(ht_r, 2) + tiny(rw_l, 2) + tiny(rw_r, 2);
}

static float ramp(float x, float t, float s)
{
    return x < t ? fm_minf(x + s, t) : fm_maxf(x - s, t);
}

/* a control step of the dust: its ramps, the LFOs and the delay's next target (reached linearly over CR samples) */
static void dust_control(void)
{
    float d;
    if (dust != dust_t || flav != flav_t) {
        dust = ramp(dust, dust_t, dust_s);
        flav = ramp(flav, flav_t, flav_s);
        dust_params();
    }
    wow_ph += (WOW_VINYL + (WOW_TAPE - WOW_VINYL) * flav) * (float)CR / SR;
    drift_ph += WOW_DRIFT * (float)CR / SR;
    flut_ph += (FLUT_VINYL + (FLUT_TAPE - FLUT_VINYL) * flav) * (float)CR / SR;
    wow_ph -= wow_ph >= 1.0f ? 1.0f : 0.0f;
    drift_ph -= drift_ph >= 1.0f ? 1.0f : 0.0f;
    flut_ph -= flut_ph >= 1.0f ? 1.0f : 0.0f;
    /* the delay swings around its centre p_wd + p_fd (at dust 0: no delay at all) */
    d = p_wd * (1.0f + 0.4f * flav) + p_fd + p_wd * fm_sin_turns(wow_ph) + 0.4f * flav * p_wd * fm_sin_turns(drift_ph) +
        p_fd * fm_sin_turns(flut_ph);
    d = fm_clampf(d, 0.0f, (float)(WL_N - 2));
    del_step = (d - del) * (1.0f / (float)CR);
}

static inline float clip(float y)
{
    float x;
    if (y <= CLIP_T && y >= -CLIP_T) /* most samples */
        return y;
    x = fm_fabsf(y) - CLIP_T;
    x = CLIP_T + (CLIP_L - CLIP_T) * x / (x + (CLIP_L - CLIP_T));
    return y < 0.0f ? -x : x;
}

/* bed steps to the next event of a Poisson stream at rate per second */
static uint32_t next_gap(float rate)
{
    return 1u + (uint32_t)(-fm_logf(1.0f - xs_u()) * (BR / rate));
}

/* the bed into l and r, at a third of the rate: one xorshift a step gives both channels' white noise; clicks and droplets
   come at Poisson-drawn gaps; the state in locals (l's stores may alias the statics) */
static void bed(float *l, float *r, uint32_t n)
{
    uint32_t i, rs = brng, tk = tick_n, cn = clk_n, dn = drop_n, ph = b_ph;
    float b = bo, v = lv, t = lt, w = lr, cl = clk_l, cr = clk_r, gs = gust;
    float h0l = hv_l[0], h0r = hv_r[0], h1l = hv_l[1], h1r = hv_r[1];
    float t1l = ht_l[1], t1r = ht_r[1];
    float r0l = rw_l[0], r0r = rw_r[0], r1l = rw_l[1], r1r = rw_r[1];
    float y1 = dp_y1, y2 = dp_y2, l0 = b_l0, l1 = b_l1, q0 = b_r0, q1 = b_r1;
    float bstep = (float)BDIV * (stopping ? STOP_STEP : RAMP);
    for (i = 0; i < n; i++) {
        float nl, nr, ol = 0.0f, or_ = 0.0f, g;
        if (ph) { /* a sample between two steps: interpolated */
            float f = (float)ph * (1.0f / (float)BDIV);
            l[i] += l0 + (l1 - l0) * f;
            r[i] += q0 + (q1 - q0) * f;
            if (++ph == BDIV)
                ph = 0;
            continue;
        }
        ph = 1;
        if (b != bo_t)
            b = ramp(b, bo_t, bstep);
        if (v != lv_t)
            v = ramp(v, lv_t, (float)BDIV * RAMP);
        if (t != lt_t)
            t = ramp(t, lt_t, (float)BDIV * RAMP);
        if (w != lr_t)
            w = ramp(w, lr_t, (float)BDIV * RAMP);
        rs ^= rs << 13;
        rs ^= rs >> 17;
        rs ^= rs << 5;
        nl = (float)(int16_t)rs * (1.0f / 32768.0f);
        nr = (float)(int16_t)(rs >> 16) * (1.0f / 32768.0f);
        if (v > 0.0f) { /* vinyl: a band of hiss (about 1.6-6.8 kHz), clicks, the rotation tick */
            float a = 0.0f, pan = 0.5f;
            g = v * b;
            h0l += 0.945f * (nl - h0l);
            h0r += 0.945f * (nr - h0r);
            h1l += 0.495f * (h0l - h1l);
            h1r += 0.495f * (h0r - h1r);
            if (++tk >= TICK_N) {
                tk = 0;
                a = TICK_A;
            } else if (--cn == 0) {
                brng = rs;
                cn = next_gap(CLICK_P);
                a = TICK_A * (0.3f + 0.4f * xs_u()) * (xs() & 1u ? 1.0f : -1.0f);
                pan = 0.3f + 0.4f * xs_u();
                rs = brng;
            }
            cl = cl * CLICK_DEC + a * pan;
            cr = cr * CLICK_DEC + a * (1.0f - pan);
            ol += g * (HISS_V * (h0l - h1l) + cl);
            or_ += g * (HISS_V * (h0r - h1r) + cr);
        }
        if (t > 0.0f) { /* tape: broadband hiss to the step rate's top, its lows thinned (about 350 Hz) */
            g = t * b * HISS_T;
            t1l += 0.139f * (nl - t1l);
            t1r += 0.139f * (nr - t1r);
            ol += g * (nl - t1l);
            or_ += g * (nr - t1r);
        }
        if (w > 0.0f) { /* rain: a wash (two one-poles, about 900 Hz) under the gusts, droplet pings */
            float x = 0.0f;
            g = w * b;
            if ((++bstep_n & (BCR - 1)) == 0) { /* the gusts: a new level every 3-6 s, smoothed twice */
                if (gust_n == 0) {
                    brng = rs;
                    gust_t = 0.35f + 0.65f * xs_u();
                    gust_n = (uint32_t)((3.0f + 3.0f * xs_u()) * BR / (float)BCR);
                    rs = brng;
                }
                gust_n--;
                gust1 += GUST_K * (float)BCR * (gust_t - gust1);
                gs += GUST_K * (float)BCR * (gust1 - gs);
            }
            r0l += 0.319f * (nl - r0l);
            r0r += 0.319f * (nr - r0r);
            r1l += 0.319f * (r0l - r1l);
            r1r += 0.319f * (r0r - r1r);
            if (--dn == 0) { /* a droplet: a ping at 1.5-4.5 kHz, about 20 ms long */
                float f;
                brng = rs;
                dn = next_gap(DROP_P);
                f = 1500.0f + 3000.0f * xs_u();
                dp_a1 = 2.0f * DROP_R * fm_cosf(FM_TWO_PI * f / BR);
                x = DROP_A * (0.3f + 0.7f * xs_u());
                dp_pl = 0.2f + 0.6f * xs_u();
                dp_pr = 1.0f - dp_pl;
                rs = brng;
            }
            x += dp_a1 * y1 - DROP_R * DROP_R * y2;
            y2 = y1;
            y1 = x;
            ol += g * (RAIN_W * gs * r1l + dp_pl * y1);
            or_ += g * (RAIN_W * gs * r1r + dp_pr * y1);
        }
        l0 = l1;
        q0 = q1;
        l1 = ol;
        q1 = or_;
        l[i] += l0; /* a step late, so the next sample can interpolate */
        r[i] += q0;
    }
    brng = rs;
    tick_n = tk;
    clk_n = cn;
    drop_n = dn;
    b_ph = (uint8_t)ph;
    bo = b;
    lv = v;
    lt = t;
    lr = w;
    clk_l = fm_flush(cl);
    clk_r = fm_flush(cr);
    gust = gs;
    hv_l[0] = fm_flush(h0l);
    hv_r[0] = fm_flush(h0r);
    hv_l[1] = fm_flush(h1l);
    hv_r[1] = fm_flush(h1r);
    ht_l[1] = fm_flush(t1l);
    ht_r[1] = fm_flush(t1r);
    rw_l[0] = fm_flush(r0l);
    rw_r[0] = fm_flush(r0r);
    rw_l[1] = fm_flush(r1l);
    rw_r[1] = fm_flush(r1r);
    dp_y1 = fm_flush(y1);
    dp_y2 = fm_flush(y2);
    b_l0 = fm_flush(l0);
    b_l1 = fm_flush(l1);
    b_r0 = fm_flush(q0);
    b_r1 = fm_flush(q1);
}

/* the dust on the music in l, in place: control steps of CR samples, the state in locals inside each (l's stores may
   alias the statics, so the compiler would reload them every sample) */
static void dust_run(float *l, uint32_t n, float dt)
{
    uint32_t i = 0;
    while (i < n) {
        uint32_t m, k, w = wl_w;
        float d = del, ds, lp = lp_y, g = dg, gs, sh = sh_ph, shy = sh_y;
        float drive, idrive, grit, rate, q, a;
        int wow;
        if (cr_n == 0) {
            dust_control();
            duck_s += (dt - duck_s) * (dt > duck_s ? DUCK_ATT : DUCK_REL) * (float)CR;
            duck_s = fm_flush(duck_s);
            dg_step = (1.0f - p_duck * duck_s - dg) * (1.0f / (float)CR);
        }
        m = CR - cr_n < n - i ? CR - cr_n : n - i;
        ds = del_step;
        wow = d != 0.0f || ds != 0.0f;
        gs = dg_step;
        drive = p_drive;
        idrive = p_idrive;
        grit = p_grit;
        rate = p_rate;
        q = p_q;
        a = p_lp;
        for (k = 0; k < m; k++) {
            float y, f, u;
            int j;
            /* wow and flutter (at dust 0 the delay sits at 0: the line is kept, the read skipped) */
            y = l[i + k];
            wl[w] = y;
            if (wow) {
                d += ds;
                j = (int)d;
                f = d - (float)j;
                y = wl[(w - (uint32_t)j) & (WL_N - 1)];
                y += (wl[(w - (uint32_t)j - 1u) & (WL_N - 1)] - y) * f;
            }
            w = (w + 1) & (WL_N - 1);
            /* saturation: a cubic soft clip of the driven signal (flat at 1 from |u| = 1.5), gain-matched */
            u = fm_clampf(y * drive, -1.5f, 1.5f);
            y = u * (1.0f - (4.0f / 27.0f) * u * u) * idrive;
            /* grit: hold at the rate, quantize, mix in */
            if (grit > 0.0f) {
                sh += rate;
                if (sh >= 1.0f) {
                    sh -= 1.0f;
                    shy = fm_floorf(y * q + 0.5f) / q;
                }
                y += grit * (shy - y);
            }
            /* the dust low-pass, then the kick ducking (its gain stepped at the control rate, reached linearly) */
            lp += a * (y - lp);
            g += gs;
            l[i + k] = lp * g;
        }
        wl_w = w;
        del = d;
        lp_y = fm_flush(lp);
        dg = g;
        sh_ph = sh;
        sh_y = shy;
        cr_n = (cr_n + m) & (CR - 1);
        i += m;
    }
}

/* the plate at a third of the rate (a triple's mean in, its output interpolated) on the send in r, added to the
   music in l after the music's soft clip */
static void plate_run(float *l, float *r, uint32_t n)
{
    uint32_t i;
    float peak = 0.0f, wl0 = pl_wl0, wl1 = pl_wl1, wr0 = pl_wr0, wr1 = pl_wr1, acc = pl_acc;
    float gate = pl_open ? 1.0f / 3.0f : 0.0f;
    uint32_t ph = pl_ph;
    for (i = 0; i < n; i++) {
        float ol, or_, y = l[i], f;
        acc += r[i];
        if (++ph == 3u) {
            ph = 0;
            wl0 = wl1;
            wr0 = wr1;
            plate(gate * acc, &wl1, &wr1);
            wl1 = fm_flush(wl1);
            wr1 = fm_flush(wr1);
            acc = 0.0f;
        }
        f = (float)ph * (1.0f / 3.0f);
        ol = wl0 + (wl1 - wl0) * f;
        or_ = wr0 + (wr1 - wr0) * f;
        if (stopping) {
            og = fm_maxf(og - STOP_STEP, 0.0f);
            ol *= og;
            or_ *= og;
        }
        f = fm_fabsf(ol) + fm_fabsf(or_);
        peak = f > peak ? f : peak;
        y = clip(y); /* the music's knee; the plate's return rides over it */
        l[i] = y + ol;
        r[i] = y + or_;
    }
    pl_wl0 = wl0;
    pl_wl1 = wl1;
    pl_wr0 = wr0;
    pl_wr1 = wr1;
    pl_acc = fm_flush(acc);
    pl_ph = (uint8_t)ph;
    pl_peak = peak;
}

void fx_process(float *l, float *r, uint32_t n, float duck)
{
    uint32_t i;
    float dt = fm_clampf((duck - DUCK_THR) * (1.0f / (1.0f - DUCK_THR)), 0.0f, 1.0f);
    dust_run(l, n, dt * dt);
    if (pl_open || pl_peak > PL_QUIET)
        plate_run(l, r, n);
    else
        for (i = 0; i < n; i++)
            l[i] = r[i] = clip(l[i]);
    if (stopping && og == 0.0f) { /* stopped: the plate shut (its tail left for vx_init to clear) */
        pl_open = 0;
        pl_peak = 0.0f;
    }
    if (bo > 0.0f || bo_t > 0.0f)
        bed(l, r, n);
}

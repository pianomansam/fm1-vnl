/* SPDX-License-Identifier: MPL-2.0 */
/* Plate reverb adapted from AMB-1 (fm1-amb, app/dsp/amb.c), after Dattorro 1997, MPL-2.0. */
/* The lo-fi chain and the bed (see fx.h). Per call, in this order:
     the music (mono, already through the role buses' filters and the song gain, voices.c)
       -> wow and flutter: a short delay, its length moved by two LFOs (vinyl: the 1.8 s rotation and a 5.3 Hz
          flutter; tape: 0.9 Hz with a slower drift, and a 7 Hz flutter), read with linear interpolation
       -> soft saturation: a cubic soft clip, sat(k x) / k
       -> grit (dust 2-3): a sample-and-hold at a fractional rate and a quantizer, mixed in
       -> the dust low-pass: a one-pole at 5 / 3.5 / 2.5 / 1.5 kHz (dust 0's is the old 5 kHz mix filter)
       -> the kick ducking: the kick's envelope above 0.4, squared, smoothed (5 ms in, 150 ms out), on the kick's
          body only, never its long 808 tail; its depth is the song's pump (fx_pump: about 1, 3 or 6 dB)
       -> the soft clip (below 0.7)
     + the wet returns, all from one loop on a 12-sample cycle, over the clip (which they rarely reach):
       - the plate's stereo return (its input: the role buses' sends, voices.c; not dusted, so the dust's wobble
         does not smear twice, and not ducked);
       - the chorus on the chords' bus: two taps swinging in quadrature (0.6 Hz, up to 10 cents) on a short line,
         tap A (12 ms) to the left, tap B (17 ms) to the right;
       - the tape delay: a dotted 8th of the song's tempo, fed back at 0.35 through a one-pole (about 2.2 kHz), a
         little wow (0.25 ms at 0.5 Hz) on its read head, its tail surviving song changes like the plate's;
       the chorus's and the delay's mid and side through the dust's low-pass (at that rate) and the duck's gain, so
       they sit in the dusted, pumping music
     + the bed: vinyl (hiss, sparse clicks, a rotation tick every 1.8 s) or tape hiss, and rain (a low-passed wash
       under slow gusts, droplet pings). The bed is neither dusted, filtered nor faded by the song gain: it has its
       own open/close ramp (FX_RAMP_S) and each layer its own level ramp, so a change crossfades. Together, surface
       and rain each sit 2.5 dB lower (the pairing rule: the bed's level stays put, it does not thicken into mud).
   Cost: the plate runs at a quarter of the rate (11.025 kHz; its input is band-limited to 3.4 kHz and its tank damped
   at 2.2 kHz anyway), the bed at a third (14.7 kHz: the hiss tops out near 7 kHz), the chorus and the delay at a
   sixth (7.35 kHz), each output interpolated linearly; the wet inputs come summed per pair of samples
   (fx_process_pairs). The plate is one masked ring; the dust's parameters and LFOs move at a control rate of CR
   samples. Dust changes ramp over FX_RAMP_S through a continuous dust value (the parameters interpolate between the
   levels' tables). Noise is a seeded xorshift; every decaying state is flushed.
   The reverb and the chorus are Rings' (rings_fx.c; Sam's pick in Task 7c's head-to-head: "ours sounds more like an
   echo"), in the slots described above for the plate and our chorus. Ours stays on the host only, for comparison
   (fx_rings(0), VNL_FX=ours); the device and web images leave the plate's ring out. */
#include <stdint.h>
#include <string.h>
#include "fastmath.h"
#include "library.h"
#include "fx.h"
#include "rings_fx.h"

/* our plate and chorus, kept on the host for comparison (fx_rings) */
#if (defined PLAT_HOST || defined __APPLE__) && !defined PLAT_WEB
#define FX_OURS 1
#else
#define FX_OURS 0
#endif

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
#define PUMP_CAL 1.39f      /* the smoothed duck reaches about 0.72 of a full dusty kick's body: the depth over it */

static float dust, dust_t, dust_s, flav, flav_t, flav_s; /* the continuous dust and flavor (0 vinyl, 1 tape) */
static float p_lp, p_lpw, p_wd, p_fd, p_drive, p_idrive, p_grit, p_rate, p_q; /* the current parameters */
static float wl[WL_N];
static uint32_t wl_w;
static float wl_last;  /* the fast path, wow off: the line is not written, only its newest sample kept */
static uint8_t wl_on;  /* the line is being written (the wow is on, or the full path) */
static uint8_t fast_off; /* fx_fast(0): every stage runs (the reference for the fast path) */
static float wow_ph, drift_ph, flut_ph, del, del_step;
static uint32_t cr_n;
static float sh_ph, sh_y, lp_y, duck_s, dg = 1.0f, dg_step;
static float pump_k = 0.12f; /* the duck's gain loss at a full kick body (fx_pump) */

/* ------------------------------------------------------------------------------------- the glue --- */
/* The mix bus's glue compressor (Sam: "yes"): the music's mean square over each control step into a level (30 ms
   attack; an auto release, 200 ms after a short peak, lengthening to 400 ms while it keeps compressing, so dense
   passages don't pump); 2:1 above GL_THR with a 4 dB soft knee, then GL_MU of make-up (the mix's level: on the
   reverb's return too, so the wet keeps its balance). Its gain rides with the duck's (the same per-sample ramp, so the
   chorus's and the delay's returns get it too), before the soft clip. Off in a dry engine (fx_init) until a song's
   fx_configure. */
#define GL_THR (-26.5f)   /* dBFS, the music's mean square (0.5 dB lower: the quieter comping, Sam) */
#define GL_KNEE 4.0f
#define GL_SLOPE 0.5f     /* 1 - 1/2 */
#define GL_MU 4.5f        /* make-up, dB: the mix's level */
#define GL_ATT ((float)CR / (0.03f * SR))
#define GL_REL_A ((float)CR / (0.2f * SR))
#define GL_REL_B ((float)CR / (0.4f * SR))
#define GL_HOLD ((float)CR / (1.0f * SR)) /* the release lengthens (and shortens) over about a second */
static float gl_acc, gl_p, gl_h, gl_gr, gl_g = 1.0f;
static float gl_mu = 1.0f; /* the make-up, linear, while the glue is on: on the reverb's return too (the mix's level) */
static uint8_t gl_on, gl_off;

/* ----------------------------------------------------------------------------------------------- bed --- */
/* The bed runs at a third of the rate (BR, 14.7 kHz: its hiss tops out near 7 kHz), each step's output interpolated
   over three samples. */
#define BDIV 3u
#define BR (SR / (float)BDIV)
#define TICK_N 26460u       /* the rotation tick: every 1.8 s (in bed steps) */
#define CLICK_P 3.0f        /* random clicks a second (the rotation's tick besides) */
#define CLICK_DEC 0.17f     /* a click's fall per step: about 0.1 ms */
/* The bed's level (Task 8, Sam: "the noise layer needs to be quieter"): about 35 dB under the music's RMS (Task 7
   had it 22-29 dB under); the songs' levels sit within a few dB of each other (the mix), so a fixed level lands each
   song's bed 30-40 dB under its own music. */
#define BED_LV 0.25f
#define TICK_A (0.33f * BED_LV)   /* the tick's peak; random clicks 0.3-0.7 of it */
#define HISS_V (0.0203f * BED_LV) /* vinyl hiss level (white, before its band-pass) */
#define HISS_T (0.0130f * BED_LV) /* tape hiss level */
#define RAIN_W (0.049f * BED_LV)  /* rain wash level (white, before its low-pass) */
#define DROP_A (0.0037f * BED_LV) /* droplet ping level */
#define DROP_P 14.0f        /* droplets a second */
#define DROP_R 0.977f       /* a droplet's ring: about 20 ms to -60 dB */
#define PAIR 0.75f          /* surface and rain together: each a little lower, the bed's sum stays put */
#define GUST_K (1.0f / (1.2f * BR)) /* the gusts' smoother, per step: 1.2 s, twice */
#define BCR 16              /* the gusts update every 16 steps */

static float bo, bo_t;                  /* the bed's open level and target */
static float bg = 1.0f, bg_t = 1.0f, bg_s; /* fx_bed_level: the gain, its target and its step (kept through fx_init) */
static float bg_u = 1.0f;               /* (fx_bed_level's own; the target is 0 while the guard has the bed off) */
static uint8_t g_bed, g_plate;          /* fx_guard's */
#define BG_STEPS (0.005f * BR)          /* its ramp: 5 ms */
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

/* ----------------------------------------------------------------------------- chorus and tape delay --- */
/* Both at a sixth of the rate (DR, 7.35 kHz, as AMB-1's tape runs at 11 kHz; a band-limited chorus, as a bucket
   brigade's), on the same step of the wet loop's 12-sample cycle (two a cycle): each input the six samples' mean,
   their return interpolated linearly over the next six. */
#define DR (SR / 6.0f)
#define CH_N 256u            /* the chorus's line: 35 ms at DR */
#define CH_A (0.012f * DR)   /* tap A's centre (left): 12 ms */
#define CH_B (0.017f * DR)   /* tap B's (right): 17 ms */
#define CH_MOD 11.25f        /* the taps' swing at depth 1, in steps: 1.5 ms, 10 cents at CH_HZ */
#define CH_HZ 0.6f
#define CH_W 0.5f            /* each tap's wet level at depth 1 */
#define CH_SLEW 0.002f       /* the depth's ramp per step: about 70 ms */
#define DL_N 8192u           /* the delay's line: 1.11 s at DR (a dotted 8th at 70 BPM is 0.64 s) */
#define DL_FB 0.35f
#define DL_LPK 0.847f        /* the loop's one-pole at DR: about 2.2 kHz */
#define DL_WOW 1.84f         /* the read head's wow: 0.25 ms at 0.5 Hz, about 1.4 cents */
#define DL_HZ 0.5f
#define DL_LAT 2.0f          /* the decimation's and the loop filter's latency (steps), taken off the read */
#define DL_GLIDE 0.0006f     /* a new tempo's glide per step: about 0.2 s */
static float ch_buf[CH_N] FX_POOL, dl_buf[DL_N] FX_POOL;
static uint32_t ch_w, dl_w;
static float ch_dep, ch_dep_t, ch_s, ch_c = 1.0f;        /* depth (0-1) and its target; the LFO (sine, cosine) */
static float w_el0, w_el1, w_er0, w_er1;                 /* the chorus's and delay's return, left, right: the last two */
static float dl_t, dl_tt = 0.75f * DR - DL_LAT, dl_y, dl_ws, dl_wc = 1.0f;
/* the line's level: a peak hold of what is written, falling no faster than the loop loses (9 dB a pass, 0.35's): above
   PL_QUIET the tail still rings */
static float dl_peak, dl_k = 0.99993f;
static float w_ac, w_ad;                 /* the chorus's and the delay's input sums */
static float w_ml, w_sl;                 /* their return's mid and side, through the dust's low-pass */
static uint8_t w_ph, dl_off, dl_land; /* the wet loop's phase: samples into its 12-sample cycle */
#define FX_BLK 256u
#define FX_NP (FX_BLK / FX_PAIR + 2u)

/* --------------------------------------------------------------------------------------------- plate --- */
/* AMB-1's Dattorro plate at 11.025 kHz: the paper's lengths (for 29761 Hz) scaled by HS(), a 10 ms predelay. */
#define HS(n) (((n) * 3705 + 5000) / 10000)  /* 11025 / 29761 */
#define PRE_N 110
#define EXC HS(16)
enum { L_PRE, L_IN1, L_IN2, L_IN3, L_IN4, L_APL, L_D1L, L_AP2L, L_D2L, L_APR, L_D1R, L_AP2R, L_D2R, L_N };
static const int DL_LEN[L_N] = {PRE_N, HS(142), HS(107), HS(379), HS(277), HS(672) + EXC + 2, HS(4453), HS(1800),
                                HS(3720), HS(908) + EXC + 2, HS(4217), HS(2656), HS(3163)};
/* One ring for every line (as Mutable Instruments' FxEngine): a single write position pw, falling one per step; line
   k lives at offsets [pl_base[k], pl_base[k] + DL_LEN[k] + 1) from it, so a read or a write is an add and a mask. */
#define PL_N (FX_OURS ? 16384u : 1u) /* a power of two above the lines' sum (8.5 k); one sample where ours is left out
                                        (plate_step never runs there) */
#define PL_DECAY 0.5f        /* a small plate: about 1.8 s to -60 dB */
#define PL_DD2 0.5f
#define PL_OUT_4 0.65f       /* the return over four taps a side (AMB-1: 0.6 over seven) */
#define PL_QUIET 1e-5f       /* -100 dB: below this the tail is over */
#define PL_FLOOR 1e-3f       /* -60 dB: a new song clears a tail under this (inaudible under its start) */
static float pl_buf[PL_N] FX_POOL;
static uint32_t pl_base[L_N], pw;
static float pl_bw, pl_damp_l, pl_damp_r, lfo_s, lfo_c = 1.0f;
static float pl_acc, pl_wl0, pl_wl1, pl_wr0, pl_wr1, pl_peak, pl_pp;   /* (pl_pp: the plate's own peak, a block) */
static uint8_t pl_open;
static uint8_t rings = 1; /* Rings' reverb and chorus (the default), or (fx_rings(0), host only) our plate and chorus */
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

/* one step of the plate (AMB-1's reverb(), its decay fixed, its filters retuned for 11.025 kHz) */
static void plate_step(float in, float *ol, float *or_)
{
    float x, a, b, fb_l = tap(L_D2R, HS(3163)), fb_r = tap(L_D2L, HS(3720));
    push(L_PRE, in);
    x = tap(L_PRE, PRE_N);
    pl_bw = fm_flush(pl_bw + 0.859f * (x - pl_bw)); /* about 3.4 kHz */
    x = allpass(L_IN1, HS(142), pl_bw, 0.75f);
    x = allpass(L_IN2, HS(107), x, 0.75f);
    x = allpass(L_IN3, HS(379), x, 0.625f);
    x = allpass(L_IN4, HS(277), x, 0.625f);
    {
        float s = lfo_s + 0.0004f * lfo_c, c = lfo_c - 0.0004f * s; /* about 0.7 Hz */
        lfo_s = s;
        lfo_c = c;
    }
    a = allpass_mod(L_APL, (float)HS(672) + (float)EXC * lfo_s, x + PL_DECAY * fb_l, -0.7f);
    push(L_D1L, a);
    b = tap(L_D1L, HS(4453));
    pl_damp_l = fm_flush(pl_damp_l + 0.715f * (b - pl_damp_l)); /* about 2.2 kHz */
    b = allpass(L_AP2L, HS(1800), pl_damp_l * PL_DECAY, PL_DD2);
    push(L_D2L, b);
    a = allpass_mod(L_APR, (float)HS(908) + (float)EXC * lfo_c, x + PL_DECAY * fb_r, -0.7f);
    push(L_D1R, a);
    b = tap(L_D1R, HS(4217));
    pl_damp_r = fm_flush(pl_damp_r + 0.715f * (b - pl_damp_r));
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
    w = (1.0f - p_lp) * (1.0f - p_lp) * (1.0f - p_lp);
    p_lpw = 1.0f - w * w; /* the same one-pole at a sixth of the rate (the chorus's and delay's) */
    w = LERP(D_WOW) * (1.0f + (TAPE_WOW - 1.0f) * flav);
    fl = LERP(D_FLUT) * (1.0f + (TAPE_FLUT - 1.0f) * flav);
    p_drive = LERP(D_DRIVE);
#undef LERP
    p_idrive = 1.0f / p_drive;
    wr = WOW_VINYL + (WOW_TAPE - WOW_VINYL) * flav;
    p_wd = w / (CENTS_K * (wr + 0.4f * flav * WOW_DRIFT)); /* depth in samples: the wow and the tape's drift at peak */
    p_fd = fl / (CENTS_K * (FLUT_VINYL + (FLUT_TAPE - FLUT_VINYL) * flav));
    p_grit = fm_clampf(dust - 1.0f, 0.0f, 1.0f);
    p_rate = dust <= 2.0f ? 0.5f : 0.5f - (dust - 2.0f) * (0.5f - 1.0f / 3.0f); /* hold 2 samples, then 3 */
    p_q = fm_exp2f(11.0f - 2.0f * fm_clampf(dust - 2.0f, 0.0f, 1.0f));          /* 12 bits, then 10 */
}

/* the dust's state: its delay line, filters, LFOs and grit cleared (the ramps' targets left alone) */
static void dust_clear(void)
{
    memset(wl, 0, sizeof wl);
    wl_w = 0;
    wl_last = 0.0f;
    wl_on = 0;
    wow_ph = drift_ph = flut_ph = 0.0f;
    del = del_step = 0.0f;
    cr_n = 0;
    sh_ph = sh_y = lp_y = duck_s = dg_step = 0.0f;
    dg = 1.0f;
    gl_acc = gl_p = gl_h = gl_gr = 0.0f;
    gl_g = 1.0f;
}

/* the chorus's line and LFO, the side's filter */
static void chorus_clear(void)
{
    rfx_chorus_clear();
    memset(ch_buf, 0, sizeof ch_buf);
    ch_w = 0;
    ch_s = 0.0f;
    ch_c = 1.0f;
    w_ac = 0.0f;
}

/* the delay silent: its line, loop filter and wow cleared, its time landed */
static void delay_clear(void)
{
    memset(dl_buf, 0, sizeof dl_buf);
    dl_w = 0;
    dl_y = dl_ws = dl_peak = 0.0f;
    dl_wc = 1.0f;
    dl_k = fm_expf(-1.05f / fm_maxf(dl_tt, 1.0f)); /* ln(1 / 0.35) per pass */
    dl_t = dl_tt;
    w_ad = w_ml = w_sl = w_el0 = w_el1 = w_er0 = w_er1 = 0.0f;
}

/* the bed's noise and every state it keeps, restarted from seed (its levels left alone) */
static void bed_clear(uint32_t seed)
{
    int k;
    brng = seed * 2654435761u ^ 0x9E3779B9u;
    if (!brng)
        brng = 0x9E3779B9u;
    for (k = 0; k < 4; k++)
        (void)xs();
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
}

/* the plate silent: its ring, filters and LFO cleared (and Rings' reverb's) */
static void plate_clear(void)
{
    rfx_reverb_clear();
    pw = 0;
    memset(pl_buf, 0, sizeof pl_buf);
    pl_bw = pl_damp_l = pl_damp_r = 0.0f;
    lfo_s = 0.0f;
    lfo_c = 1.0f;
    pl_acc = pl_wl0 = pl_wl1 = pl_wr0 = pl_wr1 = pl_peak = pl_pp = 0.0f;
}

void fx_init(void)
{
    uint32_t b;
    int k;
    dust = dust_t = 0.0f;
    dust_s = flav_s = 0.0f;
    flav = flav_t = 0.0f;
    gl_on = 0;
    gl_mu = 1.0f;
    dust_clear();
    pump_k = 0.12f;
    ch_dep = ch_dep_t = 0.0f;
    chorus_clear();
    dl_tt = 0.75f * DR - DL_LAT;
    delay_clear();
    w_ph = 0;
    dust_params();
    bo = bo_t = lv = lv_t = lt = lt_t = lr = lr_t = 0.0f;
    bg = bg_t; /* (fx_bed_level kept, its ramp landed) */
    bed_clear(0u);
    for (k = 0, b = 0; k < L_N; k++) {
        pl_base[k] = b;
        b += (uint32_t)DL_LEN[k] + 1u;
    }
    plate_clear();
    pl_open = 0;
    og = 1.0f;
    stopping = 0;
}

void fx_song(uint32_t seed, int quiet)
{
    if (bo == 0.0f && bo_t == 0.0f)
        bed_clear(seed);
    else { /* still fading (a replay straight after a stop): reseed the noise, keep the filters running */
        brng = seed * 2654435761u ^ 0x9E3779B9u;
        if (!brng)
            brng = 0x9E3779B9u;
    }
    if (quiet) {
        dust_clear();
        chorus_clear();
        if (pl_peak < PL_FLOOR) /* Rings' 16-bit loop would carry a faint tail's state at its floor for ever */
            plate_clear();
    }
    if (dl_peak <= PL_QUIET) /* the delay's tail over: its line too */
        delay_clear();
    /* a song's modulation and timing are its own (the same seed renders the same samples): the plate's, the chorus's
       and the delay's LFOs, the delay's time (landing at this song's tempo) and the wet loop's phases restart; a
       ringing tail keeps its lines' contents (its read moves by at most the wow's 0.25 ms) */
    lfo_s = 0.0f;
    lfo_c = 1.0f;
    rfx_lfo_restart();
    ch_s = 0.0f;
    ch_c = 1.0f;
    dl_ws = 0.0f;
    dl_wc = 1.0f;
    dl_land = 1;
    w_ph = 0;
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
    if (stopping) { /* a replay inside a stop's fade: the old tails stay gone */
        plate_clear();
        delay_clear();
        chorus_clear();
    }
    pl_open = 1;
    gl_on = 1;
    og = 1.0f;
    stopping = 0;
}

void fx_land(void)
{
    dust = dust_t;
    flav = flav_t;
    dust_params();
    ch_dep = ch_dep_t; /* the chorus's depth too: none of it carries from the previous song's chords */
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

float fx_glue_db(void)
{
    return gl_gr;
}

void fx_glue_off(int off)
{
    gl_off = (uint8_t)(off != 0);
}

int fx_active(void)
{
    return bo > 0.0f || bo_t > 0.0f || pl_peak > PL_QUIET || dl_peak > PL_QUIET;
}

void fx_chorus(float depth)
{
    ch_dep_t = fm_clampf(depth, 0.0f, 1.0f);
}

float fx_chorus_dry(void)
{
    return fx_chorus_dry_at(ch_dep_t);
}

float fx_chorus_dry_at(float depth)
{
    return !pl_open ? 1.0f : rings ? rfx_chorus_dry(depth) : 1.0f / (1.0f + 0.5f * CH_W * depth);
}

void fx_rings(int on)
{
    on = !FX_OURS || on != 0; /* (ours is host-only) */
    if (on != rings) { /* the other pair starts silent */
        rings = (uint8_t)on;
        plate_clear();
        chorus_clear();
    }
}

void fx_tempo(float bpm)
{
    dl_tt = fm_clampf(0.75f * 60.0f / fm_maxf(bpm, 1.0f) * DR - DL_LAT, 2.0f, (float)DL_N - 8.0f);
    /* a song's start lands the time at its tempo (the same seed, the same samples) rather than gliding: a delay tail
       still ringing from the last song jumps to the new time, its repeats re-spaced (heard, rarely: the gap is 2-4 s);
       the trade for determinism */
    if (dl_land || dl_peak <= PL_QUIET)
        dl_t = dl_tt;
    dl_land = 0;
    dl_k = fm_expf(-1.05f / fm_maxf(fm_minf(dl_t, dl_tt), 1.0f));
}

void fx_wet_off(int delay_off)
{
    dl_off = (uint8_t)(delay_off != 0);
}

void fx_fast(int on)
{
    fast_off = (uint8_t)(on == 0);
}

void fx_pump(float db)
{
    pump_k = fm_clampf((1.0f - fm_db2lin(-db)) * PUMP_CAL, 0.0f, 0.9f);
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
    float st[] = {gl_p, gl_h, gl_gr, gl_g, lp_y, sh_y, duck_s, dl_y, w_el0, w_el1, w_er0, w_er1, w_ac, w_ad, w_ml, w_sl, clk_l, clk_r, dp_y1, dp_y2, b_l0, b_l1, b_r0, b_r1, pl_bw, pl_damp_l, pl_damp_r, pl_acc, pl_wl0, pl_wl1,
                  pl_wr0, pl_wr1};
    return rfx_tiny() + tiny(pl_buf, (int)PL_N) + tiny(ch_buf, (int)CH_N) + tiny(dl_buf, (int)DL_N) + tiny(wl, WL_N) + tiny(st, (int)(sizeof st / sizeof st[0])) + tiny(hv_l, 2) +
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
    if (d == 0.0f && del < 1e-3f) /* the wow gone (dust 0): the delay lands at 0, not ever closer (the read stops) */
        del = 0.0f;
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

void fx_guard(int plate_off, int bed_off)
{
    g_plate = (uint8_t)(plate_off != 0);
    if (g_bed != (bed_off != 0)) {
        g_bed = (uint8_t)(bed_off != 0);
        fx_bed_level(bg_u);
    }
}

void fx_bed_level(float g)
{
    bg_u = g;
    g = g_bed ? 0.0f : g;
    bg_t = g;
    bg_s = fm_fabsf(bg_t - bg) * (1.0f / BG_STEPS);
    if (bo == 0.0f && bo_t == 0.0f)
        bg = bg_t;
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
        if (bg != bg_t)
            bg = ramp(bg, bg_t, bg_s);
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
        l1 = ol * bg;
        q1 = or_ * bg;
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

/* a control step of the glue: the last step's mean square (acc) into its level, the gain from it */
static void glue_control(float acc)
{
    float ms = acc * (1.0f / (float)CR), lv, over, gr;
    if (!gl_on || gl_off) {
        gl_p = gl_h = gl_gr = 0.0f;
        gl_g = gl_mu = 1.0f;
        return;
    }
    gl_mu = fm_db2lin(GL_MU);
    gl_p = fm_flush(gl_p + (ms - gl_p) * (ms > gl_p ? GL_ATT : GL_REL_A + (GL_REL_B - GL_REL_A) * gl_h));
    lv = 3.0103f * fm_log2f(gl_p + 1e-12f);
    over = lv - GL_THR;
    gr = over <= -0.5f * GL_KNEE ? 0.0f
         : over >= 0.5f * GL_KNEE ? GL_SLOPE * over
                                  : GL_SLOPE * (over + 0.5f * GL_KNEE) * (over + 0.5f * GL_KNEE) * (0.5f / GL_KNEE);
    gl_h = fm_flush(gl_h + ((gr > 1.0f ? 1.0f : 0.0f) - gl_h) * GL_HOLD); /* (flushed: it decays toward 0) */
    gl_gr = gr;
    gl_g = gl_mu * fm_exp2f(-gr * 0.16609640f);
}

/* the dust on the music in l, in place: control steps of CR samples, the state in locals inside each (l's stores may
   alias the statics, so the compiler would reload them every sample). The fast path (fx_fast): with the wow off (dust
   0: its delay sits at 0) the line is not written, only its newest sample kept; when the wow comes on, the line is
   cleared and that sample put back, which is all it can read before it has written its own (its delay grows by far
   under a sample a sample from 0, and lands at once only when nothing sounds, the line's history silence); the
   saturation in its expanded form, y - K y^3 on y clamped at 1.5 / drive (K = 4/27 drive^2: the same curve, to a
   rounding); grit, at 0 (dust 0-1), skipped on both paths. */
static void dust_run(float *l, uint32_t n, float dt)
{
    uint32_t i = 0;
    while (i < n) {
        uint32_t m, k, w = wl_w;
        float d = del, ds, lp = lp_y, g = dg, gs, sh = sh_ph, shy = sh_y;
        float drive, idrive, grit, rate, q, a, kk, lim;
        int wow, fast = !fast_off;
        if (cr_n == 0) {
            dust_control();
            glue_control(gl_acc);
            gl_acc = 0.0f;
            duck_s += (dt - duck_s) * (dt > duck_s ? DUCK_ATT : DUCK_REL) * (float)CR;
            duck_s = fm_flush(duck_s);
            dg_step = (gl_g * (1.0f - pump_k * duck_s) - dg) * (1.0f / (float)CR);
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
        if (fast && !wow && grit == 0.0f) { /* the fast path: no wow, no grit */
            float y = 0.0f, acc = gl_acc;
            kk = (4.0f / 27.0f) * drive * drive;
            lim = 1.5f * idrive;
            for (k = 0; k < m; k++) {
                y = l[i + k];
                acc += y * y;
                {
                    float u = fm_clampf(y, -lim, lim);
                    u -= kk * u * u * u;
                    lp += a * (u - lp);
                }
                g += gs;
                l[i + k] = lp * g;
            }
            if (m) {
                wl_last = y;
                wl_on = 0;
            }
            gl_acc = acc;
            lp_y = fm_flush(lp);
            dg = g;
            cr_n = (cr_n + m) & (CR - 1);
            i += m;
            continue;
        }
        kk = (4.0f / 27.0f) * drive * drive;
        lim = 1.5f * idrive;
        if (!wl_on) { /* the line comes back: cleared, its newest sample put back */
            memset(wl, 0, sizeof wl);
            wl[(w - 1u) & (WL_N - 1)] = wl_last;
            wl_on = 1;
        }
        for (k = 0; k < m; k++) {
            float y, f, u;
            int j;
            /* wow and flutter (at dust 0 the delay sits at 0: the line is kept, the read skipped) */
            y = l[i + k];
            gl_acc += y * y; /* (the glue's detector) */
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
            if (fast) {
                u = fm_clampf(y, -lim, lim);
                y = u - kk * u * u * u;
            } else {
                u = fm_clampf(y * drive, -1.5f, 1.5f);
                y = u * (1.0f - (4.0f / 27.0f) * u * u) * idrive;
            }
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

/* a fractional read d steps behind the newest sample at w of a line (mask: its length less one, a power of two) */
static inline float rd(const float *b, uint32_t mask, uint32_t w, float d)
{
    uint32_t j = (uint32_t)d;
    float f = d - (float)j, a = b[(w - j) & mask];
    return a + (b[(w - j - 1u) & mask] - a) * f;
}

/* Every wet return in one loop on a 12-sample cycle (its phase ph): the inputs' pair sums in at every second sample;
   the plate stepped at every fourth (on the four samples' mean), the chorus and the delay at every sixth (on the six's
   mean); each output interpolated linearly over its step, a step late, and added to the music in l after its soft
   clip. The plate on the send; the chorus on the chords' bus and the tape delay on its send, their mid and side
   through the dust's low-pass (at their rate) and the duck's gain (its value at the block's end), so they sit in the
   dusted music. The state in locals. */
static void wet_run(float *l, float *r, const float *s2, const float *c2, const float *d2, uint32_t n)
{
    uint32_t i, ph = w_ph, cw = ch_w, dw = dl_w, t = 0;
    float acc = pl_acc, ac = w_ac, ad = w_ad, pk = 0.0f;
    float wl0 = pl_wl0, wl1 = pl_wl1, wr0 = pl_wr0, wr1 = pl_wr1, ml = w_ml, sl = w_sl, lk = p_lpw, g = dg;
    float el0 = w_el0, el1 = w_el1, er0 = w_er0, er1 = w_er1;
    /* the outputs' interpolation as running values and their steps (exact at every step) */
    float pvl, pvr, dpl, dpr, evl, evr, del_, der;
    {
        float f4 = (float)(ph & 3u) * 0.25f, f6 = (float)(ph < 6u ? ph : ph - 6u) * (1.0f / 6.0f);
        dpl = (wl1 - wl0) * 0.25f;
        dpr = (wr1 - wr0) * 0.25f;
        pvl = wl0 + (wl1 - wl0) * f4;
        pvr = wr0 + (wr1 - wr0) * f4;
        del_ = (el1 - el0) * (1.0f / 6.0f);
        der = (er1 - er0) * (1.0f / 6.0f);
        evl = el0 + (el1 - el0) * f6;
        evr = er0 + (er1 - er0) * f6;
    }
    float cs = ch_s, cc = ch_c, dep = ch_dep, dt = dl_t, ws = dl_ws, wc = dl_wc, y = dl_y, dpk = dl_peak;
    float kp = dl_k, open = pl_open ? 1.0f : 0.0f, mu = gl_mu, pin = g_plate ? 0.0f : open, ppk = 0.0f;
    int plate = g_plate ? pl_pp > PL_QUIET : pl_open || pl_peak > PL_QUIET;   /* (the guard's: its tail, then none) */
    int chorus = dep > 0.0f || ch_dep_t > 0.0f;
    int delay = !dl_off && (pl_open || dpk > PL_QUIET);
    int wet = chorus || delay || ml != 0.0f || sl != 0.0f || el1 != 0.0f || er1 != 0.0f || el0 != 0.0f || er0 != 0.0f;
    for (i = 0; i < n; i++) {
        float ol, or_, f, x;
        if (++ph == 12u)
            ph = 0;
        if (!(ph & 1u)) { /* a pair complete: its sums in */
            acc += s2[t];
            ac += c2[t];
            ad += d2[t];
            t++;
        }
        if (!(ph & 3u)) { /* the plate's step */
            wl0 = wl1;
            wr0 = wr1;
            if (plate) {
                if (rings)
                    rfx_reverb((0.25f * pin) * acc, &wl1, &wr1);
                else
                    plate_step((0.25f * pin) * acc, &wl1, &wr1);
                wl1 = fm_flush(wl1 * mu);
                wr1 = fm_flush(wr1 * mu);
                ppk = fm_maxf(ppk, fm_fabsf(wl1) + fm_fabsf(wr1));
            } else
                wl1 = wr1 = 0.0f;
            acc = 0.0f;
            pvl = wl0;
            pvr = wr0;
            dpl = (wl1 - wl0) * 0.25f;
            dpr = (wr1 - wr0) * 0.25f;
        } else {
            pvl += dpl;
            pvr += dpr;
        }
        if (ph == 0u || ph == 6u) { /* the chorus's and the delay's step */
            el0 = el1;
            er0 = er1;
            if (wet) {
                float mid = 0.0f, side = 0.0f;
                if (chorus && rings) { /* Rings' chorus: its wet sides as mid and side */
                    float a, b;
                    dep = dep < ch_dep_t ? fm_minf(dep + CH_SLEW, ch_dep_t) : fm_maxf(dep - CH_SLEW, ch_dep_t);
                    rfx_chorus((open * (1.0f / 6.0f)) * ac, dep, &a, &b);
                    mid = 0.5f * (a + b);
                    side = 0.5f * (a - b);
                } else if (chorus) { /* two taps in quadrature, one a side */
                    float sw, a, b;
                    ch_buf[cw] = fm_flush((open * (1.0f / 6.0f)) * ac);
                    cs += (FM_TWO_PI * CH_HZ / DR) * cc;
                    cc -= (FM_TWO_PI * CH_HZ / DR) * cs;
                    dep = dep < ch_dep_t ? fm_minf(dep + CH_SLEW, ch_dep_t) : fm_maxf(dep - CH_SLEW, ch_dep_t);
                    sw = CH_MOD * (0.4f + 0.6f * dep);
                    a = rd(ch_buf, CH_N - 1u, cw, CH_A + sw * cs);
                    b = rd(ch_buf, CH_N - 1u, cw, CH_B + sw * cc);
                    cw = (cw + 1u) & (CH_N - 1u);
                    mid = (0.5f * CH_W) * dep * (a + b);
                    side = (0.5f * CH_W) * dep * (a - b);
                }
                if (delay) { /* the tape delay: read (with wow), the loop's one-pole, write the send and the feedback */
                    ws += (FM_TWO_PI * DL_HZ / DR) * wc;
                    wc -= (FM_TWO_PI * DL_HZ / DR) * ws;
                    dt += (dl_tt - dt) * DL_GLIDE;
                    y = fm_flush(y + DL_LPK * (rd(dl_buf, DL_N - 1u, dw, dt + DL_WOW * ws) - y));
                    dw = (dw + 1u) & (DL_N - 1u);
                    x = fm_flush((open * (1.0f / 6.0f)) * ad + DL_FB * y);
                    dl_buf[dw] = x;
                    dpk = fm_maxf(fm_fabsf(x), dpk * kp);
                }
                mid += y;
                ml = fm_flush(ml + lk * (mid - ml));
                sl = fm_flush(sl + lk * (side - sl));
                el1 = g * (ml + sl);
                er1 = g * (ml - sl);
            } else
                el1 = er1 = 0.0f;
            ac = ad = 0.0f;
            evl = el0;
            evr = er0;
            del_ = (el1 - el0) * (1.0f / 6.0f);
            der = (er1 - er0) * (1.0f / 6.0f);
        } else {
            evl += del_;
            evr += der;
        }
        ol = pvl + evl;
        or_ = pvr + evr;
        if (stopping) {
            og = fm_maxf(og - STOP_STEP, 0.0f);
            ol *= og;
            or_ *= og;
        }
        f = fm_fabsf(ol) + fm_fabsf(or_);
        pk = f > pk ? f : pk;
        x = clip(l[i]); /* the music's knee; the wet rides over it */
        l[i] = x + ol;
        r[i] = x + or_;
    }
    if (ph & 1u) { /* a pair left open: its part so far carried */
        acc += s2[t];
        ac += c2[t];
        ad += d2[t];
    }
    w_ph = (uint8_t)ph;
    pl_wl0 = wl0;
    pl_wl1 = wl1;
    pl_wr0 = wr0;
    pl_wr1 = wr1;
    pl_acc = fm_flush(acc);
    pl_peak = pk;
    pl_pp = ppk;
    w_ac = fm_flush(ac);
    w_ad = fm_flush(ad);
    w_ml = fm_flush(ml);
    w_sl = fm_flush(sl);
    w_el0 = fm_flush(el0);
    w_el1 = fm_flush(el1);
    w_er0 = fm_flush(er0);
    w_er1 = fm_flush(er1);
    ch_w = cw;
    dl_w = dw;
    ch_s = cs;
    ch_c = cc;
    ch_dep = dep;
    dl_t = dt;
    dl_ws = ws;
    dl_wc = wc;
    dl_y = fm_flush(y);
    dl_peak = fm_flush(dpk);
}

static int wet_active(void)
{
    return pl_open || pl_peak > PL_QUIET || dl_peak > PL_QUIET || w_ml != 0.0f || w_sl != 0.0f || w_el1 != 0.0f ||
           w_er1 != 0.0f;
}

uint32_t fx_phase(void)
{
    return w_ph & 1u;
}

void fx_process_pairs(float *l, float *r, const float *s2, const float *c2, const float *d2, uint32_t n, float duck)
{
    uint32_t i;
    float dt = fm_clampf((duck - DUCK_THR) * (1.0f / (1.0f - DUCK_THR)), 0.0f, 1.0f);
    dust_run(l, n, dt * dt);
    if (wet_active())
        wet_run(l, r, s2, c2, d2, n);
    else
        for (i = 0; i < n; i++)
            l[i] = r[i] = clip(l[i]);
    if (stopping && og == 0.0f) { /* stopped: the plate and the delay shut (their tails left for vx_init to clear) */
        pl_open = 0;
        pl_peak = dl_peak = 0.0f;
        w_ml = w_sl = w_el0 = w_el1 = w_er0 = w_er1 = 0.0f;
    }
    if ((bo > 0.0f || bo_t > 0.0f) && !(g_bed && bg == 0.0f))   /* (the guard's: once faded, not run) */
        bed(l, r, n);
}

void fx_process_buses(float *l, float *r, const float *ch, const float *dl, uint32_t n, float duck)
{
    static float s2[FX_NP], c2[FX_NP], d2[FX_NP];
    uint32_t d = 0;
    while (d < n) { /* in blocks of FX_BLK: the inputs summed per pair, then the chain */
        uint32_t b = n - d < FX_BLK ? n - d : FX_BLK, i, k = fx_phase(), t = 0;
        memset(s2, 0, sizeof s2);
        memset(c2, 0, sizeof c2);
        memset(d2, 0, sizeof d2);
        for (i = 0; i < b; i++) {
            s2[t] += r[d + i];
            c2[t] += ch ? ch[d + i] : 0.0f;
            d2[t] += dl ? dl[d + i] : 0.0f;
            if (++k == FX_PAIR) {
                k = 0;
                t++;
            }
        }
        fx_process_pairs(l + d, r + d, s2, c2, d2, b, duck);
        d += b;
    }
}

void fx_process(float *l, float *r, uint32_t n, float duck)
{
    fx_process_buses(l, r, NULL, NULL, n, duck);
}

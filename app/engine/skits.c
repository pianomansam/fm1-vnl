/* SPDX-License-Identifier: MPL-2.0 */
/* The sampled kits (see skits.h), ported from the instrument spike's sampled drums (dr_*, spike.c): one-shot voices
   reading the pool's ADPCM hits at the kit's pitch (4-point Hermite between decoded samples), a lane's own one-pole
   low-pass, then the kit's bus (two one-pole low-passes and a tanh drive). A voice keeps the kit it started on, and
   its bus, so a kit change leaves sounding hits as they were. Hi-hat choke: any hit on the hat lanes (closed, pedal,
   open) fades what the hi-hat still sounds to -60 dB in 6 ms. Levels as loudness: a hit's gain (samples.h) brings
   its first 150 ms to unit RMS, a lane's level sets it (the spike's: kick 0.30, snare 0.19, hats 0.05 times the
   round-4 trims), the kit's level sets the kit against the synth kits. */
#include <stdint.h>
#include <string.h>
#include "fastmath.h"
#include "library.h"
#include "samples.h"
#include "sampler.h"
#include "skits.h"

#define SR 44100.0f
#define NSV 8                 /* voices: a ride kit's busiest bar sounds about 4 at once (the spike: 3.9) */
#define NL 7                  /* lanes: V_KICK..V_RIDE */
#define NK (KIT_S_CLEAN - KIT_S_WARM + 1)
#define CHOKE_K 0.974231f     /* exp(-ln 1000 / (6 ms * SR)): -60 dB in 6 ms */
#define OFF_THR 1e-4f         /* a fading voice is done under -80 dB */
#define KICK_DEC 0.999652f    /* exp(-ln 1000 / (0.45 s * SR)): the kick's envelope for the duck, the hit's length */
#define GHOST 0.35f           /* the rim lane: the kit's snare at a ghost's level (-9 dB) */
#define VEL_EXP 1.3f          /* a hit's level is (vel / 127) ^ 1.3 (the spike's) */

/* the lanes' levels at full velocity (RMS over a hit's first 150 ms) before the kit's trims and level */
static const float LANE_LVL[NL] = {0.30f, 0.15f, 0.06f, 0.06f, 0.065f, 0.15f * GHOST, 0.06f};

/* The kits (the spec's table; the hits and processing as Sam approved them). The hat trims are the spike's round-4
   pairs (hat_pairs.h: closed, then closed times open); the shaker's is round 4's (1.161); the warm kit's hat is the
   round-3 lineup's raw Gogodze closed (1.135), its open-hat lane the closed hat; its shaker 10 dB up, as the bus's
   3.2 kHz low-pass takes about that much of it (by measurement; Sam's ear to confirm). */
#define SHK 1.161f
static const skit_t KITS[NK] = {
    /* KIT_S_WARM: the Gogodze kick, closed hat and sidestick backbeat, the ride; down 1 st, LP 3.2 kHz, drive 1.2 */
    {{SH_GG_KICK, SH_GG_SIDE, SH_GG_HAT, -1, SH_VC_SHAKER, SH_GG_SIDE, SH_RIDE}, -1.0f, 3200.0f, 1.2f, 0.310f,
     {1.0f, 1.0f, 1.135f, 1.135f, SHK * 3.2f, 1.0f, 1.0f}, {0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0}},
    /* KIT_S_DARK: the Gogodze kick; its snare down 3 st, LP 3.5 kHz; its hats down 3 st, LP 5 kHz, and the ride with
       them (the ride follows each kit's processing: here the cymbals'; elsewhere the kit's bus, or none) */
    {{SH_GG_KICK, SH_GG_SNARE, SH_GG_HAT, SH_GG_OHAT, SH_VC_SHAKER, SH_GG_SNARE, SH_RIDE}, 0.0f, 0.0f, 0.0f, 0.275f,
     {1.0f, 1.0f, 1.072f, 1.072f * 0.75f, SHK, 1.0f, 1.0f}, {0, -3, -3, -3, 0, -3, -3},
     {0, 3500, 5000, 5000, 0, 3500, 5000}},
    /* KIT_S_RUSTY: the Big Rusty kick, Frankensnare 20x6 poplar, Big Rusty foot chick / open (soft) */
    {{SH_BR_KICK, SH_FS_POPLAR, SH_BR_CHICK, SH_BR_OHAT, SH_VC_SHAKER, SH_FS_POPLAR, SH_RIDE}, 0.0f, 0.0f, 0.0f, 0.284f,
     {1.0f, 1.0f, 0.822f, 0.822f * 1.28f, SHK, 1.0f, 1.0f}, {0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0}},
    /* KIT_S_GENTLE: the Big Rusty kick, the VCSL rope snare (snares off), VCSL pedal close / open */
    {{SH_BR_KICK, SH_VC_ROPE, SH_VC_PEDAL, SH_VC_OHAT, SH_VC_SHAKER, SH_VC_ROPE, SH_RIDE}, 0.0f, 0.0f, 0.0f, 0.273f,
     {1.0f, 1.0f, 0.923f, 0.923f * 1.37f, SHK, 1.0f, 1.0f}, {0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0}},
    /* KIT_S_CLEAN: the Gogodze kick, Frankensnare 20x6 poplar, VCSL closed (softest) / open */
    {{SH_GG_KICK, SH_FS_POPLAR, SH_VC_HAT, SH_VC_OHAT, SH_VC_SHAKER, SH_FS_POPLAR, SH_RIDE}, 0.0f, 0.0f, 0.0f, 0.278f,
     {1.0f, 1.0f, 1.012f, 1.012f * 1.23f, SHK, 1.0f, 1.0f}, {0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0}},
};

typedef struct {
    smp_dec_t d;
    float h[4], fr, step, gain; /* the last four decoded samples (h[3] newest), the read fraction, step, gain */
    float lp, lc;               /* the lane's one-pole low-pass (lc 1: none) */
    float fade;                 /* 1, or the choke's fade */
    uint8_t on, lane, kit, ended;
    uint32_t age;
} sv_t;

typedef struct { float lc, l1, l2, drive, idrive; } sbus_t; /* a kit's bus: low-pass coefficient (1: none), states */

static sv_t sv[NSV];
static sbus_t sb[NK];
static int cur = 0;           /* the current kit, 0..NK-1 */
static uint32_t clock_n;
static float kick_env;
static int n_on;              /* voices sounding */
static unsigned ring;         /* kits whose bus still rings (a bit per kit) */

/* Half the rate (the default; skits_half): the hits are 16 kHz, so nothing they hold lies above 8 kHz, and every kit
   pitches down or not at all. The voices and the buses run on the engine's even samples at 22.05 kHz (steps, low-pass
   coefficients and the choke's fall rescaled), then one 27-tap half-band (voices.c's: flat within 0.01 dB to 8 kHz,
   -60 dB from 14 kHz; 14 samples, 0.3 ms, of latency, as the half-rate keys) brings the sum to the full rate. */
#define HB_M 7
#define HB_H (2 * HB_M)
static const float HB_G[HB_M] = {0.628231535f, -0.188138067f, 0.090460645f, -0.045399865f,
                                 0.020987195f, -0.007939147f, 0.001797704f};
static int half = 1;
static float dv = 2.0f, choke_k;  /* a step in output samples (2 at half the rate), the choke's fall per step */
static float hh[HB_H + 129];      /* the half-band's history, then a call's half-rate samples (at most 128 + 1) */
static int hlive;                 /* half-rate samples until the history is silent again */

/* A one-pole low-pass's coefficient (1: none). At half the rate, the one whose gain matches the full rate's at 1.4
   times the cutoff (at most 7 kHz): the impulse-invariant coefficient alone would let about 3 dB more through at 8
   kHz (a two-pole bus's 3.2 kHz), lifting a bright hit's level 0.9 dB; matched there, every octave band of every
   hit stays within 0.7 dB of the full rate's (song_test's test_skits). */
static float lp_coef(float hz)
{
    float a = hz > 0.0f ? 1.0f - fm_expf(-6.2831853f * hz / SR) : 1.0f, w, t, c, b;
    if (hz <= 0.0f || dv < 2.0f)
        return a;
    w = 6.2831853f * fm_minf(1.4f * hz, 7000.0f) / SR;
    b = 1.0f - a;
    t = a * a / (1.0f - 2.0f * b * fm_cosf(w) + b * b); /* the full rate's gain there, squared */
    c = 1.0f - fm_cosf(2.0f * w);                       /* the same frequency at half the rate: a^2 (1 - t) +
                                                           2 t c a - 2 t c = 0 for the coefficient a */
    return (-2.0f * t * c + fm_sqrtf(4.0f * t * t * c * c + 8.0f * (1.0f - t) * t * c)) / (2.0f * (1.0f - t));
}

const skit_t *skits_kit(int kit) { return lib_kit_sampled(kit) ? &KITS[kit - KIT_S_WARM] : 0; }

void skits_set(int kit)
{
    if (lib_kit_sampled(kit))
        cur = kit - KIT_S_WARM;
}

void skits_half(int on)
{
    if ((on != 0) == half) /* (unchanged: hits sounding keep sounding) */
        return;
    half = on != 0;
    skits_clear();
}

void skits_clear(void)
{
    int k;
    dv = half ? 2.0f : 1.0f;
    choke_k = half ? CHOKE_K * CHOKE_K : CHOKE_K;
    memset(sv, 0, sizeof sv);
    memset(hh, 0, sizeof hh);
    hlive = 0;
    for (k = 0; k < NK; k++) {
        sb[k].lc = lp_coef(KITS[k].lp_hz);
        sb[k].l1 = sb[k].l2 = 0.0f;
        sb[k].drive = KITS[k].drive;
        sb[k].idrive = KITS[k].drive > 0.0f ? 1.0f / KITS[k].drive : 0.0f;
    }
    clock_n = 0u;
    kick_env = 0.0f;
    n_on = 0;
    ring = 0u;
}

/* the lane's hit in kit k: an open hat with none plays the closed hat */
static const smp_zone_t *lane_hit(const skit_t *k, int lane)
{
    int h = k->hit[lane];
    if (h < 0 && lane == 3)
        h = k->hit[2];
    return smp_hit(h);
}

static float lane_step(const skit_t *k, int lane, const smp_zone_t *z)
{
    return fm_exp2f((k->semis + (float)k->lsemis[lane]) * (1.0f / 12.0f)) * (float)z->rate * (1.0f / SR);
}

int skits_lane_len(int kit, int voice)
{
    const skit_t *k = skits_kit(kit);
    int lane = voice - V_KICK;
    const smp_zone_t *z;
    if (!k || lane < 0 || lane >= NL || !(z = lane_hit(k, lane)))
        return 0;
    return (int)((float)z->len / lane_step(k, lane, z)) + 1;
}

void skits_on(const song_ev_t *e)
{
    const skit_t *k = &KITS[cur];
    int lane = (int)e->voice - V_KICK, i, best = 0;
    const smp_zone_t *z;
    sv_t *v;
    float vel;
    if (lane < 0 || lane >= NL || !(z = lane_hit(k, lane)))
        return;
    if (lane == 2 || lane == 3) /* the hi-hat: any hit chokes what it still sounds */
        for (i = 0; i < NSV; i++)
            if (sv[i].on && (sv[i].lane == 2 || sv[i].lane == 3))
                sv[i].fade = sv[i].fade < 1.0f ? sv[i].fade : 0.999f;
    for (i = 0; i < NSV; i++) { /* a free voice, else the oldest */
        if (!sv[i].on) {
            best = i;
            break;
        }
        if (sv[i].age < sv[best].age)
            best = i;
    }
    v = &sv[best];
    n_on += !v->on;
    memset(v, 0, sizeof *v);
    smp_dec_init(&v->d, z);
    vel = (float)e->vel * (1.0f / 127.0f);
    v->step = lane_step(k, lane, z) * dv;
    v->gain = LANE_LVL[lane] * k->trim[lane] * k->level * z->gain * (1.0f / 32768.0f) * fm_powf(vel, VEL_EXP);
    v->lc = lp_coef((float)k->llp[lane]);
    v->fr = 1.0f - (v->step - v->step / dv); /* fill the interpolator at once (at half the rate, a full-rate step
                                                 behind: its steps fall on the full rate's even samples) */
    v->fade = 1.0f;
    v->on = 1;
    v->lane = (uint8_t)lane;
    v->kit = (uint8_t)cur;
    v->age = clock_n;
    if (lane == 0)
        kick_env = vel;
}

/* Up to n steps of voice v added into acc, until it ends: the number of steps it ticked (the one it ended on
   included). Its state in locals for the run (acc could alias it otherwise, and every step would reload it). Per
   step: the read position moves on by the step, decoding the hit's next samples as it passes them; 4-point Hermite
   between the last four; the choke's fade; the lane's low-pass. */
static uint32_t sv_run(sv_t *v, float *acc, uint32_t n)
{
    smp_dec_t dec = v->d;
    float h0 = v->h[0], h1 = v->h[1], h2 = v->h[2], h3 = v->h[3], f = v->fr, st = v->step, g = v->gain;
    float fade = v->fade, lp = v->lp, lc = v->lc, ck = choke_k;
    uint32_t s, len = dec.z->len;
    unsigned ended = v->ended;
    int on = 1;
    for (s = 0; s < n; s++) {
        float y;
        f += st;
        while (f >= 1.0f) {
            f -= 1.0f;
            h0 = h1;
            h1 = h2;
            h2 = h3;
            h3 = (float)smp_dec_fast(&dec);
            if (dec.ni >= len && ++ended > 4u) { /* played out (the interpolator's zeros too) */
                on = 0;
                break;
            }
        }
        if (!on) {
            acc[s] += 0.0f;
            s++;
            break;
        }
        {
            float c1 = 0.5f * (h2 - h0), c2 = h0 - 2.5f * h1 + 2.0f * h2 - 0.5f * h3,
                  c3 = 0.5f * (h3 - h0) + 1.5f * (h1 - h2);
            y = (((c3 * f + c2) * f + c1) * f + h1) * g;
        }
        if (fade < 1.0f) {
            y *= fade;
            fade *= ck;
            if (fade < OFF_THR)
                on = 0;
        }
        if (lc < 1.0f)
            y = lp = fm_flush(lp + (y - lp) * lc);
        acc[s] += y;
        if (!on) {
            s++;
            break;
        }
    }
    v->d = dec;
    v->h[0] = h0;
    v->h[1] = h1;
    v->h[2] = h2;
    v->h[3] = h3;
    v->fr = f;
    v->fade = fade;
    v->lp = lp;
    v->ended = (uint8_t)ended;
    if (!on) {
        v->on = 0;
        n_on--;
    }
    return s;
}

/* n steps of every sounding hit, through their kits' buses, into out (written). Each voice renders its run in one
   loop (its state in registers), summed into its kit's buffer in voice order; each kit's bus then runs over it in kit
   order: at the full rate, the same sums in the same order as a sample at a time. Returns the steps a kick sounded
   (after each step's tick). */
#define SK_MAX 256 /* the longest call (voices.c's chunk) */
static uint32_t render_steps(float *out, uint32_t n)
{
    static float acc[SK_MAX];
    uint32_t uend[NK] = {0}, kend = 0, s, e;
    int i, k;
    memset(out, 0, n * sizeof(float));
    for (k = 0; k < NK; k++) {
        sbus_t *b = &sb[k];
        int any = 0;
        for (i = 0; i < NSV; i++) {
            sv_t *v = &sv[i];
            if (!v->on || v->kit != k)
                continue;
            if (!any) {
                memset(acc, 0, n * sizeof(float));
                any = 1;
            }
            s = sv_run(v, acc, n);
            uend[k] = s > uend[k] ? s : uend[k]; /* (the steps it ticked: the bus's "used" ones) */
            if (v->lane == 0)
                kend = s > kend ? s : kend;
        }
        e = uend[k];
        if (b->lc < 1.0f) { /* (run while it rings, so a hit's tail through it stays the same) */
            if (!e && !(ring & 1u << k))
                continue;
            float l1 = b->l1, l2 = b->l2, lc = b->lc, dr = b->drive, idr = b->idrive; /* (in locals: out could
                                                                                     alias the bus) */
            for (s = 0; s < n; s++) {
                float x = s < e ? acc[s] : 0.0f;
                if (s >= e && fm_fabsf(l1) < 1e-7f && fm_fabsf(l2) < 1e-7f) {
                    l1 = l2 = 0.0f; /* rung out */
                    break;
                }
                l1 = fm_flush(l1 + (x - l1) * lc);
                l2 = fm_flush(l2 + (l1 - l2) * lc);
                x = l2;
                if (dr > 0.0f)
                    x = fm_tanhf(x * dr) * idr;
                out[s] += x;
            }
            b->l1 = l1;
            b->l2 = l2;
            if (s < n) /* (rung out) */
                ring &= ~(1u << k);
            else
                ring |= 1u << k;
        } else if (b->drive > 0.0f)
            for (s = 0; s < e; s++)
                out[s] += fm_tanhf(acc[s] * b->drive) * b->idrive;
        else
            for (s = 0; s < e; s++)
                out[s] += acc[s];
    }
    return kend;
}

/* the kick's envelope (the duck's source) over n output samples, a kick sounding for the first kn of them */
static void kick_run(uint32_t kn, uint32_t n)
{
    uint32_t s;
    if (kick_env > 0.0f) {
        for (s = 0; s < kn && s < n; s++)
            kick_env = fm_flush(kick_env * KICK_DEC);
        if (kn < n)
            kick_env = 0.0f;
    }
}

void skits_render(float *out, uint32_t n)
{
    uint32_t c0 = clock_n, nm, i, kn;
    float *x = hh + HB_H; /* x[m]: the call's m-th half-rate sample; x[-1] .. x[-HB_H] the history */
    int m = 0, busy = n_on || ring;
    clock_n += n;
    if (!half) {
        if (!busy) { /* idle: nothing to run (a synth kit's song pays nothing here) */
            memset(out, 0, n * sizeof(float));
            kick_env = 0.0f;
            return;
        }
        kick_run(render_steps(out, n), n);
        return;
    }
    nm = (n + 1u - (c0 & 1u)) >> 1; /* the call's even samples (a half-rate step each) */
    if (!busy && hlive <= 0) {
        memset(out, 0, n * sizeof(float));
        kick_env = 0.0f;
        return;
    }
    if (busy) {
        kn = render_steps(x, nm);
        hlive = HB_H + 1;
        kick_run(kn < nm ? 2u * kn : n, n);
    } else {
        memset(x, 0, nm * sizeof(float));
        hlive -= (int)nm;
        kick_env = 0.0f;
    }
    { /* the odd samples (halfway between x[m-1-HB_M] and x[m-HB_M]) for every m at once, a tap at a time (so the
         loop vectorizes; each sum still adds its taps in order), then the even ones (x[m - HB_M]) between them */
        static float yo[129];
        uint32_t no = n - nm, t, m0 = c0 & 1u ? 0u : 1u;
        int k;
        const float *q = x + m0 - HB_M;
        for (t = 0; t < no; t++)
            yo[t] = 0.0f;
        for (k = 0; k < HB_M; k++) {
            float g = HB_G[k];
            const float *a = q + k, *b = q - 1 - k;
            for (t = 0; t < no; t++)
                yo[t] += g * (a[t] + b[t]);
        }
        for (i = 0, t = 0; i < n; i++)
            if (((c0 + i) & 1u) == 0u)
                out[i] = x[m++ - HB_M];
            else
                out[i] = yo[t++];
    }
    memmove(hh, hh + nm, HB_H * sizeof(float));
}

float skits_sample(void)
{
    float y;
    skits_render(&y, 1u);
    return y;
}

void skits_release(void)
{
    int i;
    for (i = 0; i < NSV; i++)
        if (sv[i].on && sv[i].fade >= 1.0f)
            sv[i].fade = 0.999f;
}

int skits_active(void)
{
    int i, c = 0;
    for (i = 0; i < NSV; i++)
        c += sv[i].on;
    for (i = 0; i < NK; i++) /* a bus still ringing counts as one */
        c += sb[i].l1 != 0.0f || sb[i].l2 != 0.0f;
    c += half && hlive > 0; /* (the half-band's history still ringing out) */
    return c;
}

float skits_kick_env(void) { return kick_env; }

static int tiny(float x) { return x != 0.0f && fm_fabsf(x) < 1e-20f; }

int skits_tiny(void)
{
    int i, c = tiny(kick_env);
    for (i = 0; i < NSV; i++)
        c += sv[i].on && (tiny(sv[i].lp) || tiny(sv[i].fade));
    for (i = 0; i < NK; i++)
        c += tiny(sb[i].l1) + tiny(sb[i].l2);
    return c;
}

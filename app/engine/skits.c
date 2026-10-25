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
    /* KIT_S_DARK: the Gogodze kick; its snare down 3 st, LP 3.5 kHz; its hats down 3 st, LP 5 kHz */
    {{SH_GG_KICK, SH_GG_SNARE, SH_GG_HAT, SH_GG_OHAT, SH_VC_SHAKER, SH_GG_SNARE, SH_RIDE}, 0.0f, 0.0f, 0.0f, 0.275f,
     {1.0f, 1.0f, 1.072f, 1.072f * 0.75f, SHK, 1.0f, 1.0f}, {0, -3, -3, -3, 0, -3, 0},
     {0, 3500, 5000, 5000, 0, 3500, 0}},
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

static float lp_coef(float hz) { return hz > 0.0f ? 1.0f - fm_expf(-6.2831853f * hz / SR) : 1.0f; }

const skit_t *skits_kit(int kit) { return lib_kit_sampled(kit) ? &KITS[kit - KIT_S_WARM] : 0; }

void skits_set(int kit)
{
    if (lib_kit_sampled(kit))
        cur = kit - KIT_S_WARM;
}

void skits_clear(void)
{
    int k;
    memset(sv, 0, sizeof sv);
    for (k = 0; k < NK; k++) {
        sb[k].lc = lp_coef(KITS[k].lp_hz);
        sb[k].l1 = sb[k].l2 = 0.0f;
        sb[k].drive = KITS[k].drive;
        sb[k].idrive = KITS[k].drive > 0.0f ? 1.0f / KITS[k].drive : 0.0f;
    }
    clock_n = 0u;
    kick_env = 0.0f;
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
    memset(v, 0, sizeof *v);
    smp_dec_init(&v->d, z);
    vel = (float)e->vel * (1.0f / 127.0f);
    v->step = lane_step(k, lane, z);
    v->gain = LANE_LVL[lane] * k->trim[lane] * k->level * z->gain * (1.0f / 32768.0f) * fm_powf(vel, VEL_EXP);
    v->lc = lp_coef((float)k->llp[lane]);
    v->fr = 1.0f; /* fill the interpolator at once */
    v->fade = 1.0f;
    v->on = 1;
    v->lane = (uint8_t)lane;
    v->kit = (uint8_t)cur;
    v->age = clock_n;
    if (lane == 0)
        kick_env = vel;
}

static inline float sv_tick(sv_t *v)
{
    float f = v->fr + v->step, a, b, c, d, y;
    while (f >= 1.0f) {
        f -= 1.0f;
        v->h[0] = v->h[1];
        v->h[1] = v->h[2];
        v->h[2] = v->h[3];
        v->h[3] = (float)smp_dec_fast(&v->d);
        if (v->d.ni >= v->d.z->len && ++v->ended > 4u) { /* played out (the interpolator's zeros too) */
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
        y = (((c3 * f + c2) * f + c1) * f + b) * v->gain;
    }
    if (v->fade < 1.0f) {
        y *= v->fade;
        v->fade *= CHOKE_K;
        if (v->fade < OFF_THR)
            v->on = 0;
    }
    if (v->lc < 1.0f)
        y = v->lp = fm_flush(v->lp + (y - v->lp) * v->lc);
    return y;
}

float skits_sample(void)
{
    float acc[NK] = {0.0f}, out = 0.0f;
    unsigned used = 0u;
    int i, k;
    clock_n++;
    for (i = 0; i < NSV; i++)
        if (sv[i].on) {
            float y = sv_tick(&sv[i]);
            k = sv[i].kit;
            if (!(used & 1u << k)) {
                used |= 1u << k;
                acc[k] = 0.0f;
            }
            acc[k] += y;
        }
    for (k = 0; k < NK; k++) {
        sbus_t *s = &sb[k];
        float x = used & 1u << k ? acc[k] : 0.0f;
        if (s->lc < 1.0f) { /* (run while it rings, so a hit's tail through it stays the same) */
            if (!(used & 1u << k) && fm_fabsf(s->l1) < 1e-7f && fm_fabsf(s->l2) < 1e-7f) {
                s->l1 = s->l2 = 0.0f; /* rung out */
                continue;
            }
            s->l1 = fm_flush(s->l1 + (x - s->l1) * s->lc);
            s->l2 = fm_flush(s->l2 + (s->l1 - s->l2) * s->lc);
            x = s->l2;
        } else if (!(used & 1u << k))
            continue;
        if (s->drive > 0.0f)
            x = fm_tanhf(x * s->drive) * s->idrive;
        out += x;
    }
    if (kick_env > 0.0f) {
        int on = 0;
        for (i = 0; i < NSV; i++)
            on |= sv[i].on && sv[i].lane == 0;
        kick_env = on ? fm_flush(kick_env * KICK_DEC) : 0.0f;
    }
    return out;
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

/* SPDX-License-Identifier: GPL-3.0-only */
/* Adapted from FOMAJAM's src/drums.c (FOMAJAM 138c5d9, GPL-3.0-only: LICENSE; by FOMAJAM's authors, its
   LICENSING.md), whose modeled machines are ported from trommelsynthesizer in poketrack (zlib: LICENSE.poketrack).
   Altered for VNL-1: the modeled machines only (808, 909, SDS-V, CR-78; the ROM recordings, the store's kits, the
   samples, the synths and the tracks left out), a voice rendered whole, once (the drum lanes play it back, wb_drums.c),
   its knobs at the middle (TONE, EXTRA, DECAY, TUNE 64; LEVEL and velocity full), its noise from a fixed seed, its sine (FOMAJAM's
   platform/common.c) here, and every 64-bit multiply in a helper of its own, out of line on the FM-1 (a multiply
   inlined beside a load may leave a register a bit off there: base/bsp/docs/CPU.md). This is not the original
   software.

   FOMAJAM's own words: voices built from a few voice types (tools/drums.py, from trommelsynthesizer). This code runs
   in audio(): integers only. */
#include "fj_drums.h"
#include "fj_tables.h"
#include "gains.h"

#define FULL (1 << 30) /* an envelope at its top, Q30 */
#define HZ_INC 97392u  /* phase step of 1 Hz: 2^32 / 44100 */

#ifdef FELUCCA_ID
#define MUL64 __attribute__((noinline)) static
#else
#define MUL64 static inline
#endif
MUL64 int32_t mul_sh(int32_t a, int32_t b, int sh) { return (int32_t)(((int64_t)a * b) >> sh); }
MUL64 int32_t mul_u31(int32_t a, uint32_t c) { return (int32_t)(((int64_t)a * c) >> 31); }
MUL64 uint32_t mul_uu(uint32_t a, uint32_t b, int sh) { return (uint32_t)(((uint64_t)a * b) >> sh); }
MUL64 uint32_t inc_mod(uint32_t inc, int32_t amt) { return inc + (uint32_t)(((int64_t)inc * amt) >> 12); }
MUL64 int64_t mul_wide(int32_t a, int32_t b) { return (int64_t)a * b; }
/* (a wa + b wb) >> sh: the two products each in mul_wide (two in one helper still sat beside a load) */
static inline int32_t mix_sh(int32_t a, int32_t wa, int32_t b, int32_t wb, int sh)
{
    return (int32_t)((mul_wide(a, wa) + mul_wide(b, wb)) >> sh);
}

static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static int32_t sine(uint32_t phase) /* a refined parabola, ~0.1 % (FOMAJAM's platform/common.c) */
{
    int32_t x = (int32_t)phase >> 16, ax = x < 0 ? -x : x, y, ay;
    y = (x * (32768 - ax)) >> 13; /* 4x(1-|x|), Q15 */
    ay = y < 0 ? -y : y;
    y += ((((y * ay) >> 15) - y) * 7373) >> 15; /* + 0.225 (y|y| - y) */
    return y > 32767 ? 32767 : y < -32767 ? -32767 : y;
}

/* ---- building blocks */
typedef struct {
    int32_t a1, a2, a3, k, ic1, ic2; /* Q24 */
} svf_t;

static void svf_set(svf_t *s, int row, int f)
{
    const int32_t *c = &SVF[(row * 128 + clamp(f, 0, 127)) * 3];
    s->a1 = c[0];
    s->a2 = c[1];
    s->a3 = c[2];
    s->k = SVF_K[row];
    s->ic1 = s->ic2 = 0;
}

/* the trapezoidal state-variable filter: stable at any frequency */
static inline int32_t svf_tick(svf_t *s, int32_t x, int32_t *bp, int32_t *hp)
{
    int32_t v3 = x - s->ic2;
    int32_t v1 = mix_sh(s->a1, s->ic1, s->a2, v3, 24);
    int32_t v2 = s->ic2 + mix_sh(s->a2, s->ic1, s->a3, v3, 24);
    s->ic1 = 2 * v1 - s->ic1;
    s->ic2 = 2 * v2 - s->ic2;
    *bp = v1;
    if (hp)
        *hp = x - mul_sh(s->k, v1, 24) - v2;
    return v2;
}

static inline int32_t decay(int32_t env, uint32_t coef) { return mul_u31(env, coef); }
static inline int32_t scale(int32_t x, int32_t env) { return mul_sh(x, env, 30); }

static int32_t tanh_q15(int32_t u) /* u: Q12 */
{
    int32_t a = u < 0 ? -u : u, y;
    if (a >= (8 << 12)) {
        y = 32767;
    } else {
        int i = a >> 7, fr = a & 127;
        y = TANH[i] + ((TANH[i + 1] - TANH[i]) * fr >> 7);
    }
    return u < 0 ? -y : y;
}

/* a phase step moved by semitones */
static uint32_t tuned(uint32_t inc, int semis)
{
    int s = semis + 48, oct = s / 12 - 4;
    uint32_t r = mul_uu(inc, SEMI[s % 12], 16);
    return oct >= 0 ? r << oct : r >> -oct;
}

static uint32_t xr = 2463534242u;
static inline int32_t noise(void)
{
    xr ^= xr << 13;
    xr ^= xr >> 17;
    xr ^= xr << 5;
    return (int32_t)xr >> 16; /* +-32767 */
}

/* ---- voices */
typedef struct {
    const patch_t *p;
    uint32_t ph[6], inc[6], t, end;
    int32_t env[5];
    uint32_t coef[5];
    svf_t s1, s2;
    int32_t lp1, lp2, ca;
    int32_t m[6]; /* amounts, per kind */
    int32_t gain;
    uint8_t kind, on;
} dv_t;

static dv_t rv;   /* the voice being rendered (fj_voice_begin) */
static int32_t acc[256];

static const uint16_t METAL_HZ10[6] = {2053, 3044, 3696, 5227, 5400, 8000};

static void env_start(dv_t *v, int i, int tau)
{
    v->env[i] = FULL;
    v->coef[i] = TAU[clamp(tau, 0, 255)];
}

/* the knobs at the middle: TONE, EXTRA, DECAY, TUNE 64 */
#define TN 64
#define EX 64

static void voice_start(dv_t *v, int m, int voice)
{
    const patch_t *p = &PATCH[m][voice];
    int tn = TN, ex = EX, ds = 0, semis = 0, i;
    v->t = 0;
    v->lp1 = v->lp2 = 0;
    for (i = 0; i < 6; i++)
        v->ph[i] = 0;
    v->p = p;
    v->kind = p->kind;
    switch (p->kind) {
    case K_SWEPT: {
        int td = p->k[4];
        v->inc[0] = tuned(p->hz[0] * HZ_INC, semis);
        env_start(v, 0, p->t[0]);      /* the sweep */
        env_start(v, 1, p->t[1] + ds); /* the body */
        env_start(v, 2, TAU_CLICK);
        v->m[0] = p->k[0];
        v->m[1] = p->k[1] * (ex * 64) >> 12; /* click * 2 * EXTRA */
        if (!td)
            v->m[1] = v->m[1] * (tn * 64) >> 12; /* * 2 * TONE */
        v->ca = ONEPOLE[clamp(p->f[0] + (td ? 0 : (tn - 64) * 36 / 128), 0, 127)];
        v->m[2] = td ? p->k[2] * (1638 + tn * 39) >> 12 : p->k[2]; /* drive */
        v->m[3] = p->k[3];
        v->m[4] = (1 << 27) / tanh_q15(v->m[2]); /* 1 / tanh(drive), Q12 */
        break;
    }
    case K_SNARE:
        v->inc[0] = tuned(p->hz[0] * HZ_INC, semis);
        v->inc[1] = tuned(p->hz[1] * HZ_INC, semis);
        env_start(v, 0, p->t[0] + ds);
        env_start(v, 1, p->t[1] + ds);
        env_start(v, 2, p->t[2] + ds + SNAPPY[ex]); /* the noise */
        env_start(v, 3, TAU_DROP);
        env_start(v, 4, TAU_SNAP);
        svf_set(&v->s1, p->qc, p->f[0]);
        v->ca = ONEPOLE[clamp(p->f[1] + (tn - 64) * 24 / 128, 0, 127)];
        v->m[0] = p->k[0];
        v->m[1] = p->k[1] * (1229 + ex * 45) >> 12; /* noise * (0.3 + 1.4 EXTRA) */
        v->m[2] = p->k[2];
        v->m[3] = 2048 + tn * 32; /* the second drum's balance */
        break;
    case K_METAL:
        for (i = 0; i < 6; i++)
            v->inc[i] = tuned(mul_uu(METAL_HZ10[i] * (HZ_INC / 10), (uint32_t)p->k[0], 12), semis);
        svf_set(&v->s1, p->qc, p->f[0] + semis);
        svf_set(&v->s2, 0, p->f[1] + (tn - 64) * 14 / 128);
        env_start(v, 0, p->t[0] + ds);
        env_start(v, 1, p->t[1] + ds);
        env_start(v, 2, TAU_TICK);
        v->m[0] = clamp(p->k[1] * (1638 + tn * 39) >> 12, 0, 4096); /* the slow decay's share */
        v->m[1] = p->k[2];
        v->m[2] = p->k[3];
        break;
    case K_PING:
        v->inc[0] = tuned(p->hz[0] * HZ_INC, semis);
        v->inc[1] = tuned(p->hz[1] * HZ_INC, semis);
        env_start(v, 0, p->t[0] + ds);
        env_start(v, 1, p->t[1] + ds);
        env_start(v, 2, p->t[2] + ds);
        if (p->k[4])
            svf_set(&v->s1, p->qc, p->f[0] + semis);
        v->m[0] = p->k[0] * (1638 + tn * 39) >> 12;
        v->m[1] = p->k[1];
        v->m[4] = (1 << 27) / tanh_q15(p->k[1]);
        break;
    case K_CLAP:
        svf_set(&v->s1, p->qc, p->f[0] + (tn - 64) * 12 / 128);
        svf_set(&v->s2, 0, p->f[1]);
        env_start(v, 0, p->t[0]);
        env_start(v, 1, p->t[1] + ds);
        v->end = p->hz[0] * p->k[0]; /* the bursts end here */
        break;
    case K_NOISE:
        svf_set(&v->s1, 0, p->f[0] + (tn - 64) * 12 / 128);
        env_start(v, 0, p->t[0]);
        env_start(v, 1, p->t[1] + ds);
        break;
    }
}

/* one voice into out for n frames: the frames written (fewer than n once it has ended: v->on 0) */
static uint32_t voice_render(dv_t *v, int32_t *out, uint32_t n)
{
    const patch_t *p = v->p;
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t x = 0, quiet = 0, bp, hp;
        switch (v->kind) {
        case K_SWEPT: {
            int32_t sweep = v->m[0] * (v->env[0] >> 15) >> 15; /* Q12 */
            int32_t e, nz;
            v->ph[0] += inc_mod(v->inc[0], sweep);
            e = v->env[1];
            nz = noise();
            x = scale(sine(v->ph[0]), e);
            v->lp1 += mul_sh(v->ca, nz - v->lp1, 16);
            x += scale(v->lp1, v->env[2]) * v->m[1] >> 12;
            if (v->m[3]) {
                v->lp2 += (nz - v->lp2) / 5;
                x += scale(v->lp2, scale(e, e)) * v->m[3] >> 12;
            }
            x = tanh_q15(x * v->m[2] >> 15) * v->m[4] >> 12;
            v->env[0] = decay(v->env[0], v->coef[0]);
            v->env[1] = decay(e, v->coef[1]);
            v->env[2] = decay(v->env[2], v->coef[2]);
            quiet = v->t > 441 && e < (FULL >> 11);
            break;
        }
        case K_SNARE: {
            int32_t drop = v->m[0] * (v->env[3] >> 15) >> 15; /* Q12 */
            int32_t body;
            int j;
            v->ph[0] += inc_mod(v->inc[0], drop);
            v->ph[1] += inc_mod(v->inc[1], drop);
            body = scale(sine(v->ph[0]), v->env[0]) * 3 / 4 + (scale(sine(v->ph[1]), v->env[1]) * v->m[3] >> 13);
            svf_tick(&v->s1, noise(), &bp, &hp);
            v->lp1 += mul_sh(v->ca, hp - v->lp1, 16);
            x = body * 3 / 5 + (scale(v->lp1, v->env[2]) * v->m[1] >> 13);
            x += scale(noise(), v->env[4]) * v->m[2] >> 12;
            for (j = 0; j < 5; j++)
                v->env[j] = decay(v->env[j], v->coef[j]);
            quiet = v->t > 882 && v->env[0] < (FULL >> 11) && v->env[1] < (FULL >> 11) && v->env[2] < (FULL >> 11);
            break;
        }
        case K_METAL: {
            int c = 0, j;
            int32_t s, nz = v->m[1], e;
            for (j = 0; j < 6; j++) {
                v->ph[j] += v->inc[j];
                c += (int32_t)v->ph[j] < 0 ? -1 : 1;
            }
            s = c * 10922;
            x = (s * (4096 - nz) >> 12) + (noise() * nz * 4 / 5 >> 12);
            svf_tick(&v->s1, x, &bp, 0);
            svf_tick(&v->s2, bp * 3 / 2 + x * 3 / 10, &bp, &hp);
            e = mix_sh(v->env[0], 4096 - v->m[0], v->env[1], v->m[0], 12);
            x = scale(hp + (scale(noise(), v->env[2]) * v->m[2] >> 13), e);
            v->env[0] = decay(v->env[0], v->coef[0]);
            v->env[1] = decay(v->env[1], v->coef[1]);
            v->env[2] = decay(v->env[2], v->coef[2]);
            quiet = v->t > 441 && e < (FULL >> 12);
            break;
        }
        case K_PING: {
            int32_t e;
            int j;
            v->ph[0] += v->inc[0];
            v->ph[1] += v->inc[1];
            if (p->k[2]) { /* two square waves, band-passed */
                x = ((int32_t)v->ph[0] < 0 ? -32767 : 32767) + (((int32_t)v->ph[1] < 0 ? -32767 : 32767) * v->m[0] >> 12);
                e = mix_sh(v->env[0], 4096 - p->k[3], v->env[2], p->k[3], 12);
                if (p->k[4]) {
                    svf_tick(&v->s1, x, &bp, 0);
                    x = bp;
                }
                x = scale(x, e);
            } else { /* two sines, driven */
                x = scale(sine(v->ph[0]), v->env[0]) + (scale(sine(v->ph[1]), v->env[1]) * v->m[0] >> 12);
                x = tanh_q15(x * v->m[1] >> 15) * v->m[4] >> 12;
                e = v->env[0] > v->env[1] ? v->env[0] : v->env[1];
            }
            for (j = 0; j < 3; j++)
                v->env[j] = decay(v->env[j], v->coef[j]);
            quiet = v->t > 220 && e < (FULL >> 11);
            break;
        }
        case K_CLAP: {
            int32_t e;
            if (v->t < v->end) { /* the bursts: each starts loud */
                if (v->t % p->hz[0] == 0)
                    v->env[0] = FULL;
                e = v->env[0];
            } else {
                if (v->t == v->end)
                    v->env[0] = v->env[1] = FULL;
                e = v->env[0] + mul_sh(v->env[1], p->k[1], 12);
                v->env[1] = decay(v->env[1], v->coef[1]);
            }
            v->env[0] = decay(v->env[0], v->coef[0]);
            svf_tick(&v->s1, noise(), &bp, 0);
            svf_tick(&v->s2, bp, &bp, &hp);
            x = scale(hp, e);
            quiet = v->t > v->end + 441 && e < (FULL >> 11);
            break;
        }
        case K_NOISE: {
            int32_t e;
            svf_tick(&v->s1, noise(), &bp, &hp);
            e = scale(FULL - v->env[0], v->env[1]);
            x = scale(hp, e);
            v->env[0] = decay(v->env[0], v->coef[0]);
            v->env[1] = decay(v->env[1], v->coef[1]);
            quiet = v->t > 220 && e < (FULL >> 11);
            break;
        }
        default:
            v->on = 0;
            return i;
        }
        v->t++;
        out[i] = mul_sh(x, v->gain, 15);
        if (quiet) {
            v->on = 0;
            return i + 1;
        }
    }
    return n;
}

int fj_has(int m, int v) { return m >= 0 && m < M_MODELLED && v >= 0 && v < V_COUNT && PATCH[m][v].kind != K_NONE; }

void fj_voice_begin(int m, int v)
{
    xr = 2463534242u ^ (2654435761u * (uint32_t)(m * V_COUNT + v + 1));   /* (a hit's noise: the same every time) */
    rv.on = 0;
    if (!fj_has(m, v))
        return;
    voice_start(&rv, m, v);
    rv.gain = mul_sh(127 * 127 * 2, GAIN[m][v], 15); /* (FOMAJAM's, at LEVEL and velocity 127) */
    rv.on = 1;
}

uint32_t fj_voice_next(int16_t *out, uint32_t n)
{
    uint32_t i, k = 0;
    while (rv.on && k < n) {
        uint32_t c = n - k < 256u ? n - k : 256u, w = voice_render(&rv, acc, c);
        for (i = 0; i < w; i++)
            out[k + i] = (int16_t)clamp(acc[i], -32767, 32767);
        k += w;
    }
    return k;
}

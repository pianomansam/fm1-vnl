/* SPDX-License-Identifier: MPL-2.0 */
/* The sampler voice (see sampler.h), from the instrument spike's sv_* (branch spike/instruments): IMA ADPCM decode,
   4-point Hermite, a one-pole tone whose cutoff falls after the strike, the envelope, a delayed vibrato. */
#include <string.h>
#include "fastmath.h"
#include "sampler.h"

#define SR 44100.0f
#define T60 6.9078f       /* ln 1000: a time to -60 dB */
#define ENV_OFF 1e-4f     /* the envelope's floor (-80 dB): the voice is done under it */
#define TONE_MAX 7500.0f  /* the tone's highest cutoff */
#define TONE_CR 32u       /* the tone's fall: its coefficient moves every TONE_CR samples */
#define MUTE_TONE 2.5f    /* a muted note (the strum's chuck): its cutoff at this multiple of the note ... */
#define MUTE_DEC 0.06f    /* ... falling to -60 dB in 60 ms ... */
#define MUTE_REL 0.03f    /* ... and released in 30 ms */
#define VIB_RATE 5.0f     /* the vibrato as the winds': 5 Hz, waiting 0.3 s, then fading in over 0.6 s */
#define VIB_DELAY 0.3f
#define VIB_FADE 0.6f
#define CENT 0.000577790f /* 2^(1/1200) - 1: a cent's step */

static const int16_t STEP[89] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,    21,    23,    25,    28,
    31,    34,    37,    41,    45,    50,    55,    60,    66,    73,    80,    88,    97,    107,   118,
    130,   143,   157,   173,   190,   209,   230,   253,   279,   307,   337,   371,   408,   449,   494,
    544,   598,   658,   724,   796,   876,   963,   1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,
    2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,
    9493,  10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
static const int8_t IDXT[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

void smp_dec_init(smp_dec_t *d, const smp_zone_t *z)
{
    d->z = z;
    d->ni = 0;
    d->pred = 0;
    d->idx = z->idx0 > 88 ? 88 : z->idx0;
    d->lpred = 0;
    d->lidx = d->idx;
}

static inline int dec_next(smp_dec_t *d)
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
    code = (z->data[d->ni >> 1] >> ((d->ni & 1u) * 4u)) & 15;
    step = STEP[d->idx];
    diff = step >> 3;
    if (code & 4)
        diff += step;
    if (code & 2)
        diff += step >> 1;
    if (code & 1)
        diff += step >> 2;
    d->pred += code & 8 ? -diff : diff;
    d->pred = d->pred < -32768 ? -32768 : d->pred > 32767 ? 32767 : d->pred;
    d->idx += IDXT[code & 7];
    d->idx = d->idx < 0 ? 0 : d->idx > 88 ? 88 : d->idx;
    d->ni++;
    return (int)d->pred;
}

int smp_dec_next(smp_dec_t *d) { return dec_next(d); }

const smp_zone_t *samp_zone(const smp_bank_t *b, float note)
{
    int k;
    for (k = 0; k + 1 < b->n; k++) /* the first zone whose midpoint to the next lies above the note */
        if (2.0f * note < (float)b->z[k].root + (float)b->z[k + 1].root)
            return &b->z[k];
    return &b->z[b->n - 1];
}

static float t60k(float secs, float sr) { return secs > 0.0f ? fm_expf(-T60 / (secs * sr)) : 0.0f; }

void samp_start(smp_voice_t *v, const vx_patch_t *p, float note, float vel)
{
    const smp_bank_t *b = smp_bank((int)p->p[0]);
    float dv = v->dv == 2.0f ? 2.0f : 1.0f, sr = SR / dv, hz = 440.0f * fm_exp2f((note - 69.0f) * (1.0f / 12.0f));
    float dec = p->dec, rel = p->rel, fc;
    int muted = p->p[3] > 0.0f;
    memset(v, 0, sizeof *v);
    v->dv = dv;
    if (!b)
        return; /* (an unknown bank: silent, done) */
    smp_dec_init(&v->d, samp_zone(b, note));
    v->step = fm_exp2f((note - (float)v->d.z->root) * (1.0f / 12.0f)) * (float)v->d.z->rate / sr;
    v->gain = v->d.z->gain * (1.0f / 32768.0f);
    vel = fm_clampf(vel, 0.0f, 1.0f);
    /* the tone: the cutoff at full velocity (Hz plus the note's multiple), darker by p[2] at velocity 0 */
    fc = muted ? MUTE_TONE * hz : p->p[1] + p->p[8] * hz;
    fc *= 1.0f - p->p[2] * (1.0f - vel);
    if (fc > 0.0f) {
        v->lc = 1.0f - fm_expf(-FM_TWO_PI * fm_minf(fc, TONE_MAX) / sr);
        v->lce = muted || p->p[6] <= 0.0f ? v->lc : v->lc * p->p[6];
        v->lcf = muted || p->p[7] <= 0.0f ? 0.0f : 1.0f - fm_expf(-(float)TONE_CR / (p->p[7] * sr));
    } else
        v->lc = v->lce = 1.0f;
    if (muted) {
        dec = MUTE_DEC;
        rel = fm_minf(rel, MUTE_REL);
    }
    v->ainc = p->att * sr > 1.0f ? 1.0f / (p->att * sr) : 1.0f;
    v->env = v->ainc >= 1.0f ? 1.0f : 0.0f;
    v->stage = v->ainc >= 1.0f ? 1 : 0;
    v->sus = muted ? 0.0f : p->sus;
    v->dm = t60k(dec * fm_exp2f(-(note - 60.0f) * (1.0f / 12.0f) * p->p[5]), sr);
    v->rm = t60k(rel, sr);
    v->vamt = muted ? 0.0f : p->p[4] * CENT;
    v->vdt = dv / SR;
    v->on = 1;
}

void samp_release(smp_voice_t *v)
{
    if (v->on)
        v->stage = 2;
}

float samp_sample(smp_voice_t *v)
{
    float f, s = v->step, x, a, b, c, d;
    if (!v->on)
        return 0.0f;
    if (v->vamt > 0.0f) { /* the delayed vibrato: the step bent by up to p[4] cents */
        float dep = fm_clampf((v->vt - VIB_DELAY) * (1.0f / VIB_FADE), 0.0f, 1.0f) * v->vamt;
        if (dep > 0.0f) {
            s *= 1.0f + dep * fm_sin_turns(v->vph);
            v->vph += VIB_RATE * v->vdt;
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
        v->h[3] = (float)dec_next(&v->d);
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
        if (v->lcf > 0.0f && ++v->tc >= TONE_CR) {
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
        if (v->sus <= 0.0f && v->env < ENV_OFF)
            v->on = 0;
        break;
    default:
        v->env *= v->rm;
        if (v->env < ENV_OFF)
            v->on = 0;
    }
    return v->on ? x * v->env : 0.0f;
}

/* SPDX-License-Identifier: MPL-2.0 */
#include <stdint.h>
#include <string.h>
#include "groove.h"
#include "rng.h"

enum { BASS_LO = 28, BASS_HI = 47 };

/* integer mix of the caller's salt with (bar, step, lane): per-hit jitter without any state */
static uint32_t mix(uint32_t salt, int bar, int step, int lane)
{
    uint32_t h = salt * 0x9E3779B1u ^ (uint32_t)(bar + 1) * 0x85EBCA6Bu ^ (uint32_t)(step + 1) * 0xC2B2AE35u
                 ^ (uint32_t)(lane + 1) * 0x27D4EB2Fu;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}

static float frand(uint32_t h) { return (float)(h >> 8) * (1.0f / 16777216.0f); }   /* [0, 1) */

static uint32_t to_samples(float ms)
{
    return ms <= 0.0f ? 0u : (uint32_t)(ms * 44.1f + 0.5f);
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

int groove_bass_style(const song_roll_t *r)
{
    rng_t g;
    const lib_set_t *s = lib_set_by_id((uint8_t)r->v[RF_SET]);
    const lib_map_t *m = lib_map_by_id((uint8_t)r->v[RF_MAP]);
    int pedal, walk, fifth;

    rng_seed(&g, r->seed, 112);
    pedal = rng_int(&g, 100);
    walk = rng_int(&g, 100);
    fifth = rng_int(&g, 2);
    if (((m && m->sparse) || r->v[RF_MOOD] == 0) && pedal < 50) return BS_PEDAL;
    if (s && s->bass == INS_UPRIGHT && walk < 35) return BS_WALK;
    return fifth ? BS_ROOT_FIFTH : BS_ROOTS;
}

/* the pitch (MIDI, 28-47) of pitch class pc plus an interval, folded into the bass register */
static int bass_pitch(int pc, int interval)
{
    int n = BASS_LO + (((pc - BASS_LO) % 12) + 12) % 12;
    n += interval;
    while (n > BASS_HI) n -= 12;
    while (n < BASS_LO) n += 12;
    return n;
}

/* 7 when the chord holds a perfect fifth above its root, else 0 (the root): a fifth the chord lacks could rub the
   melody, which keeps clear of the chord's own pitch classes only */
static int chord_fifth_interval(const int *pcs, int npcs)
{
    int i;
    for (i = 1; i < npcs; i++)
        if (((pcs[i] - pcs[0]) % 12 + 12) % 12 == 7) return 7;
    return 0;
}

/* the bass note (MIDI) on this step, 0 when none; style is the effective one (walk already resolved) */
static int bass_note(int style, const lib_pattern_t *p, int bar, int step, const int *pcs, int npcs, int bpc)
{
    int root = pcs[0], fifth = chord_fifth_interval(pcs, npcs);
    switch (style) {
    case BS_PEDAL:                        /* R21: the root at every chord change, held to the next */
        return step == 0 && bar % bpc == 0 ? bass_pitch(root, 0) : 0;
    case BS_WALK: {                       /* quarter notes: root, third, fifth, chromatic approach */
        int third = npcs > 1 ? ((pcs[1] - root) % 12 + 12) % 12 : 4;
        if (step & 3) return 0;
        switch (step >> 2) {
        case 0: return bass_pitch(root, 0);
        case 1: return bass_pitch(root, third);
        case 2: return bass_pitch(root, fifth);
        default: return bass_pitch(root, (bar & 1) ? 2 : -1);
        }
    }
    case BS_ROOT_FIFTH:
        if (step != 0 && !((p->hit[D_KICK] >> step) & 1)) return 0;
        return bass_pitch(root, step == 8 || step == 10 ? fifth : 0);
    default:                              /* BS_ROOTS: the root on kick hits */
        return (p->hit[D_KICK] >> step) & 1 ? bass_pitch(root, 0) : 0;
    }
}

/* the bass style playing at `bar`: BS_WALK only inside its 8-bar window (seed stream 113), root and fifth elsewhere */
static int bass_style_at(const song_roll_t *r, int bar)
{
    int style = groove_bass_style(r);
    if (style == BS_WALK) {
        rng_t g;
        rng_seed(&g, r->seed, 113);
        if ((bar / 8) % 4 != (int)(rng_next(&g) % 4u)) style = BS_ROOT_FIFTH;
    }
    return style;
}

int groove_bass_note(const song_roll_t *r, const plan_block_t *b, int bar, int step, const int *chord_pcs, int npcs)
{
    const lib_pattern_t *p = lib_pattern_by_id((uint8_t)r->v[RF_PATTERN]);
    const lib_prog_t *pr = lib_prog_by_id((uint8_t)r->v[RF_PROG]);
    int bpc = pr && pr->bars_per_chord ? pr->bars_per_chord : 1;
    if (!p || !b || !(b->layers & L_BASS) || !chord_pcs || npcs <= 0 || step < 0 || step > 15) return 0;
    return bass_note(bass_style_at(r, bar), p, bar, step, chord_pcs, npcs, bpc);
}

static int push(song_ev_t *out, int n, int max, int voice, int note, int vel, uint32_t delay, uint32_t len)
{
    if (n >= max) return n;
    out[n].voice = (uint8_t)voice;
    out[n].note = (uint8_t)note;
    out[n].vel = (uint8_t)clampi(vel, 25, 110);
    out[n].pad = 0;
    out[n].delay = delay;
    out[n].len = len;
    return n + 1;
}

int groove_step(const song_roll_t *r, const plan_block_t *b, int bar, int step, float step_samples,
                const int *chord_pcs, int npcs, uint32_t salt, song_ev_t *out, int max)
{
    static const uint8_t lane_voice[6] = {V_KICK, V_SNARE, V_HAT, V_OHAT, V_SHAKER, V_RIM};
    static const uint8_t lane_layer[6] = {L_KICK, L_SNARE, L_HATS, L_HATS, L_HATS, L_SNARE};
    const lib_pattern_t *p = lib_pattern_by_id((uint8_t)r->v[RF_PATTERN]);
    rng_t g;
    float snare_base, hat_base, swing_f;
    uint32_t swing = 0;
    int n = 0, lane, last_bar, drop, fill, style;

    if (!p || !b || step < 0 || step > 15) return 0;
    last_bar = bar == b->start + b->bars - 1;
    drop = last_bar && (b->flags & PB_DROP_LAST);
    fill = last_bar && (b->flags & PB_FILL_LAST) && !drop;

    /* per-song lateness bases; the snare range leaves room for its +-3 ms so every snare stays within 10-30 ms */
    rng_seed(&g, r->seed, 110);
    snare_base = 13.0f + 14.0f * rng_float(&g);
    rng_seed(&g, r->seed, 111);
    hat_base = 40.0f * rng_float(&g);

    if (step & 1) {
        swing_f = ((float)r->v[RF_SWING] * 0.001f - 0.5f) * 2.0f * step_samples;
        swing = swing_f > 0.0f ? (uint32_t)(swing_f + 0.5f) : 0u;
    }

    if (!drop)
        for (lane = 0; lane < 6; lane++) {
            int hit, ghost, vel;
            uint32_t late = 0;
            float jit = frand(mix(salt, bar, step, lane));
            float jit2 = frand(mix(salt, bar, step, lane + 8));

            if (!(b->layers & lane_layer[lane])) continue;
            if (p->poly3 && lane == D_SHAKER) {
                hit = (bar * 16 + step) % 3 == 0;
                ghost = 0;
            } else {
                hit = (p->hit[lane] >> step) & 1;
                ghost = !hit && ((p->ghost[lane] >> step) & 1);
            }
            if (fill && step >= 12 && (lane == D_SNARE || (lane == D_KICK && step == 14))) hit = ghost = 0;
            if (!hit && !ghost) continue;

            switch (lane) {
            case D_KICK:   vel = hit ? 94 + (int)(jit * 14.0f) : 42 + (int)(jit * 10.0f); break;
            case D_SNARE:  vel = hit ? 88 + (int)(jit * 16.0f) : 25 + (int)(jit * 10.0f); break;
            case D_RIM:    vel = hit ? 58 + (int)(jit * 12.0f) : 30 + (int)(jit * 8.0f); break;
            case D_HAT:    vel = hit ? 46 + (int)(jit * 10.0f) + ((step & 3) == 0 ? 4 : 0) : 36 + (int)(jit * 8.0f); break;
            case D_OHAT:   vel = hit ? 50 + (int)(jit * 10.0f) : 34 + (int)(jit * 8.0f); break;
            default:       vel = hit ? 42 + (int)(jit * 10.0f) : 28 + (int)(jit * 8.0f); break;
            }
            if (lane == D_SNARE || lane == D_RIM)
                late = to_samples(snare_base + (jit2 * 6.0f - 3.0f));
            else if (lane != D_KICK) {
                float ms = hat_base + (jit2 * 16.0f - 8.0f);
                late = to_samples(ms < 0.0f ? 0.0f : ms > 70.0f ? 70.0f : ms);
            }
            n = push(out, n, max, lane_voice[lane], 0, vel, swing + late, 0);
        }

    if (fill && step >= 12 && (b->layers & (L_SNARE | L_KICK))) {
        float jit2 = frand(mix(salt, bar, step, 20));
        uint32_t late = to_samples(snare_base + (jit2 * 6.0f - 3.0f));
        if (b->layers & L_SNARE)
            n = push(out, n, max, V_SNARE, 0, 60 + (step - 12) * 10, swing + late, 0);
        if ((b->layers & L_KICK) && step == 14)
            n = push(out, n, max, V_KICK, 0, 96, swing, 0);
    }

    if ((b->layers & L_BASS) && chord_pcs && npcs > 0) {
        const lib_prog_t *pr = lib_prog_by_id((uint8_t)r->v[RF_PROG]);
        int note, bpc = pr && pr->bars_per_chord ? pr->bars_per_chord : 1;
        style = bass_style_at(r, bar);
        note = bass_note(style, p, bar, step, chord_pcs, npcs, bpc);
        if (note) {
            uint32_t len;
            int vel = 78 + (int)(frand(mix(salt, bar, step, 30)) * 12.0f);
            if (style == BS_PEDAL)
                len = (uint32_t)((float)(16 * bpc) * step_samples);
            else if (style == BS_WALK)
                len = (uint32_t)(4.0f * step_samples);
            else {
                int d = 1;      /* distance in steps to the next bass note, at most one beat */
                while (d < 4 && !bass_note(style, p, bar, (step + d) & 15, chord_pcs, npcs, bpc)) d++;
                len = (uint32_t)((float)d * step_samples);
            }
            n = push(out, n, max, V_BASS, note, vel, swing, len);
        }
    }
    return n;
}

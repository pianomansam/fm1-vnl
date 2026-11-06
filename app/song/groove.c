/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdint.h>
#include <string.h>
#include "groove.h"
#include "rng.h"
#include "theory.h"
#include "harmony.h"
#include "override.h"

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

/* the bass's draws' seed: the song's, or the workbench's bass re-roll (bass_salt) */
static uint32_t bass_seed(uint32_t seed) { return song_ov_salted(seed, song_ov()->bass_salt); }

/* the onsets a bass style plays in a bar over pattern p, fills and ghosts aside */
static unsigned bass_rhythm(int style, const lib_pattern_t *p)
{
    unsigned kick = p ? lib_lane_hits(p, D_KICK, 0) : 1u;
    switch (style) {
    case BS_PEDAL: return 1u;
    case BS_WALK: return 0x1111u;
    default: return kick;
    }
}

/* the bass style drawn from seed (stream 112). No root-fifth style (Sam, 2026-11-04: the 1-to-5 lines are cheesy):
   its songs play roots on the kick, the same rhythm, as every approved pattern kicks on beat 1. The styles the song
   allows (roots always; pedal over a sparse map or a sad song; walk in a walking set) drawn evenly, tilted by the
   rolled motion (roll.h's motion_tilt; Sam, 2026-11-05: "motion roll -> informed busy-ness roll"): pedal sparse,
   roots middle, walk busy */
static int bass_style_of(const song_roll_t *r, uint32_t seed)
{
    static const uint8_t BUSY[3] = {1, 2, 0};                                 /* by BS_*: roots, walk, pedal */
    static const uint8_t ALT[3][3] = {{BS_PEDAL, BS_WALK, BS_ROOTS},          /* the next to try */
                                      {BS_ROOTS, BS_PEDAL, BS_WALK},
                                      {BS_ROOTS, BS_WALK, BS_PEDAL}};
    const lib_pattern_t *p = lib_pattern_by_id((uint8_t)r->v[RF_PATTERN]);
    unsigned keys;
    int style, first, i;
    rng_t g;
    const lib_set_t *s = lib_set_by_id((uint8_t)r->v[RF_SET]);
    const lib_map_t *m = lib_map_by_id((uint8_t)r->v[RF_MAP]);
    int ok[3], mo = roll_motion(r);
    uint32_t tot = 0, x;

    ok[BS_ROOTS] = 1;
    ok[BS_WALK] = lib_set_walks(s);
    ok[BS_PEDAL] = (m && m->sparse) || r->v[RF_MOOD] == 0;
    for (i = 0; i < 3; i++) tot += ok[i] ? motion_tilt(1, BUSY[i], mo) : 0u;
    rng_seed(&g, seed, 112);
    x = roll_mix(rng_next(&g), 112u) % tot;   /* hashed: a bare stream tracks the others (xorshift is linear) */
    for (style = BS_ROOTS, i = 0; i < 3; i++) {
        uint32_t w = ok[i] ? motion_tilt(1, BUSY[i], mo) : 0u;
        if (x < w) { style = i; break; }
        x -= w;
    }
    /* never the keys' rhythm when it is more than a note on 1 (Sam): the stabs, wherever they fall, or the half notes on 1 and 3 */
    keys = harmony_comp_steps(r);
    for (i = 0, first = style; keys && i < 3 && bass_rhythm(style, p) == keys; i++) style = ALT[first][i];
    return style;
}

int groove_bass_style(const song_roll_t *r) { return bass_style_of(r, bass_seed(r->seed)); }

/* Where the bass plays pitch class pc: in a 12-semitone window centered on the song's tonic (A1-Ab2 for the tonic),
   so roots a step apart in the key sound a step apart, not nearly an octave (a fold by pitch class alone put Eb and E
   11 semitones apart); folded into E1-B2. */
static int bass_place(const song_roll_t *r, int pc)
{
    int key = r->v[RF_KEY], t = 33 + ((key - 9) % 12 + 12) % 12, n = t + ((pc - key) % 12 + 12) % 12;
    if (n > t + 6) n -= 12;
    while (n > BASS_HI) n -= 12;
    while (n < BASS_LO) n += 12;
    return n;
}

/* 1 if bass note n would rub the chord: a chord tone a minor 2nd above it, or a major 7th above it when n is not
   the root (root and major 7th are the maj7 chord itself) */
static int rubs_chord(int n, const int *pcs, int npcs)
{
    int i, pc = ((n % 12) + 12) % 12, root = ((pcs[0] % 12) + 12) % 12;
    for (i = 0; i < npcs; i++) {
        int iv = ((pcs[i] - pc) % 12 + 12) % 12;
        if (iv == 1 || (iv == 11 && pc != root)) return 1;
    }
    return 0;
}

/* 7 when the chord holds a perfect fifth above its root, else 0 (the root): a fifth the chord lacks could rub the
   lead, which keeps clear of the chord's own pitch classes only */
static int chord_fifth_interval(const int *pcs, int npcs)
{
    int i;
    for (i = 1; i < npcs; i++)
        if (((pcs[i] - pcs[0]) % 12 + 12) % 12 == 7) return 7;
    return 0;
}

/* The walking line's last beat leads into the next chord's root (next_root, a pitch class; < 0 at the song's end):
   the scale tone a step below or above it, on the side the line comes from, never one that rubs the chord still
   sounding; failing both, the chord's 5th (or root). Diatonic only (Sam: nothing out of the key). */
static int root_place(const song_roll_t *r, int pc, int bar);

static int approach(const song_roll_t *r, int bar, int from, const int *pcs, int npcs, int next_root, int fallback)
{
    int target, d, side, k, cand[2], nc = 0;
    if (next_root < 0) return fallback;
    target = root_place(r, next_root, bar + 1);           /* exactly where the next bar's root lands */
    side = from < target ? -1 : 1;                       /* arrive in the line's direction */
    for (k = 0; k < 2; k++, side = -side)
        for (d = 1; d <= 2; d++) {
            int n = target + side * d;
            if (th_in_scale(r->v[RF_KEY], r->v[RF_MODE], n)) {
                if (n >= BASS_LO && n <= BASS_HI) cand[nc++] = n;   /* (folded, it would land an octave away) */
                break;
            }
        }
    for (k = 0; k < nc; k++)
        if (!rubs_chord(cand[k], pcs, npcs)) return cand[k];
    return fallback;
}

static int prev_root_of(const song_roll_t *r, int bar);

/* the chord's root in the bass: in the tonic's window, moved an octave when that keeps a step-wise root motion from
   the previous chord's root a step (the window's edge would otherwise split it nearly an octave) */
static int root_place(const song_roll_t *r, int pc, int bar)
{
    int n = bass_place(r, pc), q = prev_root_of(r, bar), p;
    if (q < 0) return n;
    p = bass_place(r, q);
    if (n - p > 6 && n - 12 >= BASS_LO) n -= 12;
    if (p - n > 6 && n + 12 <= BASS_HI) n += 12;
    return n;
}

/* the bass note (MIDI) on this step, 0 when none; style is the effective one (walk already resolved) */
static int bass_note(const song_roll_t *r, int style, const lib_pattern_t *p, int bar, int step, const int *pcs,
                     int npcs, int bpc, int next_root)
{
    int root = root_place(r, pcs[0], bar);
    switch (style) {
    case BS_PEDAL:                        /* R21: the root at every chord change, held to the next */
        return step == 0 && bar % bpc == 0 ? root : 0;
    case BS_WALK: {                       /* quarter notes up from the root: root, third, fifth, then the approach */
        int third = npcs > 1 ? ((pcs[1] - pcs[0]) % 12 + 12) % 12 : 4, fifth = chord_fifth_interval(pcs, npcs);
        int up3 = root + third > BASS_HI ? root + third - 12 : root + third;
        int up5 = root + fifth > BASS_HI ? root + fifth - 12 : root + fifth;
        if (rubs_chord(up3, pcs, npcs)) up3 = root;     /* a note the chord's colors rub gives way to the root */
        if (rubs_chord(up5, pcs, npcs)) up5 = root;
        if (step & 3) return 0;
        switch (step >> 2) {
        case 0: return root;
        case 1: return up3;
        case 2: return up5;
        default: return approach(r, bar, up5, pcs, npcs, (bar + 1) % bpc ? pcs[0] : next_root, up5);
        }
    }
    default:                              /* BS_ROOTS: the root on kick hits */
        return (lib_lane_hits(p, D_KICK, bar) >> step) & 1 ? root : 0;
    }
}

/* the root (pitch class) of the chord before `bar`'s chord, -1 at the song's start */
static int prev_root_of(const song_roll_t *r, int bar)
{
    const lib_prog_t *pr = lib_prog_by_id((uint8_t)r->v[RF_PROG]);
    int bpc = pr && pr->bars_per_chord ? pr->bars_per_chord : 1, b = bar - bar % bpc - 1, loop, pcs[5];
    if (b < 0) return -1;
    loop = pr && pr->n * pr->bars_per_chord ? pr->n * pr->bars_per_chord : 4;
    return harmony_chord_at(r, b, b / loop + 1, 0, pcs) > 0 ? pcs[0] : -1;
}

/* the root (pitch class) of the chord after `bar`, -1 past the song's end: the walking line's target */
static int next_root_of(const song_roll_t *r, int bar)
{
    const lib_prog_t *pr = lib_prog_by_id((uint8_t)r->v[RF_PROG]);
    int loop = pr && pr->n * pr->bars_per_chord ? pr->n * pr->bars_per_chord : 4, pcs[5];
    if (bar + 1 >= r->v[RF_BARS]) return -1;
    return harmony_chord_at(r, bar + 1, (bar + 1) / loop + 1, 0, pcs) > 0 ? pcs[0] : -1;
}

/* the bass style playing at `bar`: BS_WALK only inside its 8-bar window (seed stream 113), roots elsewhere */
static int bass_style_at(const song_roll_t *r, int bar)
{
    int style = groove_bass_style(r);
    if (style == BS_WALK) {
        rng_t g;
        rng_seed(&g, bass_seed(r->seed), 113);
        if ((bar / 8) % 4 != (int)(rng_next(&g) % 4u)) style = BS_ROOTS;
    }
    return style;
}

int groove_bass_note(const song_roll_t *r, const plan_block_t *b, int bar, int step, const int *chord_pcs, int npcs)
{
    const lib_pattern_t *p = lib_pattern_by_id((uint8_t)r->v[RF_PATTERN]);
    const lib_prog_t *pr = lib_prog_by_id((uint8_t)r->v[RF_PROG]);
    int bpc = pr && pr->bars_per_chord ? pr->bars_per_chord : 1;
    if (!p || !b || !(b->layers & L_BASS) || !chord_pcs || npcs <= 0 || step < 0 || step > 15) return 0;
    return bass_note(r, bass_style_at(r, bar), p, bar, step, chord_pcs, npcs, bpc, next_root_of(r, bar));
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
    uint32_t swing = 0, dsalt = song_ov_salted(salt, song_ov()->drums_salt);
    int n = 0, lane, last_bar, drop, fill, style, eighths;

    if (!p || !b || step < 0 || step > 15) return 0;
    /* a sampled kit's hats and shaker in 16ths at a light swing march (Sam, set 26): there, 8ths only */
    eighths = r->v[RF_SWING] < 550 && lib_kit_sampled(lib_song_kit(r));
    last_bar = bar == b->start + b->bars - 1;
    drop = (b->flags & PB_DROP_LAST) && (last_bar || ((b->flags & PB_DROP2) && bar == b->start + b->bars - 2));   /* 1 or 2 bars */
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
            float jit = frand(mix(dsalt, bar, step, lane));
            float jit2 = frand(mix(dsalt, bar, step, lane + 8));

            if (!(b->layers & lane_layer[lane])) continue;
            if (p->poly3 && lane == D_SHAKER) {
                hit = (bar * 16 + step) % 3 == 0;
                ghost = 0;
            } else {
                hit = (lib_lane_hits(p, lane, bar) >> step) & 1;
                ghost = !hit && ((lib_lane_ghosts(p, lane, bar) >> step) & 1);
            }
            /* calm songs thin the hats (Sam, 2026-11-05): rolled below motion 35, the closed hat drops its ghosts
               (below 25, song.c drops the hats altogether, after the ride takes its share) */
            if (lane == D_HAT && roll_motion(r) < 35) ghost = 0;
            if (eighths && (lane == D_HAT || lane == D_SHAKER) && (step & 1)) hit = ghost = 0;
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
        float jit2 = frand(mix(dsalt, bar, step, 20));
        uint32_t late = to_samples(snare_base + (jit2 * 6.0f - 3.0f));
        if (b->layers & L_SNARE)
            n = push(out, n, max, V_SNARE, 0, 60 + (step - 12) * 10, swing + late, 0);
        if ((b->layers & L_KICK) && step == 14)
            n = push(out, n, max, V_KICK, 0, 96, swing, 0);
    }

    if ((b->layers & L_BASS) && chord_pcs && npcs > 0) {
        const lib_prog_t *pr = lib_prog_by_id((uint8_t)r->v[RF_PROG]);
        int note, bpc = pr && pr->bars_per_chord ? pr->bars_per_chord : 1, nr = next_root_of(r, bar);
        style = bass_style_at(r, bar);
        note = bass_note(r, style, p, bar, step, chord_pcs, npcs, bpc, nr);
        if (note) {
            uint32_t len;
            int vel = 78 + (int)(frand(mix(bass_seed(salt), bar, step, 30)) * 12.0f);
            if (style == BS_PEDAL)
                len = (uint32_t)((float)(16 * bpc) * step_samples);
            else if (style == BS_WALK)
                len = (uint32_t)(4.0f * step_samples);
            else {
                int d = 1;      /* distance in steps to the next bass note, at most one beat */
                while (d < 4 && !bass_note(r, style, p, bar, (step + d) & 15, chord_pcs, npcs, bpc, nr)) d++;
                len = (uint32_t)((float)d * step_samples);
            }
            n = push(out, n, max, V_BASS, note, vel, swing, len);
        }
    }
    return n;
}

int groove_ride(const song_roll_t *r, int *section_block)
{
    song_plan_t p;
    rng_t g;
    int kit = lib_song_kit(r), x, i, j, n = 0, cand[25], last_groove = -1, walk = -1, ride;
    *section_block = -1;
    if (!lib_kit_sampled(kit)) return RIDE_NONE;
    rng_seed(&g, mix(r->seed, 193, 0, 0), 193);    /* hashed apart from the kit's draw (192) and the groove's */
    x = rng_int(&g, 100);
    ride = song_ov()->ride >= 0 ? song_ov()->ride : kit == KIT_S_WARM || x < 15 ? RIDE_ALL : x >= 65 ? RIDE_NONE
                                                                                                 : RIDE_SECTION;
    if (ride != RIDE_SECTION) return ride;
    /* one block that has hats: a lead section (the B section), the last groove, or where the bass walks */
    plan_build(r, &p);
    if (bass_style_of(r, r->seed) == BS_WALK) {   /* the rolled bass: its re-roll (bass_salt) leaves the drums be */
        rng_t w;
        rng_seed(&w, r->seed, 113);                 /* bass_style_at's window */
        walk = (int)(rng_next(&w) % 4u);
    }
    for (i = 0; i < p.n; i++) {
        const plan_block_t *b = &p.b[i];
        int walks = 0, bar;
        if (!(b->layers & L_HATS)) continue;
        if (b->section == SEC_GROOVE) last_groove = i;
        for (bar = b->start; walk >= 0 && (b->layers & L_BASS) && bar < b->start + b->bars; bar++)
            walks |= (bar / 8) % 4 == walk;
        if (b->section == SEC_LEAD || walks) cand[n++] = i;
    }
    if (last_groove >= 0) {
        for (j = 0; j < n && cand[j] != last_groove; j++) {}
        if (j == n) cand[n++] = last_groove;
    }
    if (n == 0) return RIDE_NONE;
    *section_block = cand[rng_int(&g, n)];
    return RIDE_SECTION;
}

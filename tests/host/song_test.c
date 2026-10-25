/* SPDX-License-Identifier: MPL-2.0 */
/* The song engine on the host: seeded random streams (and, in later tasks, the rest of app/song). */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "rng.h"
#include "theory.h"
#include "fastmath.h"
#include "library.h"
#include "roll.h"
#include "plan.h"
#include "groove.h"
#include "harmony.h"
#include "melody.h"
#include "voices.h"
#include "patch.h"
#include "song.h"
#include "fx.h"
#include "phrase.h"
#include "pads.h"
#include "samples.h"
#include "sampler.h"
#include "strum.h"

static int fails;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        if (!(c)) {                                                                                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                            \
            printf(__VA_ARGS__);                                                                                   \
            printf("\n");                                                                                          \
            fails++;                                                                                               \
        }                                                                                                          \
    } while (0)

static void test_rng(void)
{
    rng_t a, b;
    int i, same = 1, hit[7] = {0}, all = 1, bad = 0, ones = 0;
    uint8_t w[3] = {0, 3, 1}, z[3] = {0, 0, 0};

    rng_seed(&a, 12345, 2);
    rng_seed(&b, 12345, 2);
    for (i = 0; i < 1000; i++)
        if (rng_next(&a) != rng_next(&b))
            same = 0;
    CHECK(same, "same (seed, stream) must repeat");

    rng_seed(&a, 777, 0);
    rng_seed(&b, 777, 1);
    CHECK(a.s != 0 && b.s != 0, "states must be nonzero");
    CHECK(rng_next(&a) != rng_next(&b), "streams 0 and 1 must differ");

    rng_seed(&a, 0, 0);
    CHECK(a.s != 0, "seed 0 stream 0 must give a nonzero state");

    rng_seed(&a, 99, 3);
    for (i = 0; i < 100000; i++) {
        float f = rng_float(&a);
        if (!(f >= 0.0f && f < 1.0f))
            bad++;
    }
    CHECK(bad == 0, "rng_float left [0,1) %d times", bad);

    for (i = 0; i < 1000; i++) {
        int v = rng_int(&a, 7);
        if (v < 0 || v > 6)
            bad++;
        else
            hit[v] = 1;
    }
    for (i = 0; i < 7; i++)
        all &= hit[i];
    CHECK(all && bad == 0, "rng_int(7) must hit all of 0..6 in range");
    CHECK(rng_int(&a, 0) == 0 && rng_int(&a, -3) == 0, "rng_int with n <= 0 is 0");

    for (i = 0; i < 10000; i++) {
        int v = rng_weighted(&a, w, 3);
        if (v == 0 || v < 0 || v > 2)
            bad++;
        if (v == 1)
            ones++;
    }
    CHECK(bad == 0, "rng_weighted returned a zero-weight or invalid index");
    CHECK(ones >= 7000 && ones <= 8000, "rng_weighted index 1 came %d of 10000 (want ~7500)", ones);
    CHECK(rng_weighted(&a, z, 3) == -1, "all-zero weights return -1");
}

static void test_theory(void)
{
    int pcs[6], n, i, k, d, col, mode, key, out[4], prev[4], bad = 0, third, seventh, move = 0;
    char nm[16];
    th_chord_spec_t c;

    c.degree = 1; c.alter = 0; c.quality = Q_DIATONIC;
    n = th_chord_pcs(0, MODE_IONIAN, c, COLOR_9, pcs);
    CHECK(n == 5 && pcs[0] == 2 && pcs[1] == 5 && pcs[2] == 9 && pcs[3] == 0 && pcs[4] == 4, "ii9 pcs");
    CHECK(!strcmp(th_chord_name(pcs, n, nm), "Dm9"), "name %s, want Dm9", nm);
    {
        int dm9[6], g13[6], n2;
        memcpy(dm9, pcs, sizeof dm9);
        th_voice(dm9, n, NULL, prev);
        c.degree = 4;
        n2 = th_chord_pcs(0, MODE_IONIAN, c, COLOR_RICH, g13);
        CHECK(!strcmp(th_chord_name(g13, n2, nm), "G13"), "name %s, want G13", nm);
        th_voice(g13, n2, prev, out);
        for (i = 0; i < 4; i++)
            if (out[i] - prev[i] > move || prev[i] - out[i] > move)
                move = out[i] > prev[i] ? out[i] - prev[i] : prev[i] - out[i];
        CHECK(move <= 4, "Dm9 to G13 moved a voice %d semitones", move);
    }
    c.degree = 0;
    n = th_chord_pcs(0, MODE_IONIAN, c, COLOR_9, pcs);
    CHECK(!strcmp(th_chord_name(pcs, n, nm), "Cmaj9"), "name %s, want Cmaj9", nm);
    c.degree = 3; c.alter = 0; c.quality = Q_MIN7;
    n = th_chord_pcs(0, MODE_IONIAN, c, COLOR_7, pcs);
    CHECK(!strcmp(th_chord_name(pcs, n, nm), "Fm7"), "name %s, want Fm7", nm);
    c.degree = 4; c.quality = Q_DIATONIC;
    n = th_chord_pcs(0, MODE_IONIAN, c, COLOR_7, pcs);
    CHECK(!strcmp(th_chord_name(pcs, n, nm), "G7"), "name %s, want G7", nm);
    th_guide_tones(pcs, n, &third, &seventh);
    CHECK(third == 11 && seventh == 5, "G7 guide tones %d %d, want 11 5", third, seventh);

    for (mode = 0; mode < MODE_N; mode++)
        for (key = 0; key < 12; key++)
            for (d = 0; d < 7; d++)
                for (col = COLOR_7; col <= COLOR_RICH; col++) {
                    c.degree = (int8_t)d; c.alter = 0; c.quality = Q_DIATONIC;
                    n = th_chord_pcs(key, mode, c, col, pcs);
                    th_voice(pcs, n, NULL, out);
                    for (i = 0; i < 4; i++) {
                        if (out[i] > 72 || out[i] < 48)  /* the keys' register: C3-C5, E3-A4 preferred */
                            bad++;
                        for (k = i + 1; k < 4; k++)
                            if (out[i] == out[k])
                                bad++;
                    }
                    if (out[0] < 48 || out[0] > 60)
                        bad++;
                    for (i = 1; i < 4; i++)
                        if (out[i] < out[0])
                            bad++;
                    if (n < 4 || n > 5)
                        bad++;
                    for (i = 0; i < n; i++)
                        if (!th_in_scale(key, mode, pcs[i]) && col != COLOR_RICH)
                            bad++;
                }
    CHECK(bad == 0, "voicing/chord invariants broken %d times", bad);

    pcs[0] = 0; pcs[1] = 4; pcs[2] = 7; pcs[3] = 11;
    CHECK(th_is_avoid(0, MODE_IONIAN, pcs, 4, 65) == 1, "F4 over Cmaj7 is an avoid note");
    CHECK(th_is_avoid(0, MODE_IONIAN, pcs, 4, 64) == 0, "E4 over Cmaj7 is not an avoid note");
    CHECK(th_in_scale(0, MODE_IONIAN, 64) == 1 && th_in_scale(0, MODE_IONIAN, 63) == 0, "th_in_scale");
    CHECK(th_mode_pool(MODE_IONIAN) == 0xAB5u, "ionian pool 0x%x", th_mode_pool(MODE_IONIAN));
}

static void test_library(void)
{
    int i, j, kc[3] = {0, 0, 0};
    CHECK(LIB_NPROGS >= 14 && LIB_NSETS >= 10 && LIB_NPATTERNS >= 8 && LIB_NMAPS == 5, "table sizes");
    for (i = 0; i < LIB_NPROGS; i++) {
        const lib_prog_t *p = &LIB_PROGS[i];
        for (j = i + 1; j < LIB_NPROGS; j++) CHECK(LIB_PROGS[j].id != p->id, "prog id %d reused", p->id);
        CHECK(lib_prog_by_id(p->id) == p, "prog by id %d", p->id);
        CHECK(p->n >= 2 && p->n <= 4, "prog %d n=%d", p->id, p->n);
        CHECK(p->kind <= PK_JAZZY && p->moods != 0 && p->src && p->src[0] && p->bars_per_chord >= 1, "prog %d fields", p->id);
        if (p->kind <= PK_JAZZY) kc[p->kind]++;
    }
    for (i = 0; i < LIB_NPROGS; i++) {
        int cyc = LIB_PROGS[i].n * LIB_PROGS[i].bars_per_chord;
        CHECK(LIB_PROGS[i].modes != 0 && !(LIB_PROGS[i].modes & (1 << MODE_PHRYGIAN)), "prog %d modes", LIB_PROGS[i].id);
        CHECK(cyc == 2 || cyc == 4 || cyc == 8, "prog %d cycle %d bars", LIB_PROGS[i].id, cyc);
    }
    {
        int m;
        for (i = 0; i < 4; i++)
            for (m = 0; m < MODE_N; m++) {
                int cnt = 0;
                if (!LIB_MOODS[i].mode_w[m]) continue;
                for (j = 0; j < LIB_NPROGS; j++)
                    if ((LIB_PROGS[j].moods & (1 << i)) && (LIB_PROGS[j].modes & (1 << m))) cnt++;
                CHECK(cnt >= 3, "mood %d mode %d has %d progressions", i, m, cnt);
            }
    }
    {   /* R25: a non-jazzy progression's chords, at every color, lie in each mode it claims (jazzy ones may borrow) */
        int m, c, k;
        for (j = 0; j < LIB_NPROGS; j++) {
            const lib_prog_t *p = &LIB_PROGS[j];
            if (p->kind == PK_JAZZY) continue;
            for (m = 0; m < MODE_N; m++) {
                if (!(p->modes & (1 << m))) continue;
                for (i = 0; i < p->n; i++)
                    for (c = COLOR_7; c <= COLOR_RICH; c++) {
                        int pcs[8], n = th_chord_pcs(0, m, p->c[i], c, pcs);
                        for (k = 0; k < n; k++)
                            if (!th_in_scale(0, m, pcs[k])) {
                                CHECK(0, "R25: prog %d mode %d chord %d color %d: pc %d out of scale", p->id, m, i, c,
                                      pcs[k]);
                                break;
                            }
                    }
            }
        }
    }
    {   /* spot-check the claimed chords by name */
        static const struct { int id, mode; const char *want; } sp[] = {
            {9, MODE_IONIAN, "Fmaj7 G7 Em7 Am7"}, {15, MODE_IONIAN, "Dm7 G7 Cmaj7 A7"},
            {6, MODE_DORIAN, "Cm7 Bb7"}, {18, MODE_AEOLIAN, "Cm7 Bb7 Abmaj7 G7"}, {13, MODE_IONIAN, "Fmaj7 E7 Am7 C7"},
            {22, MODE_AEOLIAN, "Cm7 Fm7 Abmaj7 G7"}, {24, MODE_DORIAN, "Cm7 Dbmaj7"},
            {25, MODE_DORIAN, "Cm7 F7 Bbmaj7 Ebmaj7"}, {26, MODE_DORIAN, "Cm7 Dm7 Ebmaj7 F7"},
        };
        int k;
        for (k = 0; k < (int)(sizeof sp / sizeof sp[0]); k++) {
            const lib_prog_t *p = lib_prog_by_id((uint8_t)sp[k].id);
            char got[64] = "", nm[16];
            int pcs[8];
            for (j = 0; j < p->n; j++) {
                int n = th_chord_pcs(0, sp[k].mode, p->c[j], COLOR_7, pcs);
                if (j) strcat(got, " ");
                strcat(got, th_chord_name(pcs, n, nm));
            }
            CHECK(strcmp(got, sp[k].want) == 0, "prog %d: got '%s' want '%s'", sp[k].id, got, sp[k].want);
        }
    }
    CHECK(lib_prog_by_id(255) == NULL, "prog 255");
    {
        int pct[3] = {40, 45, 15};
        for (i = 0; i < 3; i++) {
            int got = kc[i] * 100 / LIB_NPROGS;
            CHECK(got >= pct[i] - 10 && got <= pct[i] + 10, "kind %d is %d%%", i, got);
        }
    }
    for (i = 0; i < LIB_NSETS; i++) {
        const lib_set_t *s = &LIB_SETS[i];
        for (j = i + 1; j < LIB_NSETS; j++) CHECK(LIB_SETS[j].id != s->id, "set id %d reused", s->id);
        CHECK(lib_set_by_id(s->id) == s, "set by id %d", s->id);
        CHECK(s->chords < INS_NONE && s->bass < INS_NONE && s->lead < INS_NONE && s->candy <= INS_NONE, "set %d insts", s->id);
        CHECK(s->kit <= KIT_BRUSHES && s->flavor <= FL_CHILLWAVE && s->moods && s->name && s->src, "set %d fields", s->id);
        /* the pad layer's instrument: the warm synth pad under the synth sets (13, 18, 19, 20), strings elsewhere */
        CHECK(s->pad == ((s->id == 13 || s->id == 18 || s->id == 19 || s->id == 20) ? INS_WARM_PAD : INS_STRINGS),
              "set %d: pad instrument %d", s->id, s->pad);
    }
    CHECK(lib_set_by_id(255) == NULL, "set 255");
    for (i = 0; i < LIB_NPATTERNS; i++) {
        const lib_pattern_t *p = &LIB_PATTERNS[i];
        for (j = i + 1; j < LIB_NPATTERNS; j++) CHECK(LIB_PATTERNS[j].id != p->id, "pattern id %d reused", p->id);
        CHECK(lib_pattern_by_id(p->id) == p, "pattern by id %d", p->id);
        CHECK(p->hit[D_SNARE] != 0 && p->hit[D_KICK] != 0 && p->family <= PF_BOSSA && p->src, "pattern %d", p->id);
    }
    CHECK(lib_pattern_by_id(255) == NULL, "pattern 255");
    {
        static const char *names[5] = {"Classic", "Slow bloom", "Straight in", "Breather", "Drift"};
        for (i = 0; i < LIB_NMAPS; i++) {
            const lib_map_t *m = &LIB_MAPS[i];
            for (j = i + 1; j < LIB_NMAPS; j++) CHECK(LIB_MAPS[j].id != m->id, "map id %d reused", m->id);
            CHECK(lib_map_by_id(m->id) == m, "map by id %d", m->id);
            CHECK(strcmp(m->name, names[i]) == 0 && m->src, "map %d name", i);
        }
        CHECK(lib_map_by_id(255) == NULL, "map 255");
        CHECK(LIB_MAPS[1].intro_bars == 8 && LIB_MAPS[1].entry == EN_SNARE_FIRST && LIB_MAPS[2].intro_bars == 0 &&
              LIB_MAPS[3].middle == MID_KICK_OUT && LIB_MAPS[4].sparse == 1 && LIB_MAPS[0].sparse == 0, "map values");
    }
    for (i = 0; i < 4; i++) {
        int sum = 0;
        for (j = 0; j < MODE_N; j++) sum += LIB_MOODS[i].mode_w[j];
        CHECK(sum > 0 && LIB_MOODS[i].mode_w[MODE_PHRYGIAN] == 0, "mood %d weights", i);
    }
}

static int major_family(int mode) { return mode == MODE_IONIAN || mode == MODE_LYDIAN || mode == MODE_MIXOLYDIAN; }

static int prog_legal(const song_roll_t *r, int id)
{
    const lib_prog_t *p = lib_prog_by_id((uint8_t)id);
    return p && (p->moods & (1 << r->v[RF_MOOD])) && (p->modes & (1 << r->v[RF_MODE]));
}

static int set_legal(const song_roll_t *r, int id)
{
    const lib_set_t *s = lib_set_by_id((uint8_t)id);
    return s && (s->moods & (1 << r->v[RF_MOOD]));
}

static int novelty(const song_roll_t *a, const song_roll_t *b)
{
    int d = 0, t = a->v[RF_TEMPO] - b->v[RF_TEMPO];
    d += a->v[RF_MAP] != b->v[RF_MAP];
    d += a->v[RF_PROG] != b->v[RF_PROG];
    d += a->v[RF_SET] != b->v[RF_SET];
    d += lib_pattern_by_id((uint8_t)a->v[RF_PATTERN])->family != lib_pattern_by_id((uint8_t)b->v[RF_PATTERN])->family;
    d += a->v[RF_MODE] != b->v[RF_MODE];
    d += a->v[RF_DUST] != b->v[RF_DUST];
    d += a->v[RF_NOISE] != b->v[RF_NOISE];
    d += a->v[RF_AMBIENCE] != b->v[RF_AMBIENCE];
    d += t >= 6 || t <= -6;
    return d;
}

/* a synth instrument (the synths Sam kept: the triangle bass, M_SUB's organ, saw lead and pluck, the FM keys and bass;
   not the pad layer's strings and warm pad) */
static int is_synth(int ins) { return ins >= INS_CHIP_BASS && ins <= INS_FM_BASS; }
/* all synth: the keys, the bass and the lead (the ear candy may be acoustic, or none) */
static int set_all_synth(const lib_set_t *s) { return is_synth(s->chords) && is_synth(s->bass) && is_synth(s->lead); }
static int moods_n(int m) { return (m & 1) + (m >> 1 & 1) + (m >> 2 & 1) + (m >> 3 & 1); }

/* The synth sets (Sam's cut, 2026-10-24): the five are there with their instruments, two moods each; every mood has
   an all-synth set; over 3,000 chained songs (the app's xorshift32 seeds) each rolls in 3% or more, and the sets with
   any synth instrument in 25% or more. */
static void test_synth_sets(void)
{
    static const char *names[5] = {"DX keys and FM bass", "Organ and saw lead", "Rhodes and saw lead", "DX and pluck",
                                   "Nylon and synth pluck"};
    static const int want[5][3] = {{INS_DX_EP, INS_FM_BASS, INS_SAW_LEAD},
                                   {INS_ORGAN, INS_CHIP_BASS, INS_SAW_LEAD},
                                   {INS_RHODES, INS_FM_BASS, INS_SAW_LEAD},
                                   {INS_DX_EP, INS_CHIP_BASS, INS_SYNTH_PLUCK},
                                   {INS_NYLON, INS_SUB, INS_SYNTH_PLUCK}};
    const lib_set_t *ns[5];
    long got[5] = {0, 0, 0, 0, 0}, synth = 0;
    uint32_t seed = 4242u;
    song_roll_t prev, r;
    int i, k, m, nsynth = 0;
    for (i = 0; i < 5; i++) {
        ns[i] = NULL;
        for (k = 0; k < LIB_NSETS; k++)
            if (strcmp(LIB_SETS[k].name, names[i]) == 0) ns[i] = &LIB_SETS[k];
        CHECK(ns[i] != NULL, "synth sets: no set \"%s\"", names[i]);
        if (!ns[i]) return;
        CHECK(moods_n(ns[i]->moods) == 2, "synth sets: %s has %d moods, wanted 2", names[i], moods_n(ns[i]->moods));
        CHECK(ns[i]->chords == want[i][0] && ns[i]->bass == want[i][1] && ns[i]->lead == want[i][2],
              "synth sets: %s plays %d %d %d, wanted %d %d %d", names[i], ns[i]->chords, ns[i]->bass, ns[i]->lead,
              want[i][0], want[i][1], want[i][2]);
        CHECK(ns[i]->candy == INS_NONE || ns[i]->candy == INS_VIBES || ns[i]->candy == INS_MUSIC_BOX ||
                  ns[i]->candy == INS_KALIMBA,
              "synth sets: %s's candy %d, wanted vibes, music box, kalimba or none", names[i], ns[i]->candy);
    }
    for (k = 0; k < LIB_NSETS; k++) /* no other set plays a synth */
        nsynth += is_synth(LIB_SETS[k].chords) || is_synth(LIB_SETS[k].bass) || is_synth(LIB_SETS[k].lead) ||
                  is_synth(LIB_SETS[k].candy);
    CHECK(nsynth == 5, "synth sets: %d sets play a synth, wanted the 5", nsynth);
    for (m = 0; m < 4; m++) {
        int any = 0;
        for (k = 0; k < LIB_NSETS; k++) any |= (LIB_SETS[k].moods & (1 << m)) && set_all_synth(&LIB_SETS[k]);
        CHECK(any, "synth sets: mood %d has no all-synth set", m);
    }
    for (k = 0; k < 3000; k++) {
        const lib_set_t *s;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        roll_song(k ? &prev : 0, seed, &r);
        prev = r;
        s = lib_set_by_id((uint8_t)r.v[RF_SET]);
        if (!s) continue;
        synth += is_synth(s->chords) || is_synth(s->bass) || is_synth(s->lead) || is_synth(s->candy);
        for (i = 0; i < 5; i++) got[i] += s == ns[i];
    }
    for (i = 0; i < 5; i++) {
        printf("synth sets: %-21s %.1f%% of 3000 songs\n", names[i], (double)got[i] / 30.0);
        CHECK(got[i] >= 90, "synth sets: %s in %ld of 3000 songs, wanted 3%%+", names[i], got[i]);
    }
    printf("synth sets: any synth in %.1f%% of 3000 songs\n", (double)synth / 30.0);
    CHECK(synth >= 750, "synth sets: a synth set in %ld of 3000 songs, wanted 25%%+", synth);
}

static int roll_valid(const song_roll_t *r)
{
    const int16_t *v = r->v;
    int s = roll_seconds(r);
    return v[RF_KEY] >= 0 && v[RF_KEY] <= 11 && v[RF_TEMPO] >= 70 && v[RF_TEMPO] <= 90 && v[RF_MOTION] >= 0 &&
           v[RF_MOTION] <= 100 && v[RF_MOOD] >= 0 && v[RF_MOOD] <= 3 && v[RF_MODE] >= 0 && v[RF_MODE] < MODE_N &&
           lib_map_by_id((uint8_t)v[RF_MAP]) && prog_legal(r, v[RF_PROG]) && set_legal(r, v[RF_SET]) &&
           lib_pattern_by_id((uint8_t)v[RF_PATTERN]) && v[RF_COLOR] >= 0 && v[RF_COLOR] <= 2 && v[RF_SWING] >= 520 &&
           v[RF_SWING] <= 620 && v[RF_DUST] >= 0 && v[RF_DUST] <= 3 && v[RF_NOISE] >= 0 && v[RF_NOISE] <= 2 &&
           v[RF_AMBIENCE] >= 0 && v[RF_AMBIENCE] <= 1 && v[RF_BARS] % 4 == 0 && v[RF_BARS] >= 36 &&
           v[RF_BARS] <= 76 && s >= 120 && s <= 210;
}

static void test_roll(void)
{
    song_roll_t prev, cur, again;
    int i, f, k, bad_valid = 0, bad_nov = 0, bad_key = 0, bad_det = 0;
    long swing_sum = 0;
    int kinds[3] = {0, 0, 0};
    int secs_lo = 999, secs_hi = 0, tempo_peak = 0, tempo_n = 0;

    roll_song(NULL, 1, &prev);
    CHECK(roll_valid(&prev), "first roll valid");
    for (i = 2; i <= 2000; i++) {
        int dk, a, b;
        roll_song(&prev, (uint32_t)i, &cur);
        roll_song(&prev, (uint32_t)i, &again);
        if (memcmp(&cur, &again, sizeof cur) != 0) bad_det++;
        if (!roll_valid(&cur)) { bad_valid++; if (bad_valid < 4) printf("  invalid roll at seed %d\n", i); }
        if (novelty(&cur, &prev) < 5) bad_nov++;
        if (cur.v[RF_TEMPO] - prev.v[RF_TEMPO] < 6 && prev.v[RF_TEMPO] - cur.v[RF_TEMPO] < 6) bad_nov++;
        dk = ((cur.v[RF_KEY] - prev.v[RF_KEY]) % 12 + 12) % 12;
        a = major_family(prev.v[RF_MODE]);
        b = major_family(cur.v[RF_MODE]);
        if (!(dk == 5 || dk == 7 || (a && !b && dk == 9) || (!a && b && dk == 3))) bad_key++;
        swing_sum += cur.v[RF_SWING];
        kinds[lib_prog_by_id((uint8_t)cur.v[RF_PROG])->kind]++;
        k = roll_seconds(&cur);
        if (k < secs_lo) secs_lo = k;
        if (k > secs_hi) secs_hi = k;
        prev = cur;
    }
    CHECK(cur.seed == 2000, "seed stored");
    CHECK(bad_det == 0, "same seed + prev differed %d times", bad_det);
    CHECK(bad_valid == 0, "%d invalid rolls", bad_valid);
    CHECK(bad_nov == 0, "%d rolls with fewer than 5 novel choices or tempo within 6 BPM", bad_nov);
    CHECK(bad_key == 0, "%d unrelated or repeated keys", bad_key);
    CHECK(swing_sum / 1999 >= 545 && swing_sum / 1999 <= 565, "swing mean %ld", swing_sum / 1999);
    {   /* R8: the progression mix the listener hears is ~40/45/15 */
        static const int want[3] = {40, 45, 15};
        for (k = 0; k < 3; k++)
            CHECK(kinds[k] * 100 / 1999 >= want[k] - 5 && kinds[k] * 100 / 1999 <= want[k] + 5,
                  "rolled kind %d is %d%%", k, kinds[k] * 100 / 1999);
    }
    CHECK(secs_lo < 135 && secs_hi > 190, "seconds span %d-%d", secs_lo, secs_hi);
    for (i = 1; i <= 2000; i++) {   /* first songs: tempo peaks at 80-84 */
        roll_song(NULL, (uint32_t)i, &cur);
        CHECK(roll_valid(&cur), "first roll seed %d valid", i);
        tempo_n++;
        if (cur.v[RF_TEMPO] >= 80 && cur.v[RF_TEMPO] <= 84) tempo_peak++;
    }
    CHECK(tempo_peak * 100 / tempo_n >= 35, "tempo 80-84 only %d of %d", tempo_peak, tempo_n);

    /* Review Focus 3: tempo picks re-clamp the length */
    roll_song(NULL, 7, &cur);
    roll_set(&cur, RF_TEMPO, 90);
    roll_set(&cur, RF_BARS, 76);
    CHECK(cur.v[RF_BARS] == 76, "76 bars at 90 BPM allowed");
    CHECK(roll_set(&cur, RF_TEMPO, 70) == 70 && roll_seconds(&cur) <= 210 && roll_valid(&cur), "70 BPM: %d s",
          roll_seconds(&cur));
    roll_set(&cur, RF_BARS, 36);
    CHECK(cur.v[RF_BARS] == 36, "36 bars at 70 BPM allowed");
    CHECK(roll_set(&cur, RF_TEMPO, 90) == 90 && roll_seconds(&cur) >= 120 && roll_valid(&cur), "90 BPM: %d s",
          roll_seconds(&cur));
    CHECK(roll_set(&cur, RF_TEMPO, 200) == 90 && roll_set(&cur, RF_TEMPO, 10) == 70, "tempo clamps");
    CHECK(roll_set(&cur, RF_BARS, 1000) <= 76 && roll_valid(&cur), "bars clamp");
    CHECK(roll_set(&cur, RF_SWING, 900) == 620 && roll_set(&cur, RF_NOISE, -4) == 0, "clamps");
    {
        int old = cur.v[RF_PATTERN];
        CHECK(roll_set(&cur, RF_PATTERN, 200) == old, "unknown pattern id rejected");
    }

    /* Review Focus 4: dice with one or two legal progressions */
    {
        static lib_prog_t one[1], two[2];
        one[0] = *lib_prog_by_id(1);
        two[0] = *lib_prog_by_id(1);
        two[1] = *lib_prog_by_id(8);
        roll_song(NULL, 3, &cur);
        roll_set(&cur, RF_MOOD, 3);      /* sunny */
        roll_set(&cur, RF_MODE, MODE_IONIAN);
        roll_use_progs(one, 1);
        cur.v[RF_PROG] = 1;
        CHECK(roll_dice(&cur, RF_PROG, 5) == 0 && cur.v[RF_PROG] == 1, "one progression: dice returns 0");
        roll_use_progs(two, 2);
        CHECK(roll_dice(&cur, RF_PROG, 5) == 1 && cur.v[RF_PROG] == 8, "two progressions: dice gives the other");
        CHECK(roll_dice(&cur, RF_PROG, 6) == 1 && cur.v[RF_PROG] == 1, "and back");
        roll_use_progs(NULL, 0);
        /* the real library: wistful + Lydian allows exactly progressions 27 and 36 (R25) */
        roll_set(&cur, RF_MOOD, 1);
        CHECK(roll_set(&cur, RF_MODE, MODE_LYDIAN) == MODE_LYDIAN, "wistful Lydian pickable");
        CHECK(prog_legal(&cur, cur.v[RF_PROG]), "prog re-picked on mode change");
        k = cur.v[RF_PROG];
        CHECK(roll_dice(&cur, RF_PROG, 9) == 1 && cur.v[RF_PROG] == (k == 27 ? 36 : 27), "wistful Lydian dice");
    }
    for (f = 0; f < RF_N; f++) {
        int same = 0, illegal = 0, unrelated = 0;
        roll_song(NULL, 11, &cur);
        for (i = 0; i < 100; i++) {
            song_roll_t before = cur;
            int ch = roll_dice(&cur, f, (uint32_t)i), g;
            if (ch && cur.v[f] == before.v[f]) same++;
            if (!ch && memcmp(&cur, &before, sizeof cur) != 0) same++;
            if (!roll_valid(&cur)) illegal++;
            if (f == RF_TEMPO && ch && cur.v[f] - before.v[f] < 2 && before.v[f] - cur.v[f] < 2) same++;
            for (g = 0; g < RF_N; g++) {   /* only the field and its dependents may move */
                int dep = g == f || (f == RF_TEMPO && g == RF_BARS) ||
                          (f == RF_MODE && g == RF_PROG) ||
                          (f == RF_MOOD && (g == RF_MODE || g == RF_PROG || g == RF_SET));
                if (!dep && cur.v[g] != before.v[g]) unrelated++;
            }
        }
        CHECK(same == 0, "field %d: dice returned 1 without a change (or 0 with one) %d times", f, same);
        CHECK(illegal == 0, "field %d: dice left %d illegal rolls", f, illegal);
        CHECK(unrelated == 0, "field %d: dice moved %d unrelated fields", f, unrelated);
    }
    /* R4: mood and mode changes keep the progression legal; illegal mode picks are refused */
    {
        int m, bad = 0;
        for (i = 1; i <= 200; i++) {
            roll_song(NULL, (uint32_t)i, &cur);
            for (m = 0; m < 4; m++) {
                roll_set(&cur, RF_MOOD, m);
                if (cur.v[RF_MOOD] != m || !roll_valid(&cur)) bad++;
            }
            for (m = 0; m < MODE_N; m++) {
                int got = roll_set(&cur, RF_MODE, m), any = 0, j;
                for (j = 0; j < LIB_NPROGS; j++)
                    if ((LIB_PROGS[j].moods & (1 << cur.v[RF_MOOD])) && (LIB_PROGS[j].modes & (1 << m))) any = 1;
                if ((got == m) != any || !roll_valid(&cur)) bad++;
            }
        }
        CHECK(bad == 0, "R4: %d bad mood/mode picks", bad);
        roll_song(NULL, 5, &cur);
        CHECK(roll_set(&cur, RF_MODE, MODE_PHRYGIAN) != MODE_PHRYGIAN, "Phrygian has no progression");
    }
}

#define DRUMS (L_KICK | L_SNARE | L_HATS)

static int bits(int x)
{
    int n = 0;
    for (; x; x >>= 1) n += x & 1;
    return n;
}

/* Optional moves (what Motion adds): 1-bar drops, hats-only toggles, lead and candy toggles. */
static int optional_moves(const song_plan_t *p)
{
    int i, n = 0;
    for (i = 0; i < p->n; i++) {
        if (p->b[i].flags & PB_DROP_LAST) n++;
        if (i > 0) {
            int d = p->b[i].layers ^ p->b[i - 1].layers;
            if ((d & DRUMS) == L_HATS) n++;
            n += bits(d & (L_LEAD | L_CANDY));
        }
    }
    return n;
}

static void plan_roll(song_roll_t *r, uint32_t seed, int map, int bars, int motion)
{
    roll_song(NULL, seed, r);
    r->v[RF_MAP] = (int16_t)map;
    r->v[RF_BARS] = (int16_t)bars;
    r->v[RF_MOTION] = (int16_t)motion;
}

static void test_plan(void)
{
    song_roll_t r;
    song_plan_t p, q;
    int s, mi, i;
    int bad_cover = 0, bad_grid = 0, bad_lead0 = 0, bad_intro = 0, bad_step = 0, bad_break = 0, bad_outro = 0;
    int bad_fade = 0, bad_entry = 0, bad_kickout = 0, bad_sparse = 0, bad_candy = 0, bad_bed = 0, bad_det = 0;
    int bad_m0 = 0, bad_at = 0, bad_steady = 0, bad_hatsonly = 0, bad_sec = 0, bad_lead = 0;
    long lead_bars = 0, mid_bars_all = 0;

    CHECK(strcmp(plan_section_name(SEC_INTRO), "intro") == 0 && strcmp(plan_section_name(SEC_GROOVE), "groove") == 0 &&
              strcmp(plan_section_name(SEC_LEAD), "lead") == 0 && strcmp(plan_section_name(SEC_BREAK), "break") == 0 &&
              strcmp(plan_section_name(SEC_OUTRO), "outro") == 0,
          "section names");

    for (s = 1; s <= 2000; s++) {
        for (mi = 0; mi < LIB_NMAPS; mi++) {
            const lib_map_t *m = &LIB_MAPS[mi];
            int bars = 36 + 4 * (s % 11), motion = (s * 37) % 101, sum = 0;
            int nbreak = 0, nkick = 0, drum_mid = 0, mid = 0, first_lead = -1;
            int kick_at = -1, snare_at = -1, hats_at = -1;
            plan_roll(&r, (uint32_t)s, m->id, bars, motion);
            plan_build(&r, &p);
            plan_build(&r, &q);
            if (memcmp(&p, &q, sizeof p) != 0) bad_det++;
            if (p.bars != bars || p.n < 1 || p.n > 24) { bad_cover++; continue; }
            for (i = 0; i < p.n; i++) {
                const plan_block_t *b = &p.b[i];
                int L = b->layers;
                if (b->start != sum || b->bars == 0) bad_cover++;
                sum += b->bars;
                if (b->start % 4 || b->bars % 4) bad_grid++;
                if ((L & L_LEAD) && b->start == 0) bad_lead0++;
                if (L & L_BED) bad_bed++;
                if ((L & L_LEAD) && first_lead < 0) {
                    first_lead = i;
                    if (L & L_CANDY) bad_candy++;
                }
                if ((L & L_LEAD) && (b->section == SEC_INTRO || !(L & L_KEYS))) bad_lead0++;
                if (motion == 0 && ((L & L_CANDY) || (b->flags & PB_DROP_LAST))) bad_m0++;
                if (kick_at < 0 && (L & L_KICK)) { kick_at = b->start; if (!(L & L_BASS)) bad_entry++; }
                if (snare_at < 0 && (L & L_SNARE)) snare_at = b->start;
                if (hats_at < 0 && (L & L_HATS)) hats_at = b->start;
                /* R11: the skeleton binds over Motion. Slow bloom: once the kick is in, no drum-layer moves at all.
                   Straight in: hat drop-outs only, never a 1-bar drum drop. */
                if (m->middle == MID_NONE && !m->sparse && kick_at >= 0 &&
                    ((L & (DRUMS | L_BASS)) != (DRUMS | L_BASS) || (b->flags & PB_DROP_LAST)))
                    bad_steady++;
                if (m->middle == MID_HATS_ONLY &&
                    ((L & (L_KICK | L_SNARE | L_BASS)) != (L_KICK | L_SNARE | L_BASS) || (b->flags & PB_DROP_LAST)))
                    bad_hatsonly++;
                if (b->section != SEC_INTRO && b->section != SEC_OUTRO) {
                    /* R12: a break is drums out or thinned: kick and snare both out, or Breather's kick-out block */
                    int kickout = m->middle == MID_KICK_OUT && (L & L_SNARE) && (L & L_HATS) && !(L & (L_KICK | L_BASS));
                    if ((b->section == SEC_BREAK) != (!(L & (L_KICK | L_SNARE)) || kickout)) bad_sec++;
                    mid += b->bars;
                    if (L & DRUMS) drum_mid += b->bars;
                    else {
                        nbreak++;
                        if (m->middle == MID_BREAK &&
                            (b->bars < 4 || b->bars > 8 || b->section != SEC_BREAK || 100 * b->start < 43 * bars ||
                             100 * b->start > 65 * bars))
                            bad_break++;
                    }
                    if ((L & L_SNARE) && (L & L_HATS) && !(L & (L_KICK | L_BASS)) && i > 0 && (p.b[i - 1].layers & L_KICK)) {
                        nkick++;
                        if (b->bars != 8) bad_kickout++;
                    }
                    if (L & L_LEAD) lead_bars += b->bars;
                    mid_bars_all += b->bars;
                }
                if (i > 0) {
                    int a = p.b[i - 1].layers, d = a ^ L;
                    /* At most 2 of keys/kick/snare/hats/bass/lead/candy change per boundary. Exempt: the map's
                       mandated moves that need more, and nothing else may ride along with them: drums entering or
                       leaving (entry, break, Drift's comings and goings: only drum and bass bits change, one side
                       drumless) and the keys outro (everything but keys leaves, keys alone to the end). */
                    int drum_move = !(d & ~(DRUMS | L_BASS)) && (!(a & DRUMS) || !(L & DRUMS));
                    int keys_outro = i == p.n - 1 && L == L_KEYS && b->section == SEC_OUTRO;
                    if (bits(d) > 2 && !drum_move && !keys_outro) {
                        bad_step++;
                        if (bad_step < 4) printf("  step %02x->%02x map %d seed %d block %d\n", a, L, m->id, s, i);
                    }
                }
                if (plan_at(&p, b->start) != b || plan_at(&p, b->start + b->bars - 1) != b) bad_at++;
            }
            if (sum != bars) bad_cover++;
            if (plan_at(&p, -1) || plan_at(&p, bars)) bad_at++;
            if (m->intro_bars > 0 && m->entry != EN_FROM_TOP &&
                (p.b[0].layers != L_KEYS || !(p.b[0].flags & PB_DARK) || p.b[0].section != SEC_INTRO ||
                 p.b[0].bars != m->intro_bars))
                bad_intro++;
            if (m->entry == EN_FROM_TOP && (p.b[0].layers & (DRUMS | L_BASS)) != (DRUMS | L_BASS)) bad_entry++;
            if (m->entry == EN_TOGETHER && (kick_at != snare_at || hats_at - kick_at < 4 || hats_at - kick_at > 8))
                bad_entry++;
            if (m->entry == EN_SNARE_FIRST && (snare_at != hats_at || kick_at - snare_at != 8)) bad_entry++;
            if (m->middle == MID_BREAK && nbreak != 1) bad_break++;
            if (m->middle == MID_KICK_OUT && nkick != 1) bad_kickout++;
            if (m->ending == END_KEYS_OUTRO) {
                const plan_block_t *e = &p.b[p.n - 1];
                if (e->layers != L_KEYS || e->bars < 4 || e->bars > 8 || !(e->flags & PB_DARK) || e->section != SEC_OUTRO)
                    bad_outro++;
            }
            if (m->ending == END_FADE && !(p.b[p.n - 1].flags & PB_FADE)) bad_fade++;
            if (m->sparse && motion > 0 && drum_mid * 10 > mid * 6) bad_sparse++;
        }
    }
    CHECK(bad_cover == 0, "%d plans do not cover RF_BARS contiguously", bad_cover);
    CHECK(bad_grid == 0, "%d blocks off the 4-bar grid", bad_grid);
    CHECK(bad_lead0 == 0, "%d lead blocks at bar 0, in an intro or without keys", bad_lead0);
    CHECK(bad_bed == 0, "%d blocks set L_BED", bad_bed);
    CHECK(bad_intro == 0, "%d intros not keys-only, dark, map length", bad_intro);
    CHECK(bad_step == 0, "%d boundaries change more than 2 layers outside a mandated move", bad_step);
    CHECK(bad_break == 0, "%d MID_BREAK plans without exactly one 4-8 bar break at 43-65%%", bad_break);
    CHECK(bad_kickout == 0, "%d MID_KICK_OUT plans without exactly one 8-bar kick+bass-out block", bad_kickout);
    CHECK(bad_outro == 0, "%d END_KEYS_OUTRO plans without a 4-8 bar dark keys-only outro", bad_outro);
    CHECK(bad_fade == 0, "%d END_FADE plans without PB_FADE on the last block", bad_fade);
    CHECK(bad_entry == 0, "%d plans with a wrong drum/bass/hats entry", bad_entry);
    CHECK(bad_sparse == 0, "%d sparse plans with drums in over 60%% of middle bars", bad_sparse);
    CHECK(bad_candy == 0, "%d plans with candy on the lead's first statement", bad_candy);
    CHECK(bad_det == 0, "%d plans not deterministic", bad_det);
    CHECK(bad_m0 == 0, "%d Motion-0 plans with candy or drops", bad_m0);
    CHECK(bad_at == 0, "%d plan_at misses", bad_at);
    CHECK(bad_steady == 0, "R11: %d Slow bloom blocks with a drum move after the kick enters", bad_steady);
    CHECK(bad_hatsonly == 0, "R11: %d Straight in blocks with more than a hat drop-out", bad_hatsonly);
    CHECK(bad_sec == 0, "R12: %d middle blocks with the wrong break section", bad_sec);

    /* every song hears its lead: a middle (between intro and outro) of 16+ bars has lead in at least a quarter of
       it and at least 8 bars; every map, every length, Motion 0 and 100 */
    for (s = 1; s <= 2000; s++)
        for (mi = 0; mi < LIB_NMAPS; mi++) {
            int bars, motion;
            for (bars = 36; bars <= 76; bars += 4)
                for (motion = 0; motion <= 100; motion += 100) {
                    int mid = 0, lead = 0;
                    plan_roll(&r, (uint32_t)s, LIB_MAPS[mi].id, bars, motion);
                    plan_build(&r, &p);
                    for (i = 0; i < p.n; i++)
                        if (p.b[i].section != SEC_INTRO && p.b[i].section != SEC_OUTRO) {
                            mid += p.b[i].bars;
                            if (p.b[i].layers & L_LEAD) lead += p.b[i].bars;
                        }
                    if (mid >= 16 && (lead < 8 || 4 * lead < mid)) {
                        bad_lead++;
                        if (bad_lead < 4)
                            printf("  lead %d of %d: map %d seed %d bars %d motion %d\n", lead, mid, LIB_MAPS[mi].id,
                                   s, bars, motion);
                    }
                }
        }
    CHECK(bad_lead == 0, "%d songs with lead under a quarter of the middle or 8 bars", bad_lead);
    CHECK(mid_bars_all > 0 && lead_bars * 100 >= mid_bars_all * 30 && lead_bars * 100 <= mid_bars_all * 70,
          "lead in about half the middle: %ld of %ld bars", lead_bars, mid_bars_all);

    for (mi = 0; mi < LIB_NMAPS; mi++) {
        long lo = 0, hi = 0;
        for (s = 1; s <= 500; s++) {
            int bars = 36 + 4 * (s % 11);
            plan_roll(&r, (uint32_t)s, LIB_MAPS[mi].id, bars, 0);
            plan_build(&r, &p);
            lo += optional_moves(&p);
            plan_roll(&r, (uint32_t)s, LIB_MAPS[mi].id, bars, 100);
            plan_build(&r, &p);
            hi += optional_moves(&p);
        }
        CHECK(hi > lo, "map %d: optional moves at Motion 100 (%ld) not above Motion 0 (%ld)", LIB_MAPS[mi].id, hi, lo);
    }
}

/* ---- groove ---- */
#define SPS 7782.4f   /* one 16th at 85 BPM */

static void groove_roll(song_roll_t *r, uint32_t seed, int pattern)
{
    roll_song(NULL, seed, r);
    r->v[RF_PATTERN] = (int16_t)pattern;
    r->v[RF_TEMPO] = 85;
    r->v[RF_SWING] = 560;
}

static int upright_set(void)   /* a set whose bass may walk (the former upright sets) */
{
    int i;
    for (i = 0; i < LIB_NSETS; i++)
        if (lib_set_walks(&LIB_SETS[i])) return LIB_SETS[i].id;
    return -1;
}

static void test_groove(void)
{
    song_roll_t r, r2;
    plan_block_t all = {0, 64, SEC_GROOVE, 0xFF, 0}, nokick = {0, 64, SEC_GROOVE, 0xFF & ~L_KICK, 0};
    plan_block_t drop = {0, 8, SEC_GROOVE, 0xFF, PB_DROP_LAST}, fill = {0, 8, SEC_GROOVE, 0xFF, PB_FILL_LAST};
    song_ev_t ev[16], ev2[16];
    int bar, step, i, n, n2;
    int bad_snare = 0, bad_hat = 0, bad_even = 0, bad_odd = 0, kicks = 0, drops = 0, bad_det = 0;
    int bad_bass = 0, snare_n = 0, hat_n = 0;
    long snare_sum = 0, hat_sum = 0;
    uint32_t sw = (uint32_t)(0.12f * SPS + 0.5f);
    int up = upright_set(), styles[4] = {0, 0, 0, 0}, seed, fills_s = 0, fills_k = 0, s;

    groove_roll(&r, 7, 1);
    for (bar = 0; bar < 64; bar++)
        for (step = 0; step < 16; step++) {
            int chord[3] = {(bar * 5) % 12, (bar * 5 + 4) % 12, (bar * 5 + 7) % 12};
            uint32_t base = (step & 1) ? sw : 0;
            n = groove_step(&r, &all, bar, step, SPS, chord, 3, 99, ev, 16);
            n2 = groove_step(&r, &all, bar, step, SPS, chord, 3, 99, ev2, 16);
            if (n != n2 || memcmp(ev, ev2, sizeof(song_ev_t) * (size_t)n)) bad_det++;
            for (i = 0; i < n; i++) {
                uint32_t d = ev[i].delay;
                if (ev[i].vel < 25 || ev[i].vel > 110) bad_bass++;
                if (ev[i].voice == V_SNARE || ev[i].voice == V_HAT || ev[i].voice == V_OHAT || ev[i].voice == V_SHAKER) {
                    int ok = d + 1 >= base;
                    uint32_t late = ok ? d - base : 0;
                    if (ev[i].voice == V_SNARE) {
                        if (!ok || late + 1 < (uint32_t)(10.0f * 44.1f) || late > (uint32_t)(30.0f * 44.1f) + 1) bad_snare++;
                        if (ev[i].vel >= 50) { snare_sum += ev[i].vel; snare_n++; }
                    } else if (!ok || late > (uint32_t)(70.0f * 44.1f) + 1) bad_hat++;
                    if (ev[i].voice == V_HAT) { hat_sum += ev[i].vel; hat_n++; }
                } else if (ev[i].voice == V_KICK) {
                    if (d + 1 < base || d > base + 1) { if (step & 1) bad_odd++; else bad_even++; }
                }
            }
            /* on-beat 16ths: no swing at all on the kick and bass */
            for (i = 0; i < n; i++)
                if ((step & 1) == 0 && (ev[i].voice == V_KICK || ev[i].voice == V_BASS) && ev[i].delay != 0) bad_even++;
            n = groove_step(&r, &nokick, bar, step, SPS, chord, 3, 99, ev, 16);
            for (i = 0; i < n; i++) if (ev[i].voice == V_KICK) kicks++;
        }
    CHECK(bad_det == 0, "groove: %d steps differ between identical calls", bad_det);
    CHECK(bad_snare == 0, "groove: %d snare delays outside 10-30 ms", bad_snare);
    CHECK(bad_hat == 0, "groove: %d hat delays over 70 ms", bad_hat);
    CHECK(bad_even == 0 && bad_odd == 0, "groove: swing wrong (%d on-beat, %d off)", bad_even, bad_odd);
    CHECK(bad_bass == 0, "groove: %d velocities outside 25-110", bad_bass);
    CHECK(snare_n > 0 && hat_n > 0 && hat_sum * snare_n * 10 >= snare_sum * hat_n * 4 &&
          hat_sum * snare_n * 10 <= snare_sum * hat_n * 6, "groove: hat/snare velocity %ld/%d vs %ld/%d", hat_sum, hat_n,
          snare_sum, snare_n);
    CHECK(kicks == 0, "groove: %d kicks in a block without L_KICK", kicks);

    /* an off-16th kick (pattern 1 has kick on step 6? use pattern 2, step 7) carries exactly the swing delay */
    {
        int found = 0, wrong = 0;
        for (s = 0; s < 16; s += 1) {
            int chord[3] = {0, 4, 7};
            n = groove_step(&r, &all, 0, s, SPS, chord, 3, 5, ev, 16);
            for (i = 0; i < n; i++)
                if ((s & 1) && ev[i].voice == V_BASS) { found++; if (ev[i].delay + 1 < sw || ev[i].delay > sw + 1) wrong++; }
        }
        CHECK(wrong == 0, "groove: off-16th bass swing delay wrong");
        (void)found;
    }

    /* drop: no drums in the last bar, bass continues */
    for (seed = 1; seed < 40 && drops == 0; seed++) {
        int bassn = 0, drum = 0;
        groove_roll(&r2, (uint32_t)seed, 1);
        if (groove_bass_style(&r2) == BS_PEDAL) continue;
        for (step = 0; step < 16; step++) {
            int chord[3] = {0, 4, 7};
            n = groove_step(&r2, &drop, 7, step, SPS, chord, 3, 1, ev, 16);
            for (i = 0; i < n; i++) { if (ev[i].voice == V_BASS) bassn++; else drum++; }
        }
        CHECK(drum == 0, "groove: %d drum events in a dropped bar", drum);
        CHECK(bassn > 0, "groove: bass silent in a dropped bar");
        drops++;
    }

    /* fill: snares on 12-15 and a kick on 14 in the last bar; none in the bar before */
    groove_roll(&r2, 3, 1);
    for (step = 12; step < 16; step++) {
        int chord[3] = {0, 4, 7}, sn = 0, k = 0;
        n = groove_step(&r2, &fill, 7, step, SPS, chord, 3, 1, ev, 16);
        for (i = 0; i < n; i++) {
            if (ev[i].voice == V_SNARE && ev[i].vel >= 60 && ev[i].vel <= 90) sn++;
            if (ev[i].voice == V_KICK && step == 14) k++;
        }
        fills_s += sn >= 1;
        fills_k += k;
    }
    CHECK(fills_s == 4 && fills_k >= 1, "groove: fill has %d of 4 snare steps and %d kick on 14", fills_s, fills_k);

    /* bass styles over many seeds */
    for (seed = 1; seed <= 300; seed++) {
        int style, root_ok = 1, pedal_n = 0, bpc, sounded = 0;
        const lib_prog_t *pr;
        groove_roll(&r2, (uint32_t)seed, (seed % 8) + 1);
        if (up >= 0 && (seed & 1)) r2.v[RF_SET] = (int16_t)up;
        style = groove_bass_style(&r2);
        if (style < 0 || style > 3) { CHECK(0, "groove: bad style %d", style); continue; }
        styles[style]++;
        pr = lib_prog_by_id((uint8_t)r2.v[RF_PROG]);
        bpc = pr && pr->bars_per_chord ? pr->bars_per_chord : 1;
        for (bar = 0; bar < 16; bar++)
            for (step = 0; step < 16; step++) {
                int root = (bar / bpc * 7) % 12, chord[3];   /* a new chord every bpc bars, as harmony has it */
                chord[0] = root; chord[1] = (root + 4) % 12; chord[2] = (root + 7) % 12;
                n = groove_step(&r2, &all, bar, step, SPS, chord, 3, 2, ev, 16);
                for (i = 0; i < n; i++) {
                    int pc;
                    if (ev[i].voice != V_BASS) continue;
                    if (ev[i].note < 28 || ev[i].note > 47) root_ok = 0;
                    pc = ev[i].note % 12;
                    if (style != BS_WALK && pc != root && pc != (root + 7) % 12) root_ok = 0;
                    if (style == BS_PEDAL) {
                        /* R21: one note at each chord change, the root, held for the chord's length (rounding
                           aside), so no bar is silent */
                        float want = (float)(16 * bpc) * SPS, got = (float)ev[i].len;
                        pedal_n++;
                        if (bar % bpc || step != 0 || pc != root || got < want - 1.0f || got > want + 1.0f)
                            root_ok = 0;
                        else
                            sounded |= ((1 << bpc) - 1) << bar;
                    }
                    if (ev[i].len == 0) root_ok = 0;
                }
            }
        CHECK(root_ok, "groove: seed %d style %d bass notes out of range, off the chord or mis-timed", seed, style);
        if (style == BS_PEDAL)
            CHECK(pedal_n == 16 / bpc && sounded == 0xFFFF, "groove: seed %d pedal emitted %d notes in 16 bars "
                  "(%d bars a chord), bars sounded %04x", seed, pedal_n, bpc, sounded);
    }
    CHECK(styles[BS_ROOTS] > 0 && styles[BS_ROOT_FIFTH] > 0 && styles[BS_WALK] > 0 && styles[BS_PEDAL] > 0,
          "groove: bass styles drawn %d/%d/%d/%d", styles[0], styles[1], styles[2], styles[3]);
}

static int first_step(int st) { return st == CS_STABS ? 0 : 2; }

static int has_iv(const int *pcs, int n, int iv);

static void test_harmony(void)
{
    song_roll_t r;
    int pi, key, mode, color, bar, i, seed, pass, step;
    int bad_pass1 = 0, reharm = 0, bad_dark = 0, bad_range = 0, bad_roll = 0, bad_vel = 0, bad_det = 0;
    int styles[5] = {0, 0, 0, 0, 0}, chords_seen = 0;
    plan_block_t keys = {0, 8, SEC_GROOVE, L_KEYS, 0}, dark = {0, 4, SEC_INTRO, L_KEYS, PB_DARK};
    plan_block_t nokeys = {0, 8, SEC_GROOVE, L_BASS, 0};
    song_ev_t ev[16], ev2[16];
    const float SPS16 = 5512.5f;   /* a 16th at 90 BPM is 0.1667 s */

    memset(&r, 0, sizeof r);
    for (pi = 0; pi < LIB_NPROGS; pi++)
        for (key = 0; key < 12; key++)
            for (mode = 0; mode < MODE_N; mode++)
                for (color = 0; color < 3; color++) {
                    const lib_prog_t *p = &LIB_PROGS[pi];
                    r.v[RF_KEY] = (int16_t)key; r.v[RF_MODE] = (int16_t)mode; r.v[RF_PROG] = p->id;
                    r.v[RF_COLOR] = (int16_t)color; r.seed = (uint32_t)(pi * 7 + key);
                    for (bar = 0; bar < 16; bar++) {
                        int got[5], want[5], ng, nw;
                        const th_chord_spec_t c = p->c[(bar / p->bars_per_chord) % p->n];
                        ng = harmony_chord_at(&r, bar, 1, 0, got);
                        nw = th_chord_pcs(key, mode, c, COLOR_7, want);
                        if (has_iv(want, nw, 4) && has_iv(want, nw, 10)) {
                            /* a dominant: its own root and 3rd, its 7th unless it fell back to a triad (colors are
                             * test_dominant_extensions' business) */
                            if (ng < 3 || got[0] != want[0] || !has_iv(got, ng, 4) || (ng > 3 && !has_iv(got, ng, 10)))
                                bad_pass1++;
                            continue;
                        }
                        nw = th_chord_pcs(key, mode, c, color, want);
                        if (ng != nw || memcmp(got, want, sizeof(int) * (size_t)nw)) bad_pass1++;
                    }
                }
    CHECK(bad_pass1 == 0, "harmony: %d pass-1 chords differ from the progression", bad_pass1);

    /* reharmonize on repeat: over 200 seeds some pass >= 2 chord differs; at most one chord index per pass */
    for (seed = 1; seed <= 200; seed++) {
        const lib_prog_t *p;
        roll_song(NULL, (uint32_t)seed, &r);
        p = lib_prog_by_id((uint8_t)r.v[RF_PROG]);
        for (pass = 2; pass <= 4; pass++) {
            int diff_idx = 0;
            for (i = 0; i < p->n; i++) {
                int a[5], b[5], na = harmony_chord_at(&r, i * p->bars_per_chord, 1, 0, a);
                int nb = harmony_chord_at(&r, i * p->bars_per_chord, pass, 0, b);
                int same_bars = 1;
                if (na != nb || memcmp(a, b, sizeof(int) * (size_t)na)) { diff_idx++; }
                for (bar = 1; bar < p->bars_per_chord; bar++) {
                    int c1[5], c2[5];
                    int n1 = harmony_chord_at(&r, i * p->bars_per_chord + bar, pass, 0, c1);
                    int n2 = harmony_chord_at(&r, i * p->bars_per_chord, pass, 0, c2);
                    if (n1 != n2 || memcmp(c1, c2, sizeof(int) * (size_t)n1)) same_bars = 0;
                }
                if (!same_bars) bad_det++;
            }
            CHECK(diff_idx <= 1, "harmony: seed %d pass %d reharmonizes %d chords", seed, pass, diff_idx);
            reharm += diff_idx;
        }
    }
    CHECK(reharm > 0, "harmony: no reharmonized chord in 200 seeds");
    CHECK(reharm > 40 && reharm < 400, "harmony: reharmonize count %d implausible", reharm);
    CHECK(bad_det == 0, "harmony: a chord changes inside its own bars");

    /* dark (intro, outro) is a sound, not a voicing (Sam: lo-fi keeps its voicings through the song): a dark block
       voices every chord exactly as the same block unmarked, only softer; all events 48-72, rolled 10-60 ms apart,
       velocities 25-110 */
    for (seed = 1; seed <= 120; seed++) {
        harmony_voicing_t prev, prev2, same;
        memset(&prev, 0, sizeof prev);
        memset(&prev2, 0, sizeof prev2);
        memset(&same, 0, sizeof same);
        roll_song(NULL, (uint32_t)seed, &r);
        styles[harmony_style(&r)]++;
        for (bar = 0; bar < 8; bar++)
            for (step = 0; step < 16; step++) {
                const plan_block_t *b = (bar < 4) ? &dark : &keys;
                harmony_voicing_t *vs = (bar < 4) ? &prev : &prev2;
                int ne = harmony_step(&r, b, bar, step, 1 + bar / 4, SPS16, vs, 0, 3u, ev, 16);
                if (b == &dark) {                      /* the same block unmarked, from the same voicing */
                    plan_block_t lit = dark;
                    int ne2;
                    lit.flags = 0;
                    ne2 = harmony_step(&r, &lit, bar, step, 1 + bar / 4, SPS16, &same, 0, 3u, ev2, 16);
                    if (ne2 != ne) bad_dark++;
                    for (i = 0; i < ne && i < ne2; i++)
                        if (ev[i].note != ev2[i].note || ev[i].delay != ev2[i].delay || ev[i].len != ev2[i].len)
                            bad_dark++;
                }
                for (i = 0; i < ne; i++) {
                    if (ev[i].voice != V_CHORD) bad_range++;
                    {                          /* a pad with or above the lead sounds an octave up (lead design) */
                        int up = (harmony_pad_place(&r) == PL_BELOW ? 0 : 12) +
                                 lib_set_keys_shift(lib_set_by_id((uint8_t)r.v[RF_SET]));   /* (Sam: the Chillwave pad lower) */
                        if (ev[i].note < 48 + up || ev[i].note > (harmony_style(&r) == CS_STRUM ? 76 : 72) + up)
                            bad_range++;               /* (a guitar shape reaches E5) */
                    }
                    if (ev[i].vel < 25 || ev[i].vel > 110) bad_vel++;
                    if (ev[i].len == 0) bad_range++;
                }
                if (harmony_style(&r) != CS_ARP && harmony_style(&r) != CS_STRUM && ne > 0) {   /* (test_strum) */
                    chords_seen++;
                    if (ne != 4) bad_roll++;
                    for (i = 1; i < ne; i++) {
                        uint32_t gap = ev[i].delay - ev[i - 1].delay;
                        if (ev[i].delay <= ev[i - 1].delay || ev[i].note <= ev[i - 1].note) bad_roll++;
                        if (gap < 441 || gap > 2646) bad_roll++;
                    }
                    if (ev[0].delay != 0) bad_roll++;
                }
            }
    }
    CHECK(bad_range == 0, "harmony: %d events out of range or wrong voice", bad_range);
    CHECK(bad_dark == 0, "harmony: %d dark-block notes voiced differently from the same block unmarked", bad_dark);
    CHECK(bad_vel == 0, "harmony: %d velocities outside 25-110", bad_vel);
    CHECK(bad_roll == 0, "harmony: %d rolled-chord violations", bad_roll);
    CHECK(chords_seen > 0, "harmony: no chord events");
    CHECK(styles[CS_HELD] > 0 && styles[CS_OFFBEATS] == 0 && styles[CS_ARP] > 0 && styles[CS_HALF] > 0,
          "harmony: styles drawn %d/%d/%d/%d/%d", styles[0], styles[1], styles[2], styles[3], styles[4]);
    {   /* (Sam, after hearing comping-styles.musicxml) mostly held: 44% held drawn (about 55% heard, once sustained keys and a kick on 1 and 3 hold), 20% arpeggio, 36% half drawn; offbeats
           and stabs retired (Sam) */
        int w[5] = {0, 0, 0, 0, 0}, want[5] = {44, 0, 0, 20, 36}, k2;
        song_roll_t q;
        roll_song(NULL, 1u, &q);
        for (k2 = 0; k2 < 10000; k2++) {
            q.seed = (uint32_t)k2 * 2654435761u + 1u;
            w[harmony_style(&q)]++;
        }
        for (k2 = 0; k2 < 5; k2++)
            CHECK(w[k2] >= want[k2] * 100 - 200 && w[k2] <= want[k2] * 100 + 200,
                  "harmony: style %d drawn %d of 10000, want %d%% (+-2)", k2, w[k2], want[k2]);
        {   /* songs that really play half notes, after the sustained-keys and kick-on-1-and-3 holds: 25% (+-4) */
            int play = 0;
            for (k2 = 1; k2 <= 3000; k2++) {
                roll_song(NULL, (uint32_t)k2, &q);
                play += harmony_half_strikes(&q);
            }
            CHECK(play >= 3000 * 21 / 100 && play <= 3000 * 29 / 100,
                  "harmony: %d of 3000 songs play half notes, want 25%% (+-4)", play);
        }
    }

    /* style shapes, no events without L_KEYS, determinism */
    for (seed = 1; seed <= 60; seed++) {
        int count[16], bpc, st, total = 0, held_bar = 0, bar_notes[4] = {0, 0, 0, 0};
        const lib_prog_t *p;
        harmony_voicing_t vs, vsb;
        memset(&vs, 0, sizeof vs);
        memset(&vsb, 0, sizeof vsb);
        roll_song(NULL, (uint32_t)seed, &r);
        p = lib_prog_by_id((uint8_t)r.v[RF_PROG]);
        bpc = p->bars_per_chord;
        st = harmony_style(&r);
        memset(count, 0, sizeof count);
        for (bar = 0; bar < 8; bar++)
            for (step = 0; step < 16; step++) {
                int a = harmony_step(&r, &keys, bar, step, 1, SPS16, &vs, 0, 9u, ev, 16);
                int b2 = harmony_step(&r, &keys, bar, step, 1, SPS16, &vsb, 0, 9u, ev2, 16);
                if (a != b2 || memcmp(ev, ev2, sizeof(song_ev_t) * (size_t)a) || memcmp(&vs, &vsb, sizeof vs)) bad_det++;
                count[step] += a;
                total += a;
                if (st == CS_HELD && a > 0) {
                    if (step != 0 || bar % bpc != 0) held_bar++;
                    for (i = 0; i < a; i++) {
                        uint32_t want = (uint32_t)((float)(bpc * 16) * SPS16) - 441u;
                        uint32_t tot = ev[i].len + ev[i].delay;
                        if (tot + 2 < want || tot > want + 2) held_bar++;
                    }
                }
                for (i = 0; i < a; i++) {
                    float end = (float)(bar * 16 + step) * SPS16 + (float)ev[i].delay + (float)ev[i].len;
                    float lim = (float)((bar / bpc + 1) * bpc * 16) * SPS16;
                    if (end > lim + 2.0f) held_bar++;
                    if (st >= CS_STABS && st <= CS_OFFBEATS && a == 4 && ev[i].note != bar_notes[i]) {
                        if (step == first_step(st)) bar_notes[i] = ev[i].note;
                        else held_bar++;
                    }
                }
                if (a == 4 && step == first_step(st)) for (i = 0; i < 4; i++) bar_notes[i] = ev[i].note;
                if (harmony_step(&r, &nokeys, bar, step, 1, SPS16, &vs, 0, 9u, ev, 16) != 0) bad_det++;
            }
        CHECK(held_bar == 0, "harmony: held chords wrong (seed %d)", seed);
        if (st == CS_HELD) CHECK(total == 4 * (8 / bpc), "harmony: held count %d", total);
        if (st == CS_STABS) {                    /* 1, and the step the kick leaves free (harmony_stab_steps) */
            unsigned sm = harmony_stab_steps(&r);
            int s2 = 0;
            while (s2 < 15 && !((sm >> ++s2) & 1u))
                ;
            CHECK(count[0] == 32 && count[s2] == 32 && total == 64, "harmony: stabs %d/%d/%d", count[0], count[s2], total);
        }
        if (st == CS_OFFBEATS) CHECK(count[2] == 32 && count[6] == 32 && count[10] == 32 && count[14] == 32 && total == 128,
                                     "harmony: offbeats total %d", total);
        if (st == CS_ARP) {                    /* (Sam) a 16th-note arpeggio: in 8ths it reads as a melody */
            int every = 1;
            for (step = 0; step < 16; step++) if (count[step] != 8) every = 0;
            CHECK(every && total == 128, "harmony: arp in 16ths: total %d", total);
        }
    }
    CHECK(bad_det == 0, "harmony: %d determinism or L_KEYS violations", bad_det);
}

/* Half-note comping (pads revision 2): the voicing struck on beats 1 and 3 (steps 0 and 8), the second a little softer,
   each held to the next strike, the same voicing both times (a request mid-chord, the loop lock off, included); a busy
   lead thins it to step 0 of every bar, held a bar; sustained keys hold. */
static void test_comp_styles(void)
{
    static const uint8_t OLD[5] = {44, 0, 0, 20, 36};
    static const uint8_t W[3][6] = {{44, 0, 0, 20, 36, 0}, {35, 25, 0, 15, 25, 0}, {20, 20, 0, 15, 15, 30}};
    static const uint8_t SLOW[6] = {0, 20, 0, 15, 35, 30};
    static const int inss[3] = {INS_DX_EP, INS_KNIGHT, INS_EMILY};
    long cnt[3][6] = {{0}}, gcnt[6] = {0}, scnt[6] = {0}, ecnt[6] = {0};
    song_roll_t r;
    int k, i, seed, n = 500, bad_old = 0, bad_gtr = 0;
    CHECK(lib_ins_sampled_struck(INS_KNIGHT) && lib_ins_sampled_struck(INS_NYLON) &&
          lib_ins_sampled_struck(INS_EMILY) && !lib_ins_sampled_struck(INS_DX_EP) &&
          !lib_ins_sampled_struck(INS_PAD), "comp styles: lib_ins_sampled_struck");
    CHECK(CS_STRUM > CS_HALF, "comp styles: CS_STRUM appended after CS_HALF");
    for (seed = 1; seed <= n; seed++) {
        roll_song(0, (uint32_t)seed, &r);
        for (k = 0; k < LIB_NSETS; k++) {
            const lib_set_t *s = &LIB_SETS[k];
            rng_t g;
            int want, got;
            r.v[RF_SET] = s->id;
            got = harmony_style(&r);
            CHECK(got != CS_OFFBEATS, "comp styles: offbeats never (set %d)", s->id);
            if (s->chords == INS_NYLON) {
                CHECK(got != CS_OFFBEATS && got <= CS_STRUM, "comp styles: nylon style in range");
                continue;
            }
            if (s->chords == INS_KNIGHT || s->chords == INS_EMILY) continue;
            rng_seed(&g, (uint32_t)seed, 121);
            want = rng_weighted(&g, OLD, 5);
            bad_old += got != want;
            CHECK(got != CS_STABS && got != CS_STRUM, "comp styles: synth keys never stabs or strum (set %d)", s->id);
        }
        for (i = 0; i < 3; i++) {
            int st = harmony_style_ins(inss[i], 90, (uint32_t)seed);
            cnt[i][st]++;
            if (i == 0 && st != harmony_style_ins(INS_DX_EP, 70, (uint32_t)seed)) bad_old++;
            if (i == 0 && (st == CS_STABS || st == CS_STRUM)) bad_old++;
            if (i && st == CS_OFFBEATS) bad_gtr++;
            if (i == 1 && st == CS_STRUM) bad_gtr++;
        }
        gcnt[harmony_style_ins(INS_NYLON, 90, (uint32_t)seed)]++;
        scnt[harmony_style_ins(INS_NYLON, 83, (uint32_t)seed)]++;
        ecnt[harmony_style_ins(INS_EMILY, 70, (uint32_t)seed)]++;
    }
    CHECK(bad_old == 0, "comp styles: %d synth-keys songs changed style", bad_old);
    CHECK(bad_gtr == 0, "comp styles: %d offbeats or Knight strums", bad_gtr);
    for (i = 0; i < 3; i++) {
        int tot = 0;
        for (k = 0; k < 6; k++) tot += W[i][k];
        for (k = 0; k < 6; k++) {
            long pct = cnt[i][k] * 100 / n, want = W[i][k] * 100 / tot;
            CHECK(pct >= want - 5 && pct <= want + 5, "comp styles: ins %d style %d is %ld%%, wanted %ld%%", i, k, pct, want);
        }
    }
    for (k = 0; k < 6; k++) {
        long pct = gcnt[k] * 100 / n, want = W[2][k];
        CHECK(pct >= want - 5 && pct <= want + 5, "comp styles: nylon style %d is %ld%%, wanted %ld%%", k, pct, want);
        pct = scnt[k] * 100 / n; want = SLOW[k];
        CHECK(pct >= want - 5 && pct <= want + 5, "comp styles: slow nylon style %d is %ld%%, wanted %ld%%", k, pct, want);
        pct = ecnt[k] * 100 / n;
        CHECK(pct >= want - 5 && pct <= want + 5, "comp styles: slow emily style %d is %ld%%, wanted %ld%%", k, pct, want);
    }
    CHECK(scnt[CS_HELD] == 0 && ecnt[CS_HELD] == 0, "comp styles: no held guitar below 84 BPM");
    for (seed = 1; seed <= 200; seed++) {          /* through the roll: slow guitar songs never hold */
        roll_song(0, (uint32_t)seed, &r);
        for (k = 0; k < LIB_NSETS; k++) {
            r.v[RF_SET] = LIB_SETS[k].id;
            if (LIB_SETS[k].chords == INS_NYLON && r.v[RF_TEMPO] < 84)
                CHECK(harmony_style(&r) != CS_HELD, "comp styles: held nylon at %d BPM", r.v[RF_TEMPO]);
        }
    }
}

static void test_half_comping(void)
{
    const float SPS16 = 5512.5f;
    plan_block_t keys = {0, 8, SEC_GROOVE, L_KEYS, 0};
    song_roll_t r;
    song_ev_t e0[16], e1[16], eb[16];
    int seed, lock, halves = 0, sustained = 0, bpc2 = 0, bad_step = 0, bad_vel = 0, bad_len = 0, bad_same = 0;
    int bad_busy = 0, bad_sust = 0;
    for (lock = 1; lock >= 0; lock--) {
        harmony_set_loop_lock(lock);
        for (seed = 1; seed <= 400; seed++) {
            const lib_prog_t *p;
            const lib_set_t *set;
            harmony_voicing_t vs, vb;
            int bar, step, sust;
            roll_song(NULL, (uint32_t)seed, &r);
            if (harmony_style(&r) != CS_HALF) continue;
            p = lib_prog_by_id((uint8_t)r.v[RF_PROG]);
            set = lib_set_by_id((uint8_t)r.v[RF_SET]);
            sust = harmony_comp_steps(&r) == 0;   /* held instead: sustained keys, or a kick on 1 and 3 alone */
            memset(&vs, 0, sizeof vs);
            memset(&vb, 0, sizeof vb);
            if (lock) { halves++; sustained += set && lib_ins_sustained(set->chords); bpc2 += p->bars_per_chord == 2; }
            for (bar = p->bars_per_chord > 1 ? 1 : 0; bar < 8; bar++) {   /* a request mid-chord: begins at bar 1 */
                int n0 = harmony_step(&r, &keys, bar, 0, 1, SPS16, &vs, 0, 9u, e0, 16);
                int n1;
                for (step = 1; step < 16; step++) {
                    song_ev_t ex[16];
                    int nx = harmony_step(&r, &keys, bar, step, 1, SPS16, &vs, 0, 9u, step == 8 ? e1 : ex, 16);
                    if (step == 8) { n1 = nx; continue; }
                    if (nx) bad_step++;
                }
                if (n0 != 4 || n1 != 4) {
                    if (!sust) bad_step++;
                    continue;
                }
                if (sust) bad_sust++;
                {
                    int i;
                    for (i = 0; i < 4; i++) {
                        float want = 8.0f * SPS16 - 441.0f;
                        if (e1[i].note != e0[i].note || e1[i].delay != e0[i].delay) bad_same++;
                        if (e1[i].vel >= e0[i].vel) bad_vel++;
                        if ((float)(e0[i].len + e0[i].delay) < want - 2.0f || (float)(e0[i].len + e0[i].delay) > want + 2.0f
                            || (float)(e1[i].len + e1[i].delay) < want - 2.0f
                            || (float)(e1[i].len + e1[i].delay) > want + 2.0f) bad_len++;
                    }
                }
                for (step = 0; step < 16; step++) {              /* busy: step 0 of every bar, held a bar, same voicing */
                    int nb = harmony_step(&r, &keys, bar, step, 1, SPS16, &vb, 1, 9u, eb, 16), i;
                    if (nb && (step != 0 || nb != 4)) bad_busy++;
                    if (!nb && step == 0) bad_busy++;
                    for (i = 0; nb == 4 && i < 4; i++) {
                        if (eb[i].len + eb[i].delay < (uint32_t)(15.0f * SPS16)) bad_busy++;
                        if (lock && (eb[i].note != e0[i].note || eb[i].vel != e0[i].vel || eb[i].delay != e0[i].delay))
                            bad_busy++;
                    }
                }
            }
        }
    }
    harmony_set_loop_lock(1);
    CHECK(halves > 20 && sustained > 0 && bpc2 > 0, "half comping: %d songs, %d over sustained keys, %d two-bar", halves, sustained, bpc2);
    CHECK(bad_step == 0, "half comping: %d strikes off steps 0 and 8 (or missing)", bad_step);
    CHECK(bad_sust == 0, "half comping: %d second strikes over sustained keys", bad_sust);
    CHECK(bad_vel == 0, "half comping: %d second strikes not softer", bad_vel);
    CHECK(bad_len == 0, "half comping: %d strikes not held to the next", bad_len);
    CHECK(bad_same == 0, "half comping: %d notes revoiced between strikes", bad_same);
    CHECK(bad_busy == 0, "half comping: %d busy bars not thinned to step 0 of every bar, held a bar", bad_busy);
}

/* Scale-degree index of an in-scale note (octave * 7 + position), counted from the key. */
static int deg_of(int key, int mode, int note)
{
    int d = 0, n;
    for (n = key - 12; n < note; n++) if (th_in_scale(key, mode, n)) d++;
    return d;
}

/* The 4-bar lead phrases of a plan, in song order (trading fours: lead, candy, lead... in blocks with both). */
static int lead_phrase_starts(const song_plan_t *p, int *starts)
{
    int i, j, n = 0;
    for (i = 0; i < p->n; i++) {
        const plan_block_t *b = &p->b[i];
        if (!(b->layers & L_LEAD)) continue;
        for (j = 0; j < b->bars / 4; j++)
            if (!(b->layers & L_CANDY) || (j & 1) == 0) starts[n++] = b->start + 4 * j;
    }
    return n;
}

typedef struct { int bar, step, voice, note; float on, off; } mel_note_t;

/* R25: pc is in the chord-scale: a chord tone, or a scale note no chord tone lies a semitone from */
static int in_chord_scale(int key, int mode, const int *pcs, int n, int note)
{
    int k, pc = note % 12, near = 0;
    for (k = 0; k < n; k++) {
        int d = ((pcs[k] - pc) % 12 + 12) % 12;
        if (d == 0) return 1;
        if (d == 1 || d == 11) near = 1;
    }
    return !near && th_in_scale(key, mode, pc);
}

static void test_melody(void)
{
    static mel_note_t ns[76 * 16 * 2];
    song_roll_t r, r2;
    song_plan_t p;
    melody_phrases_t ph;
    motif_t m, m2;
    song_ev_t ev[8], ev2[8];
    int seed, bar, step, i, j;
    int bad_scale = 0, bad_strong = 0, bad_avoid = 0, bad_lead_reg = 0, bad_candy_reg = 0, bad_both = 0;
    int bad_late = 0, bad_vel = 0, bad_cross = 0, bad_overlap = 0, bad_det = 0, bad_ends = 0, bad_state = 0;
    int bad_motif = 0, bad_voice = 0, bad_trade = 0, trade_n = 0;
    int kinds[3] = {0, 0, 0}, lead_n = 0, candy_n = 0, candy_only_n = 0, phrases = 0;

    for (seed = 1; seed <= 1000; seed++) {
        int starts[32], np, nn = 0, mb, bpc, loop;
        float sps;
        const lib_prog_t *pr;
        roll_song(NULL, (uint32_t)seed, &r);
        plan_build(&r, &p);
        pr = lib_prog_by_id((uint8_t)r.v[RF_PROG]);
        bpc = pr->bars_per_chord;
        loop = pr->n * bpc;
        sps = 44100.0f * 60.0f / (float)r.v[RF_TEMPO] / 4.0f;
        melody_motif(&r, r.seed, &m);
        melody_plan(&r, &p, &ph);
        memcpy(&r2, &r, sizeof r);
        melody_motif(&r2, r.seed, &m2);
        if (memcmp(&m, &m2, sizeof m)) bad_det++;
        if (m.n < 3 || m.n > 8) bad_motif++;
        mb = 1;
        for (i = 0; i < m.n; i++) {
            if (m.at[i] >= 32 || (i > 0 && m.at[i] <= m.at[i - 1]) || m.len[i] == 0) bad_motif++;
            if (m.at[i] >= 16) mb = 2;
        }
        if (m.n > 0 && m.step[0] != 0) bad_motif++;

        for (bar = 0; bar < p.bars; bar++) {
            const plan_block_t *b = plan_at(&p, bar);
            int pass = bar / loop + 1, pcs[5], npc, dark = (b->flags & PB_DARK) != 0;
            npc = harmony_chord_at(&r, bar, pass, dark, pcs);
            for (step = 0; step < 16; step++) {
                int ne = melody_step(&r, &p, &ph, &m, bar, step, sps, r.seed, ev, 8), hasl = 0, hasc = 0;
                int ne2 = melody_step(&r, &p, &ph, &m, bar, step, sps, r.seed, ev2, 8);
                if (ne != ne2 || memcmp(ev, ev2, sizeof(song_ev_t) * (size_t)ne)) bad_det++;
                for (i = 0; i < ne; i++) {
                    int n = ev[i].note, k, ct = 0;
                    float on = (float)(bar * 16 + step) * sps + (float)ev[i].delay;
                    float lim = (float)((bar / bpc + 1) * bpc * 16) * sps;
                    if (ev[i].voice == V_LEAD) { hasl = 1; lead_n++; if (n < 62 || n > 86) bad_lead_reg++; }
                    else if (ev[i].voice == V_CANDY) {
                        hasc = 1; candy_n++;
                        if (!(b->layers & L_LEAD)) candy_only_n++;
                        if (n < 70 || n > 98) bad_candy_reg++;
                    } else bad_voice++;
                    if (!in_chord_scale(r.v[RF_KEY], r.v[RF_MODE], pcs, npc, n)) bad_scale++;
                    for (k = 0; k < npc; k++) if (pcs[k] == n % 12) ct = 1;
                    if ((step == 0 || step == 8) && !ct) bad_strong++;
                    if (th_is_avoid(r.v[RF_KEY], r.v[RF_MODE], pcs, npc, n)) bad_avoid++;
                    if (ev[i].delay < 220 || ev[i].delay > 1103) bad_late++;
                    if (ev[i].vel < 25 || ev[i].vel > 110) bad_vel++;
                    if (ev[i].len == 0 || on + (float)ev[i].len > lim + 1.0f) bad_cross++;
                    if (!(b->layers & (ev[i].voice == V_LEAD ? L_LEAD : L_CANDY))) bad_voice++;
                    if ((b->layers & L_LEAD) && (b->layers & L_CANDY)) {   /* trading fours, lead first */
                        int cphr = ((bar - b->start) / 4) & 1;
                        if (cphr != (ev[i].voice == V_CANDY)) bad_trade++;
                        if (cphr) trade_n++;
                    }
                    if (nn < (int)(sizeof ns / sizeof ns[0])) {
                        ns[nn].bar = bar; ns[nn].step = step; ns[nn].voice = ev[i].voice; ns[nn].note = n;
                        ns[nn].on = on; ns[nn].off = on + (float)ev[i].len; nn++;
                    }
                }
                if (hasl && hasc) bad_both++;
            }
        }
        for (i = 0; i < nn; i++)
            for (j = 0; j < nn; j++)
                if (ns[i].voice == V_LEAD && ns[j].voice == V_CANDY && ns[i].on < ns[j].off && ns[j].on < ns[i].off)
                    bad_overlap++;

        /* phrase plan: head-solos-head, kinds counted, exact restatement of the motif */
        np = lead_phrase_starts(&p, starts);
        phrases += np;
        for (i = 0; i < np; i++) {
            if (ph.kind[i] > PH_NEW) { bad_ends++; continue; }
            kinds[ph.kind[i]]++;
        }
        if (np >= 1 && ph.kind[0] != PH_STATE) bad_ends++;
        if (np >= 2 && ph.kind[np - 1] != PH_STATE) bad_ends++;
        for (i = 0; i < np; i++) {
            int got = 0, d0 = 0, ok = 1;
            if (ph.kind[i] != PH_STATE) continue;
            for (j = 0; j < nn; j++) {
                int rel = (ns[j].bar - starts[i]) * 16 + ns[j].step;
                if (ns[j].voice != V_LEAD || rel < 0 || rel >= mb * 16) continue;
                if (got >= m.n) { ok = 0; break; }
                if (got == 0) d0 = deg_of(r.v[RF_KEY], r.v[RF_MODE], ns[j].note);
                if (rel != m.at[got] || deg_of(r.v[RF_KEY], r.v[RF_MODE], ns[j].note) - d0 != m.step[got]) ok = 0;
                if (got == 0 && ns[j].note != m.first) ok = 0;
                got++;
            }
            if (got != m.n || !ok) bad_state++;
        }
    }
    CHECK(bad_motif == 0, "melody: %d motif shape violations", bad_motif);
    CHECK(bad_scale == 0, "melody: %d notes out of the chord-scale", bad_scale);
    CHECK(bad_strong == 0, "melody: %d notes on steps 0/8 not chord tones", bad_strong);
    CHECK(bad_avoid == 0, "melody: %d avoid notes", bad_avoid);
    CHECK(bad_lead_reg == 0, "melody: %d lead notes outside 62-86", bad_lead_reg);
    CHECK(bad_candy_reg == 0, "melody: %d candy notes outside 70-98", bad_candy_reg);
    CHECK(bad_both == 0, "melody: %d steps with both lead and candy", bad_both);
    CHECK(bad_overlap == 0, "melody: %d lead/candy overlaps", bad_overlap);
    CHECK(bad_late == 0, "melody: %d notes not 5-25 ms late", bad_late);
    CHECK(bad_vel == 0, "melody: %d velocities outside 25-110", bad_vel);
    CHECK(bad_cross == 0, "melody: %d notes across a chord change", bad_cross);
    CHECK(bad_voice == 0, "melody: %d events with a wrong voice or layer", bad_voice);
    CHECK(bad_det == 0, "melody: %d determinism violations", bad_det);
    CHECK(bad_trade == 0 && trade_n > 0, "melody: trading fours broken %d times (%d candy answers)", bad_trade, trade_n);
    CHECK(bad_ends == 0, "melody: %d songs not head-solos-head", bad_ends);
    CHECK(bad_state == 0, "melody: %d PH_STATE phrases not an exact restatement", bad_state);
    CHECK(phrases > 0 && kinds[PH_STATE] * 100 >= 40 * phrases && kinds[PH_STATE] * 100 <= 60 * phrases,
          "melody: restatements %d of %d phrases", kinds[PH_STATE], phrases);
    CHECK(phrases > 0 && kinds[PH_VARIANT] * 100 >= 10 * phrases && kinds[PH_VARIANT] * 100 <= 20 * phrases,
          "melody: variants %d of %d phrases", kinds[PH_VARIANT], phrases);
    CHECK(lead_n > 0 && candy_n > 0 && candy_only_n > 0, "melody: lead %d candy %d (candy-only %d) notes",
          lead_n, candy_n, candy_only_n);
    printf("melody: %d phrases, state %d variant %d new %d; lead %d candy %d notes\n", phrases, kinds[0], kinds[1],
           kinds[2], lead_n, candy_n);
}

static uint32_t vt_seed = 12345u;
static uint32_t vt_rnd(void)
{
    vt_seed = vt_seed * 1664525u + 1013904223u;
    return vt_seed >> 8;
}

static void vt_ev(int voice, int note, int vel, uint32_t delay, uint32_t len)
{
    song_ev_t e;
    memset(&e, 0, sizeof e);
    e.voice = (uint8_t)voice;
    e.note = (uint8_t)note;
    e.vel = (uint8_t)vel;
    e.delay = delay;
    e.len = len;
    vx_event(&e);
}

static float vt_l[4096], vt_r[4096];

static void test_voices(void)
{
    int i, step, k, nan = 0;
    float peak = 0.0f;
    uint32_t first;

    /* one kick: silent before its delay, audible at it (+-1 sample) */
    vx_init();
    vx_set_gain(1.0f);
    vt_ev(V_KICK, 0, 100, 1000, 0);
    memset(vt_l, 0, sizeof vt_l);
    memset(vt_r, 0, sizeof vt_r);
    vx_render(vt_l, vt_r, 3000);
    first = 0xFFFFFFFFu;
    for (i = 0; i < 3000; i++)
        if (vt_l[i] != 0.0f || vt_r[i] != 0.0f) {
            first = (uint32_t)i;
            break;
        }
    CHECK(first >= 999 && first <= 1001, "kick starts at sample %u, wanted 1000 +-1", (unsigned)first);
    {
        float m = 0.0f;
        for (i = 1000; i < 3000; i++)
            if (vt_l[i] > m)
                m = vt_l[i];
        CHECK(m > 0.05f, "kick is audible (peak %.3f)", (double)m);
    }

    /* the same kick split over small blocks lands on the same sample */
    vx_init();
    vt_ev(V_KICK, 0, 100, 1000, 0);
    first = 0xFFFFFFFFu;
    for (k = 0; k < 40; k++) {
        memset(vt_l, 0, 100 * sizeof(float));
        memset(vt_r, 0, 100 * sizeof(float));
        vx_render(vt_l, vt_r, 100);
        for (i = 0; i < 100 && first == 0xFFFFFFFFu; i++)
            if (vt_l[i] != 0.0f)
                first = (uint32_t)(k * 100 + i);
    }
    CHECK(first >= 999 && first <= 1001, "split-block kick starts at %u", (unsigned)first);

    /* 10 s of a dense, song-like stream: finite, below -3 dBFS */
    vx_init();
    vx_set_gain(1.0f);
    for (step = 0; step < 90; step++) { /* 90 sixteenths of 7779 samples = 15.9 s; render the first 10 s */
        int sp = step & 15;
        if (step * 7779 >= 441000)
            break;
        if (sp == 0 || sp == 10)
            vt_ev(V_KICK, 0, 110, 0, 0);
        if (sp == 4 || sp == 12) {
            vt_ev(V_SNARE, 0, 105, 900, 0);
            vt_ev(V_RIM, 0, 60, 0, 0);
        }
        if (sp % 2 == 0)
            vt_ev(V_HAT, 0, 55, (uint32_t)(vt_rnd() % 3000), 0);
        if (sp == 7)
            vt_ev(V_OHAT, 0, 50, 0, 0);
        if (sp % 4 == 3)
            vt_ev(V_SHAKER, 0, 40, 0, 0);
        if (sp == 0)
            for (k = 0; k < 4; k++)
                vt_ev(V_CHORD, 52 + k * 4, 100, (uint32_t)(k * 600), 7779u * 14u);
        if (sp % 4 == 0)
            vt_ev(V_BASS, 36 + (int)(vt_rnd() % 8), 105, 0, 7779u * 3u);
        if (vt_rnd() % 3 == 0)
            vt_ev(V_LEAD, 60 + (int)(vt_rnd() % 14), 100, (uint32_t)(vt_rnd() % 2000), 7779u * 2u);
        if (vt_rnd() % 5 == 0)
            vt_ev(V_CANDY, 72 + (int)(vt_rnd() % 12), 90, 0, 7779u * 4u);
        {
            int left = 7779;
            while (left > 0) {
                int n = left > 4096 ? 4096 : left;
                memset(vt_l, 0, (size_t)n * sizeof(float));
                memset(vt_r, 0, (size_t)n * sizeof(float));
                vx_render(vt_l, vt_r, (uint32_t)n);
                for (i = 0; i < n; i++) {
                    float a = vt_l[i] < 0 ? -vt_l[i] : vt_l[i], b = vt_r[i] < 0 ? -vt_r[i] : vt_r[i];
                    if (!(a == a) || !(b == b) || a > 1e9f || b > 1e9f)
                        nan++;
                    if (a > peak)
                        peak = a;
                    if (b > peak)
                        peak = b;
                }
                left -= n;
            }
        }
    }
    CHECK(nan == 0, "%d NaN/Inf samples in the dense stream", nan);
    CHECK(peak < 0.708f, "dense stream peak %.3f, wanted < 0.708 (-3 dBFS)", (double)peak);
    CHECK(peak > 0.1f, "dense stream is audible (peak %.3f)", (double)peak);

    /* 64 overlapping notes, then release_all: silent within 60 ms */
    vx_init();
    for (k = 0; k < 64; k++)
        vt_ev(k % 4 == 0 ? V_CHORD : k % 4 == 1 ? V_BASS : k % 4 == 2 ? V_LEAD : V_CANDY, 40 + k % 30, 100, 0,
              441000u);
    vt_ev(V_KICK, 0, 100, 0, 0);
    vt_ev(V_OHAT, 0, 100, 0, 0);
    vt_ev(V_LEAD, 60, 100, 500000, 1000); /* pending: release_all clears it */
    memset(vt_l, 0, sizeof vt_l);
    memset(vt_r, 0, sizeof vt_r);
    vx_render(vt_l, vt_r, 2000);
    CHECK(vx_active() > 4, "notes sounding before release (%d)", vx_active());
    vx_release_all();
    for (k = 0; k < 2646 / 441; k++) {
        memset(vt_l, 0, 441 * sizeof(float));
        memset(vt_r, 0, 441 * sizeof(float));
        vx_render(vt_l, vt_r, 441);
    }
    CHECK(vx_active() == 0, "%d voices still active 60 ms after release_all", vx_active());
    memset(vt_l, 0, sizeof vt_l);
    memset(vt_r, 0, sizeof vt_r);
    vx_render(vt_l, vt_r, 4096);
    {
        float m = 0.0f;
        for (i = 0; i < 4096; i++)
            if (vt_l[i] > m || -vt_l[i] > m)
                m = vt_l[i] < 0 ? -vt_l[i] : vt_l[i];
        CHECK(m < 1e-3f, "silence after release (%.5f); the queue was cleared", (double)m);
    }

    /* queue overflow by 200 events keeps the newest 64 */
    vx_init();
    for (k = 0; k < 200; k++)
        vt_ev(V_KICK, 0, 100, 1000u + (uint32_t)k * 200u, 0);
    first = 0xFFFFFFFFu;
    for (k = 0; k < 40000 / 1000 && first == 0xFFFFFFFFu; k++) {
        memset(vt_l, 0, 1000 * sizeof(float));
        memset(vt_r, 0, 1000 * sizeof(float));
        vx_render(vt_l, vt_r, 1000);
        for (i = 0; i < 1000; i++)
            if (vt_l[i] != 0.0f) {
                first = (uint32_t)(k * 1000 + i);
                break;
            }
    }
    CHECK(first >= 1000u + 136u * 200u - 1u && first <= 1000u + 136u * 200u + 1u,
          "first surviving kick at %u, wanted %u (events 0-135 dropped)", (unsigned)first, 1000u + 136u * 200u);

    /* gain ramps: 0 silences, and moves smoothly */
    vx_init();
    vx_set_gain(0.0f);
    for (k = 0; k < 4; k++) {
        memset(vt_l, 0, sizeof vt_l);
        memset(vt_r, 0, sizeof vt_r);
        vt_ev(V_BASS, 40, 100, 0, 100000);
        vx_render(vt_l, vt_r, 4096);
    }
    {
        float m = 0.0f;
        for (i = 2000; i < 4096; i++)
            if (vt_l[i] > m || -vt_l[i] > m)
                m = vt_l[i] < 0 ? -vt_l[i] : vt_l[i];
        CHECK(m < 1e-4f, "gain 0 is silent (%.5f)", (double)m);
    }
    printf("voices: dense peak %.3f\n", (double)peak);
}

/* largest sample-to-sample step of the left output over n samples starting at the current state */
static float vt_maxdelta(uint32_t n, uint32_t from, float *prev)
{
    float m = 0.0f;
    uint32_t i;
    memset(vt_l, 0, n * sizeof(float));
    memset(vt_r, 0, n * sizeof(float));
    vx_render(vt_l, vt_r, n);
    for (i = from; i < n; i++) {
        float d = vt_l[i] - (i ? vt_l[i - 1] : *prev);
        if (d < 0)
            d = -d;
        if (d > m)
            m = d;
    }
    *prev = vt_l[n - 1];
    return m;
}

static void test_voices_fix(void)
{
    int k, seed, i;
    float prev = 0.0f, d, dmax;
    float peak = 0.0f;

    /* a loud steady bass retriggered at a new level and note: no step */
    vx_init();
    vt_ev(V_BASS, 40, 127, 0, 400000);
    vt_maxdelta(4096, 4096, &prev);
    vt_ev(V_BASS, 47, 40, 0, 400000);
    d = vt_maxdelta(4096, 0, &prev);
    printf("voices: bass retrigger max delta %.4f\n", (double)d);
    CHECK(d < 0.01f, "bass retrigger click: max step %.4f", (double)d);

    /* a fifth chord note steals a loud held voice: the step stays near the 4-voice baseline (quiet thief, so any step is the victim cut) */
    {
        float base = 0.0f;
        int pass;
        dmax = 0.0f;
        for (pass = 0; pass < 2; pass++) {
            vx_init();
            for (k = 0; k < 4; k++)
                vt_ev(V_CHORD, 30 + k * 3, 127, 0, 400000);
            vt_maxdelta(4096, 4096, &prev);
            if (pass)
                vt_ev(V_CHORD, 31, 1, 0, 400000);
            for (i = 0; i < 4; i++) {
                d = vt_maxdelta(1024, 0, &prev);
                if (d > (pass ? dmax : base))
                    *(pass ? &dmax : &base) = d;
            }
        }
        printf("voices: chord steal max delta %.4f (baseline %.4f)\n", (double)dmax, (double)base);
        CHECK(dmax < base * 1.5f, "chord steal click: max step %.4f vs baseline %.4f", (double)dmax, (double)base);
    }

    /* stress: every voice at vel 110, drums stacked on the same samples, several seeds: under -3 dBFS */
    for (seed = 1; seed <= 6; seed++) {
        vx_init();
        vt_seed = (uint32_t)seed * 7919u;
        for (k = 0; k < 40; k++) {
            int j;
            for (j = 0; j < 4; j++)
                vt_ev(V_CHORD, 48 + (int)(vt_rnd() % 17), 110, 0, 20000);
            vt_ev(V_BASS, 33 + (int)(vt_rnd() % 12), 110, 0, 20000);
            for (j = 0; j < 2; j++) {
                vt_ev(V_LEAD, 55 + (int)(vt_rnd() % 25), 110, 0, 20000);
                vt_ev(V_CANDY, 60 + (int)(vt_rnd() % 25), 110, 0, 20000);
            }
            for (j = V_KICK; j < V_N; j++)
                vt_ev(j, 0, 110, 0, 0);
            memset(vt_l, 0, 2048 * sizeof(float));
            memset(vt_r, 0, 2048 * sizeof(float));
            vx_render(vt_l, vt_r, 2048);
            for (i = 0; i < 2048; i++) {
                float a = vt_l[i] < 0 ? -vt_l[i] : vt_l[i];
                if (a > peak)
                    peak = a;
            }
        }
    }
    printf("voices: stress peak %.3f\n", (double)peak);
    CHECK(peak < 0.708f, "stress peak %.3f, wanted < 0.708", (double)peak);

    /* gain is clamped to [0, 1] and lands exactly */
    vx_init();
    vx_set_gain(5.0f);
    memset(vt_l, 0, 2000 * sizeof(float));
    memset(vt_r, 0, 2000 * sizeof(float));
    vt_ev(V_BASS, 40, 127, 0, 100000);
    vx_render(vt_l, vt_r, 2000);
    for (i = 0; i < 2000; i++)
        if (vt_l[i] > 0.708f || vt_l[i] < -0.708f)
            CHECK(0, "gain above 1 leaked: %.3f", (double)vt_l[i]);
}

/* Render n samples (n <= 4096) into vt_l/vt_r from silence; peak |l|, and whether any sample was NaN or inf. */
static float vt_block(uint32_t n, int *bad)
{
    float m = 0.0f;
    uint32_t i;
    memset(vt_l, 0, n * sizeof(float));
    memset(vt_r, 0, n * sizeof(float));
    vx_render(vt_l, vt_r, n);
    for (i = 0; i < n; i++) {
        float a = vt_l[i] < 0 ? -vt_l[i] : vt_l[i];
        if (vt_l[i] != vt_l[i] || a > 1e30f || vt_r[i] != vt_r[i])
            *bad = 1;
        if (a > m)
            m = a;
    }
    return m;
}

/* A role an instrument plays in the library's sets (the first found; chords if none). */
static int vt_role_of(int ins)
{
    int s;
    for (s = 0; s < LIB_NSETS; s++) {
        const lib_set_t *t = &LIB_SETS[s];
        if (t->chords == ins) return V_CHORD;
        if (t->bass == ins) return V_BASS;
        if (t->lead == ins) return V_LEAD;
        if (t->candy == ins) return V_CANDY;
        if (t->pad == ins) return V_PAD;
    }
    return V_CHORD;
}

/* A mixed passage on every lane (noise drums included); a checksum of its samples. */
static uint32_t vt_passage(void)
{
    uint32_t h = 2166136261u;
    int k, i;
    vt_seed = 777u;
    for (k = 0; k < 20; k++) {
        vt_ev(V_CHORD, 50 + (int)(vt_rnd() % 20), 90, (uint32_t)(vt_rnd() % 1000), 6000);
        vt_ev(V_BASS, 36 + (int)(vt_rnd() % 12), 100, 0, 3000);
        vt_ev(V_LEAD, 62 + (int)(vt_rnd() % 12), 100, 0, 3000);
        vt_ev(V_CANDY, 74 + (int)(vt_rnd() % 12), 80, 0, 2000);
        vt_ev(V_KICK + (int)(vt_rnd() % (V_N - V_KICK)), 0, 100, 0, 0);
        memset(vt_l, 0, 4096 * sizeof(float));
        memset(vt_r, 0, 4096 * sizeof(float));
        vx_render(vt_l, vt_r, 4096);
        for (i = 0; i < 4096; i++) {
            union { float f; uint32_t u; } b;
            b.f = vt_l[i];
            h = (h ^ b.u) * 16777619u;
        }
    }
    return h;
}

/* High-frequency content: mean |first difference| over the block. */
static float vt_hf(uint32_t n)
{
    float s = 0.0f;
    uint32_t i;
    for (i = 1; i < n; i++)
        s += vt_l[i] > vt_l[i - 1] ? vt_l[i] - vt_l[i - 1] : vt_l[i - 1] - vt_l[i];
    return s / (float)n;
}

/* A note on a role, rendered for n windows of w samples: each window's peak |x| in env[] and its RMS in rms[], the
   sample of the loudest in *at, and the zero crossings of the first difference in the first zc_n samples (the
   signal's turning points: a crude spectral centroid, enough to order timbres; the plain signal's crossings miss a
   phase-locked FM's harmonics, which bend the wave between its crossings). */
static int vt_note_env(int role, int ins, int note, int vel, uint32_t len, int n, uint32_t w, float *env,
                       float *rms, uint32_t *at, uint32_t zc_n, int *bad)
{
    int k, zc = 0;
    uint32_t i, t = 0;
    float prev = 0.0f, dprev = 0.0f, top = 0.0f;
    vx_init();
    vx_assign(role, ins);
    vt_ev(role, note, vel, 0, len);
    *at = 0;
    for (k = 0; k < n; k++) {
        float m = 0.0f, e = 0.0f;
        memset(vt_l, 0, w * sizeof(float));
        memset(vt_r, 0, w * sizeof(float));
        vx_render(vt_l, vt_r, w);
        for (i = 0; i < w; i++, t++) {
            float x = vt_l[i], a = x < 0.0f ? -x : x;
            if (x != x || a > 1e30f)
                *bad = 1;
            if (a > m)
                m = a;
            e += x * x;
            if (a > top) {
                top = a;
                *at = t;
            }
            if (t < zc_n && t > 1 && ((x - prev >= 0.0f) != (dprev >= 0.0f)))
                zc++;
            dprev = x - prev;
            prev = x;
        }
        env[k] = m;
        rms[k] = fm_sqrtf(e / (float)w);
    }
    return zc;
}

/* The keys (M_EP) and pads (M_PAD): a struck key peaks within 15 ms and decays (under half the peak 1 s later), and
   the three keys order by brightness, Wurli > Rhodes > felt piano; a pad swells (at least 150 ms to its peak) and
   sustains while held (at least 70% of the peak at 2 s). */
static void test_vx_keys(void)
{
    static float env[600], rms[600];
    static const int KEYS[3] = {INS_WURLI, INS_RHODES, INS_FELT_PIANO};
    static const int PADS[2] = {INS_PAD, INS_VOX_PAD};
    static const int NOTES[3] = {55, 60, 67};
    int i, j, bad = 0, zc[3][3];

    vx_fx_off(VX_OFF_MUD); /* the voices' own timbre (zero crossings): the mix's mud high-pass left out */
    CHECK(vx_patch(INS_RHODES)->model == M_EP && vx_patch(INS_WURLI)->model == M_EP &&
              vx_patch(INS_FELT_PIANO)->model == M_EP && vx_patch(INS_PAD)->model == M_PAD &&
              vx_patch(INS_VOX_PAD)->model == M_PAD,
          "keys and pads on the wrong models");
    for (j = 0; j < 3; j++)
        for (i = 0; i < 3; i++) { /* 5 ms windows over 1.5 s; 1 s on, the loudest of two (a period at least) */
            int k;
            uint32_t at;
            float pk, later;
            zc[j][i] = vt_note_env(V_CHORD, KEYS[i], NOTES[j], 100, 3u * 44100u, 300, 220, env, rms, &at, 8820,
                                   &bad);
            pk = env[at / 220];
            k = (int)(at + 44100u) / 220;
            later = env[k] > env[k + 1] ? env[k] : env[k + 1];
            printf("keys: ins %2d note %d peak %.4f at %.1f ms, 1 s later %.4f (%.0f%%), zc %d\n", KEYS[i], NOTES[j],
                   (double)pk, (double)at / 44.1, (double)later, 100.0 * (double)(later / pk), zc[j][i]);
            CHECK(at <= 661u, "ins %d note %d: peak at %.1f ms, wanted within 15 ms", KEYS[i], NOTES[j],
                  (double)at / 44.1);
            CHECK(later < 0.5f * pk, "ins %d note %d: %.0f%% of the peak 1 s later, wanted under 50%%", KEYS[i],
                  NOTES[j], 100.0 * (double)(later / pk));
        }
    for (j = 0; j < 3; j++)
        CHECK(zc[j][0] > zc[j][1] && zc[j][1] > zc[j][2],
              "note %d: brightness (zero crossings in 200 ms) wurli %d, rhodes %d, felt %d: wanted that order",
              NOTES[j], zc[j][0], zc[j][1], zc[j][2]);

    for (j = 0; j < 3; j++)
        for (i = 0; i < 2; i++) { /* RMS in 50 ms windows (many periods) over 2.5 s, held 3 s */
            int k, pk = 0, role = vt_role_of(PADS[i]);
            uint32_t at;
            vt_note_env(role, PADS[i], NOTES[j], 100, 3u * 44100u, 50, 2205, env, rms, &at, 0, &bad);
            for (k = 1; k < 50; k++)
                if (rms[k] > rms[pk])
                    pk = k;
            printf("pads: ins %2d note %d peak %.4f at %d ms, at 2 s %.4f (%.0f%%)\n", PADS[i], NOTES[j],
                   (double)rms[pk], pk * 50 + 25, (double)rms[40], 100.0 * (double)(rms[40] / rms[pk]));
            CHECK(pk * 50 + 25 >= 150, "ins %d note %d: peak at %d ms, wanted 150 ms or later", PADS[i], NOTES[j],
                  pk * 50 + 25);
            CHECK(rms[40] >= 0.7f * rms[pk], "ins %d note %d: %.0f%% of the peak at 2 s, wanted 70%% or more",
                  PADS[i], NOTES[j], 100.0 * (double)(rms[40] / rms[pk]));
        }
    CHECK(!bad, "a key or pad rendered NaN or inf");
    vx_fx_off(0u);
}

/* A note on a role, from silence, held len samples: n samples of the left output into out (n a multiple of 4096 at
   most VT_LONG). */
#define VT_LONG (36u * 4096u)
static float vt_long[VT_LONG];
static void vt_note(int role, int ins, int note, int vel, uint32_t len, uint32_t n, int *bad)
{
    uint32_t k;
    vx_init();
    vx_assign(role, ins);
    vt_ev(role, note, vel, 0, len);
    for (k = 0; k < n; k += 4096u) {
        vt_block(4096, bad);
        memcpy(vt_long + k, vt_l, 4096 * sizeof(float));
    }
}

/* The period of vt_long[from .. from+w) near p (within 10%): the normalized autocorrelation's peak, parabolically
   interpolated. */
static float vt_period(uint32_t from, uint32_t w, float p)
{
    int lo = (int)(p * 0.9f), hi = (int)(p * 1.1f) + 1, lag, best = lo;
    float r[2048], a, b, c, d;
    for (lag = lo; lag <= hi; lag++) {
        float s = 0.0f, e0 = 0.0f, e1 = 0.0f;
        uint32_t i;
        for (i = from; i < from + w; i++) {
            s += vt_long[i] * vt_long[i + (uint32_t)lag];
            e0 += vt_long[i] * vt_long[i];
            e1 += vt_long[i + (uint32_t)lag] * vt_long[i + (uint32_t)lag];
        }
        s *= e0 > 0.0f && e1 > 0.0f ? fm_rsqrtf(e0 * e1) : 0.0f; /* normalized: a decay doesn't pull the peak */
        r[lag - lo] = s;
        if (s > r[best - lo])
            best = lag;
    }
    if (best == lo || best == hi)
        return (float)best;
    a = r[best - lo - 1];
    b = r[best - lo];
    c = r[best - lo + 1];
    d = a - 2.0f * b + c;
    return (float)best + (d != 0.0f ? 0.5f * (a - c) / d : 0.0f);
}

/* The amplitude of frequency hz (cycles per sample) in vt_long[from .. from+w), Hann-windowed (Goertzel). */
static float vt_tone(uint32_t from, uint32_t w, float f)
{
    float k = 2.0f * fm_cosf(FM_TWO_PI * f), s1 = 0.0f, s2 = 0.0f;
    uint32_t i;
    for (i = 0; i < w; i++) {
        float h = 0.5f - 0.5f * fm_cosf(FM_TWO_PI * (float)i / (float)w), s = vt_long[from + i] * h + k * s1 - s2;
        s2 = s1;
        s1 = s;
    }
    return fm_sqrtf(fm_maxf(s1 * s1 + s2 * s2 - k * s1 * s2, 0.0f));
}

/* Brightness of vt_long[from .. from+w): RMS of the first difference over RMS of the signal (a centroid proxy). */
static float vt_bright(uint32_t from, uint32_t w)
{
    float e = 0.0f, d = 0.0f;
    uint32_t i;
    for (i = from + 1; i < from + w; i++) {
        float x = vt_long[i], dx = x - vt_long[i - 1];
        e += x * x;
        d += dx * dx;
    }
    return e > 0.0f ? fm_sqrtf(d / e) : 0.0f;
}

static float vt_peak(uint32_t from, uint32_t to)
{
    float m = 0.0f;
    uint32_t i;
    for (i = from; i < to; i++)
        m = fm_maxf(m, fm_fabsf(vt_long[i]));
    return m;
}

/* The plucked strings (M_PLUCK: guitar lick, upright; the nylon is sampled now, M_SAMP: test_samp) and the basses
   (M_BASS: sub, electric bass). A pluck is in tune by its period (within 10 cents) at 40, 55 and 70; the upright falls
   to -40 dB within 1.5 s, held; the Karplus-Strong line is sized for note 28. The sub's 2x and 3x partials are each 12 dB under the
   fundamental; the electric bass's attack is brighter and settles within 100 ms. */
static void test_vx_plucks_basses(void)
{
    static const int PLUCKS[2] = {INS_GUITAR_LICK, INS_UPRIGHT};
    static const int NOTES[3] = {40, 55, 70};
    int i, j, bad = 0;

    CHECK(vx_patch(INS_NYLON)->model == M_SAMP && vx_patch(INS_GUITAR_LICK)->model == M_PLUCK &&
              vx_patch(INS_UPRIGHT)->model == M_PLUCK && vx_patch(INS_SUB)->model == M_BASS &&
              vx_patch(INS_ELEC_BASS)->model == M_BASS,
          "plucks and basses on the wrong models");
    CHECK(VX_KS_N <= 44100 / 41 && (float)VX_KS_N >= 44100.0f / 41.2034f + 1.0f,
          "the Karplus-Strong line is %d samples: wanted note 28's period (+1 for the interpolation) to 44100/41",
          VX_KS_N);
    for (i = 0; i < 2; i++)
        for (j = 0; j < 3; j++) {
            int role = vt_role_of(PLUCKS[i]);
            float p = 44100.0f / (440.0f * fm_exp2f(((float)NOTES[j] - 69.0f) / 12.0f)), got, cents;
            vt_note(role, PLUCKS[i], NOTES[j], 100, 4u * 44100u, 16u * 4096u, &bad);
            got = vt_period(4410, 8192, p);
            cents = 1200.0f * fm_log2f(p / got);
            printf("plucks: ins %2d note %d period %.2f (wanted %.2f) %+.1f cents\n", PLUCKS[i], NOTES[j], (double)got,
                   (double)p, (double)cents);
            CHECK(fm_fabsf(cents) <= 10.0f, "ins %d note %d: %+.1f cents out", PLUCKS[i], NOTES[j], (double)cents);
        }
    for (i = 1; i < 2; i++)
        for (j = 0; j < 3; j++) { /* held 5 s: the string's own decay */
            float t = 1.5f, pk, at;
            uint32_t s = (uint32_t)(t * 44100.0f);
            vt_note(vt_role_of(PLUCKS[i]), PLUCKS[i], NOTES[j], 100, 5u * 44100u, VT_LONG, &bad);
            pk = vt_peak(0, 4410);
            at = vt_peak(s, s + 2205);
            printf("plucks: ins %2d note %d peak %.4f, at %.1f s %.1f dB\n", PLUCKS[i], NOTES[j], (double)pk,
                   (double)t, 20.0 * (double)fm_log2f(at / pk + 1e-9f) * 0.30103);
            CHECK(at < 0.01f * pk, "ins %d note %d: %.1f dB at %.1f s, wanted -40 dB or lower", PLUCKS[i], NOTES[j],
                  20.0 * (double)fm_log2f(at / pk + 1e-9f) * 0.30103, (double)t);
        }

    for (j = 0; j < 3; j++) { /* the sub at 33, 40, 47: Goertzel at 1x, 2x, 3x over 8192 samples from 0.2 s */
        int note = 33 + 7 * j;
        float f = 440.0f * fm_exp2f(((float)note - 69.0f) / 12.0f) / 44100.0f, a1, a2, a3;
        vt_note(V_BASS, INS_SUB, note, 100, 2u * 44100u, 16u * 4096u, &bad);
        a1 = vt_tone(8820, 8192, f);
        a2 = vt_tone(8820, 8192, 2.0f * f);
        a3 = vt_tone(8820, 8192, 3.0f * f);
        printf("basses: sub note %d 2x %.1f dB, 3x %.1f dB\n", note, 20.0 * (double)fm_log2f(a2 / a1 + 1e-9f) * 0.30103,
               20.0 * (double)fm_log2f(a3 / a1 + 1e-9f) * 0.30103);
        CHECK(a2 < 0.251f * a1 && a3 < 0.251f * a1, "sub note %d: a 2x or 3x partial within 12 dB of the fundamental",
              note);
    }
    for (j = 0; j < 3; j++) { /* the electric bass: brightness over two periods at the attack, 100 ms and 300 ms */
        int note = 33 + 7 * j;
        uint32_t w = (uint32_t)(2.0f * 44100.0f / (440.0f * fm_exp2f(((float)note - 69.0f) / 12.0f)) + 0.5f);
        float b0, b1, b3;
        vt_note(V_BASS, INS_ELEC_BASS, note, 100, 2u * 44100u, 4u * 4096u, &bad);
        b0 = vt_bright(0, w);
        b1 = vt_bright(4410, w);
        b3 = vt_bright(13230, w);
        printf("basses: electric note %d brightness attack %.4f, 100 ms %.4f, 300 ms %.4f\n", note, (double)b0,
               (double)b1, (double)b3);
        CHECK(b0 > 1.5f * b3, "electric bass note %d: attack %.4f not brighter than %.4f settled", note, (double)b0,
              (double)b3);
        CHECK(fm_fabsf(b1 - b3) < 0.1f * b3, "electric bass note %d: not settled at 100 ms (%.4f vs %.4f)", note,
              (double)b1, (double)b3);
    }
    { /* an upright re-pluck (a loud string, then a quiet note): no step beyond the string's own or a fresh pluck's */
        float prev = 0.0f, fresh, own = 0.0f, re = 0.0f;
        int pass, k;
        vx_init();
        vx_assign(V_BASS, INS_UPRIGHT);
        vt_ev(V_BASS, 47, 20, 0, 400000);
        fresh = vt_maxdelta(4096, 0, &prev);
        for (pass = 0; pass < 2; pass++) {
            vx_init();
            vx_assign(V_BASS, INS_UPRIGHT);
            vt_ev(V_BASS, 40, 127, 0, 400000);
            prev = 0.0f;
            vt_maxdelta(1024, 1024, &prev); /* 23 ms in: the string still loud */
            if (pass)
                vt_ev(V_BASS, 47, 20, 0, 400000);
            for (k = 0; k < 4; k++)
                *(pass ? &re : &own) = fm_maxf(*(pass ? &re : &own), vt_maxdelta(1024, 0, &prev));
        }
        printf("plucks: upright re-pluck max step %.4f (string %.4f, fresh pluck %.4f)\n", (double)re, (double)own,
               (double)fresh);
        CHECK(re < 1.5f * fm_maxf(own, fresh), "upright re-pluck click: max step %.4f vs %.4f / %.4f", (double)re,
              (double)own, (double)fresh);
    }
    CHECK(!bad, "a pluck or bass rendered NaN or inf");
}

/* RMS of vt_long[from .. from+w) */
static float vt_rms(uint32_t from, uint32_t w)
{
    float e = 0.0f;
    uint32_t i;
    for (i = from; i < from + w; i++)
        e += vt_long[i] * vt_long[i];
    return fm_sqrtf(e / (float)w);
}

/* The bells (M_BELL: vibes, kalimba, music box, bells) and the winds (M_WIND: flute, sax, muted trumpet). A bell
   decays freely, held (no sustain: under -20 dB of its peak 3 s in); the vibes' level has a tremolo (4-6 Hz, at
   least 20% deep, on the detrended log RMS); the bells' modulator ratio is non-integer, and they are the least
   periodic of the four. A wind sustains while held (half its peak RMS or more at 2 s); its vibrato waits (no pitch
   swing over 3 cents in any window ending by 250 ms; then at least 4 cents by 1.5 s); its breath is there (the
   residual after the periodic part is combed out, before the vibrato, is over -30 dB); and the muted trumpet is
   brighter than the sax, the sax brighter than the flute. */
static void test_vx_leads(void)
{
    static const int BELLS[4] = {INS_VIBES, INS_KALIMBA, INS_MUSIC_BOX, INS_BELLS};
    static const int WINDS[3] = {INS_FLUTE, INS_SAX, INS_MUTED_TPT};
    static const int NOTES[3] = {60, 67, 74};
    static float lr[400];
    int i, j, bad = 0;
    float per[4];

    for (i = 0; i < 4; i++)
        CHECK(vx_patch(BELLS[i])->model == M_BELL && vx_patch(BELLS[i])->sus == 0.0f, "ins %d: not a free M_BELL",
              BELLS[i]);
    for (i = 0; i < 3; i++)
        CHECK(vx_patch(WINDS[i])->model == M_WIND, "ins %d: not an M_WIND", WINDS[i]);
    { /* the bells' ratio: the furthest from an integer of the four, and at least 0.1 from one */
        float fr[4];
        for (i = 0; i < 4; i++) {
            float r = vx_patch(BELLS[i])->p[0];
            fr[i] = fm_fabsf(r - (float)(int)(r + 0.5f));
        }
        CHECK(fr[3] >= 0.1f && fr[3] > fr[0] && fr[3] > fr[1] && fr[3] > fr[2],
              "bells' ratio %.3f off an integer by %.3f: wanted the most of the four, at least 0.1",
              (double)vx_patch(INS_BELLS)->p[0], (double)fr[3]);
    }
    for (i = 0; i < 4; i++) { /* held 3.2 s at note 72: the free decay, and the periodicity at the period */
        float pk, at, s = 0.0f, e0 = 0.0f, e1 = 0.0f, p = 44100.0f / 523.2511f;
        uint32_t n, lag = (uint32_t)(p + 0.5f);
        vt_note(vt_role_of(BELLS[i]), BELLS[i], 72, 100, 4u * 44100u, VT_LONG, &bad);
        pk = vt_peak(0, 4410);
        at = vt_peak(132300, 136710);
        /* 523.25 Hz: 84.28 samples; the normalized correlation at 3 periods (252.8, so 253) over 4096 from 50 ms */
        lag = (uint32_t)(3.0f * p + 0.5f);
        for (n = 2205; n < 2205 + 4096; n++) {
            s += vt_long[n] * vt_long[n + lag];
            e0 += vt_long[n] * vt_long[n];
            e1 += vt_long[n + lag] * vt_long[n + lag];
        }
        per[i] = s * fm_rsqrtf(e0 * e1 + 1e-30f);
        printf("bells: ins %2d peak %.4f, at 3 s %.1f dB, periodicity %.4f\n", BELLS[i], (double)pk,
               20.0 * (double)fm_log2f(at / pk + 1e-9f) * 0.30103, (double)per[i]);
        CHECK(at < 0.1f * pk, "ins %d: %.1f dB at 3 s held, wanted a free decay (-20 dB or lower)", BELLS[i],
              20.0 * (double)fm_log2f(at / pk + 1e-9f) * 0.30103);
    }
    CHECK(per[3] < per[0] && per[3] < per[1] && per[3] < per[2] && per[3] < 0.95f,
          "bells' periodicity %.4f: wanted the least periodic of the four (vibes %.4f, kalimba %.4f, music box %.4f)",
          (double)per[3], (double)per[0], (double)per[1], (double)per[2]);
    { /* the vibes' tremolo: log RMS in 10 ms windows from 0.1 to 1.6 s, the decay's line taken out */
        int k, nw = 150, zc = 0;
        float sx = 0.0f, sy = 0.0f, sxx = 0.0f, sxy = 0.0f, b, a, lo = 1e9f, hi = -1e9f, rate, depth;
        vt_note(V_LEAD, INS_VIBES, 72, 100, 4u * 44100u, 20u * 4096u, &bad);
        for (k = 0; k < nw; k++) {
            float x = (float)k, y = fm_log2f(vt_rms(4410u + (uint32_t)k * 441u, 441u) + 1e-12f);
            lr[k] = y;
            sx += x;
            sy += y;
            sxx += x * x;
            sxy += x * y;
        }
        b = ((float)nw * sxy - sx * sy) / ((float)nw * sxx - sx * sx);
        a = (sy - b * sx) / (float)nw;
        for (k = 0; k < nw; k++) {
            lr[k] -= a + b * (float)k;
            lo = fm_minf(lo, lr[k]);
            hi = fm_maxf(hi, lr[k]);
            if (k && (lr[k] >= 0.0f) != (lr[k - 1] >= 0.0f))
                zc++;
        }
        rate = (float)zc * 0.5f / 1.5f;
        depth = 1.0f - fm_exp2f(lo - hi);
        printf("bells: vibes tremolo %.2f Hz, depth %.0f%%\n", (double)rate, 100.0 * (double)depth);
        CHECK(rate >= 4.0f && rate <= 6.0f && depth >= 0.2f, "vibes tremolo %.2f Hz, %.0f%% deep: wanted 4-6 Hz, 20%%+",
              (double)rate, 100.0 * (double)depth);
    }

    for (i = 0; i < 3; i++) { /* held 3 s at 67: sustain at 2 s, the vibrato's onset, the breath */
        float p = 44100.0f / 391.9954f, pk = 0.0f, early = 0.0f, late = 0.0f, d = 0.0f, x = 0.0f, fr;
        uint32_t t, li;
        int k;
        vt_note(V_LEAD, WINDS[i], 67, 100, 3u * 44100u, 24u * 4096u, &bad);
        for (k = 0; k < 50; k++)
            pk = fm_maxf(pk, vt_rms((uint32_t)k * 2205u, 2205u));
        for (t = 4410; t + 2048 <= 66150; t += 441) { /* 46 ms windows from 100 ms to 1.5 s */
            float c = fm_fabsf(1200.0f * fm_log2f(p / vt_period(t, 2048, p)));
            if (t + 2048 <= 11025)
                early = fm_maxf(early, c);
            else
                late = fm_maxf(late, c);
        }
        li = (uint32_t)p;
        fr = p - (float)li;
        for (t = 5292; t < 5292 + 5292; t++) { /* 120 to 240 ms: x[n] - x[n + P] */
            float y = vt_long[t + li] + fr * (vt_long[t + li + 1] - vt_long[t + li]), r = vt_long[t] - y;
            d += r * r;
            x += vt_long[t] * vt_long[t];
        }
        d = 10.0f * fm_log2f(d / (2.0f * x) + 1e-12f) * 0.30103f;
        printf("winds: ins %2d peak rms %.4f, at 2 s %.0f%%, vibrato by 250 ms %.1f cents, later %.1f, breath %.1f dB\n",
               WINDS[i], (double)pk, 100.0 * (double)(vt_rms(88200, 2205) / pk), (double)early, (double)late,
               (double)d);
        CHECK(vt_rms(88200, 2205) >= 0.5f * pk, "ins %d: %.0f%% of its peak at 2 s held, wanted 50%%+", WINDS[i],
              100.0 * (double)(vt_rms(88200, 2205) / pk));
        CHECK(early < 3.0f && late >= 4.0f, "ins %d: vibrato %.1f cents by 250 ms, %.1f later: wanted it delayed",
              WINDS[i], (double)early, (double)late);
        CHECK(d > -30.0f, "ins %d: breath residual %.1f dB, wanted over -30 dB", WINDS[i], (double)d);
    }
    for (j = 0; j < 3; j++) { /* brightness from 300 ms, 8192 samples */
        float br[3];
        for (i = 0; i < 3; i++) {
            vt_note(V_LEAD, WINDS[i], NOTES[j], 100, 2u * 44100u, 8u * 4096u, &bad);
            br[i] = vt_bright(13230, 8192);
        }
        printf("winds: note %d brightness flute %.4f, sax %.4f, muted trumpet %.4f\n", NOTES[j], (double)br[0],
               (double)br[1], (double)br[2]);
        CHECK(br[2] > br[1] && br[1] > br[0], "note %d: brightness flute %.4f, sax %.4f, muted %.4f: wanted rising",
              NOTES[j], (double)br[0], (double)br[1], (double)br[2]);
    }
    CHECK(!bad, "a bell or wind rendered NaN or inf");
}

/* The chip voice (M_CHIP: the triangle bass). Its triangle is stepped to 16 levels (its plateaus in one cycle at note
   36, clustered); its output is bit crushed (each sample its pre-crush sample rounded to the step, and over a held
   second at most 2^bits + 1 distinct levels where the pre-crush signal takes more). The delay is off: its echoes only
   blur the measures. */
static int ch_cmp(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;
    return x < y ? -1 : x > y;
}
static void test_chip(void)
{
    int bad = 0;
    vx_fx_off(VX_OFF_DELAY);
    CHECK(vx_patch(INS_CHIP_BASS)->model == M_CHIP, "the triangle bass: not M_CHIP");
    CHECK(!lib_ins_sustained(INS_CHIP_BASS), "the triangle bass: wanted percussive");
    CHECK(INS_FM_BASS == INS_VOX_PAD + 6, "instruments: %d after the vox pad, wanted Sam's six synths",
          INS_FM_BASS - INS_VOX_PAD);
    CHECK(INS_STRINGS == INS_FM_BASS + 1 && INS_WARM_PAD == INS_FM_BASS + 2 && INS_KNIGHT == INS_FM_BASS + 3 &&
              INS_EMILY == INS_FM_BASS + 4 && INS_NONE == INS_FM_BASS + 5,
          "instruments: %d after the six synths, wanted the pad layer's two (strings, warm pad), then the Knight and "
          "the Emilyguitar",
          INS_NONE - INS_FM_BASS - 1);
    { /* the triangle at note 36 (674 samples a cycle): its plateaus' levels in one cycle from 0.5 s, clustered */
        static float lv[1024];
        float lo = 1e9f, hi = -1e9f, eps;
        uint32_t from = 22050u, per = 674u, t;
        int nl = 0, cl = 0, a, b;
        float dr = 0.0f, dout = 0.0f, olo = 1e9f, ohi = -1e9f;
        vt_note(V_BASS, INS_CHIP_BASS, 36, 100, 44100u, 8u * 4096u, &bad);
        for (t = from; t < from + per; t++) {
            olo = fm_minf(olo, vt_long[t]);
            ohi = fm_maxf(ohi, vt_long[t]);
            dout += (vt_long[t + 1] - vt_long[t]) * (vt_long[t + 1] - vt_long[t]);
        }
        vx_init();
        vx_assign(V_BASS, INS_CHIP_BASS);
        vt_ev(V_BASS, 36, 100, 0, 44100u);
        for (t = 0; t < from + per + 1; t++) {   /* the voice's triangle before its crush and low-pass */
            float l1 = 0.0f, r1 = 0.0f, raw, st;
            vx_render(&l1, &r1, 1u);
            vx_chip_out(V_BASS, &raw, &st);
            vt_long[t] = raw;
        }
        for (t = from; t < from + per; t++) {
            lo = fm_minf(lo, vt_long[t]);
            hi = fm_maxf(hi, vt_long[t]);
        }
        eps = 0.01f * (hi - lo);
        for (t = from + 1; t < from + per; t++)
            if (fm_fabsf(vt_long[t] - vt_long[t - 1]) < eps && fm_fabsf(vt_long[t + 1] - vt_long[t]) < eps)
                lv[nl++] = vt_long[t];
        for (a = 1; a < nl; a++) /* sort */
            for (b = a; b > 0 && lv[b] < lv[b - 1]; b--) {
                float x = lv[b];
                lv[b] = lv[b - 1];
                lv[b - 1] = x;
            }
        for (a = 0; a < nl; a++)
            cl += !a || lv[a] - lv[a - 1] > (hi - lo) / 30.0f; /* half a step of 16 levels */
        for (t = from; t < from + per; t++) dr += (vt_long[t + 1] - vt_long[t]) * (vt_long[t + 1] - vt_long[t]);
        dout /= (ohi - olo) * (ohi - olo);
        dr /= (hi - lo) * (hi - lo);
        /* mellow (Sam: "a little more mellow... filter it a bit"): the low-pass smooths the steps: the output's
           sample-to-sample motion (its energy: steps are sharp) under 0.15 of the raw triangle's, each over its range */
        printf("chip: the triangle bass's output moves %.2f of its raw triangle's motion a sample\n",
               (double)(dout / dr));
        CHECK(dout < 0.15f * dr, "triangle bass: not mellowed (output motion %.2f of the raw's)", (double)(dout / dr));
        printf("chip: triangle at note 36: %d levels in a cycle (range %.4f)\n", cl, (double)(hi - lo));
        CHECK(cl >= 15 && cl <= 17, "triangle: %d levels in a cycle, wanted 16", cl);
    }
    { /* the bit crush: the output quantized to p[0] bits over its own level (2^(bits-1) steps from 0 to it). A note
         held 1 s on the bass, its voice read a sample at a time (vx_chip_out: the sample the voice hands the mix,
         and the same before the crush) */
        static float vo[44100], vr[44100];
        float l1, r1, step = 0.0f;
        uint32_t t, k;
        int bits = (int)vx_patch(INS_CHIP_BASS)->p[0], off = 0, no = 0, nr = 0, lim = (1 << bits) + 1;
        CHECK(bits >= 4 && bits <= 8, "triangle bass: crush %d bits, wanted 4-8", bits);
        vx_init();
        vx_assign(V_BASS, INS_CHIP_BASS);
        vt_ev(V_BASS, 36, 100, 0, 44100u);
        for (t = 0; t < 44100u; t++) {
            float raw, st;
            l1 = r1 = 0.0f;
            vx_render(&l1, &r1, 1u);
            vo[t] = vx_chip_out(V_BASS, &raw, &st);
            vr[t] = raw;
            if (st > 0.0f) {
                step = st;
                off += vo[t] != fm_floorf(raw * (1.0f / st) + 0.5f) * st;
            }
        }
        qsort(vo, 44100u, sizeof vo[0], ch_cmp);
        qsort(vr, 44100u, sizeof vr[0], ch_cmp);
        for (k = 0; k < 44100u; k++) {
            no += !k || vo[k] != vo[k - 1];
            nr += !k || vr[k] != vr[k - 1];
        }
        printf("chip: triangle bass crush %d bits (step %.5f): %d output levels in 1 s (%d before the crush), %d "
               "samples off the steps\n", bits, (double)step, no, nr, off);
        CHECK(step > 0.0f && off == 0, "chip crush: %d samples off its %d-bit steps", off, bits);
        CHECK(nr > lim && no >= 4 && no <= lim, "chip crush: %d output levels (%d before), wanted 4 to %d", no, nr, lim);
    }
    CHECK(!bad, "the chip voice rendered NaN or inf");
    vx_fx_off(0u);
}

/* The pad layer's voice (M_ENS on V_PAD): each patch held 2 s at note 60 renders no NaN or inf, swells slowly (its
   level by 100 ms under 40% of its level at 1 s), is silent (under -90 dB) 3 s after its release with no state left
   denormal-bound (vx_tiny), and the same events render the same samples. */
static void test_pad_voice(void)
{
    static const int ins[2] = {INS_STRINGS, INS_WARM_PAD};
    static const char *const name[2] = {"strings", "warm pad"};
    int i, pass, k, full, bad = 0;
    for (full = 0; full < 2; full++) /* at its quarter rate, and at the full rate (VX_OFF_HALF) */
    for (i = 0; i < 2; i++) {
        uint32_t h[2];
        float a100 = 0.0f, a1s = 0.0f, tail = 0.0f;
        CHECK(vx_patch(ins[i]) && vx_patch(ins[i])->model == M_ENS, "pad voice: %s is not M_ENS", name[i]);
        for (pass = 0; pass < 2; pass++) {
            h[pass] = 2166136261u;
            vx_fx_off(full ? VX_OFF_HALF : 0u);
            vx_init();
            vx_assign(V_PAD, ins[i]);
            vt_ev(V_PAD, 60, 100, 0, 88200u);
            for (k = 0; k < 22 + 33 + 1; k++) { /* 2 s held (22 blocks), 3 s of release (33), then the tail */
                float m = vt_block(4096, &bad);
                uint32_t j;
                for (j = 0; j < 4096; j++) {
                    union { float f; uint32_t u; } b;
                    b.f = vt_l[j];
                    h[pass] = (h[pass] ^ b.u) * 16777619u;
                }
                if (k == 0) a100 = m;  /* 0-93 ms */
                if (k == 10) a1s = m;  /* 0.93-1.02 s */
                if (k == 55) tail = m;
            }
            CHECK(vx_tiny() == 0, "pad voice: %s left %d denormal-bound states", name[i], vx_tiny());
        }
        printf("pad voice: %s%s level %.4f by 100 ms, %.4f at 1 s, tail %.2e\n", name[i], full ? " (full rate)" : "",
               (double)a100, (double)a1s, (double)tail);
        CHECK(a1s > 0.01f && a100 < 0.4f * a1s, "pad voice: %s swell %.4f by 100 ms vs %.4f at 1 s", name[i],
              (double)a100, (double)a1s);
        CHECK(tail < 3.16e-5f && vx_active() == 0, "pad voice: %s still %.2e 3 s after its release", name[i],
              (double)tail);
        CHECK(h[0] == h[1], "pad voice: %s rendered different samples from the same events", name[i]);
    }
    vx_fx_off(0u);
    CHECK(!bad, "pad voice: NaN or inf");
}

/* Each role on its own bus (pads, Review Focus 1): with one bus soloed (vx_solo), a note sounds only on its role's
   bus: a pad note on V_PAD's, a kick on the drums', a chord on V_CHORD's (and the bass, the lead, the ear candy). */
static void test_role_buses(void)
{
    static const int role[6] = {V_CHORD, V_BASS, V_LEAD, V_CANDY, V_PAD, V_KICK};
    int e, s, k, bad = 0;
    for (e = 0; e < 6; e++)
        for (s = 0; s < 6; s++) {
            float m = 0.0f;
            vx_init();
            vx_solo(role[s]);
            vt_ev(role[e], role[e] == V_KICK ? 0 : role[e] == V_BASS ? 43 : 64, 110, 0, 22050u);
            for (k = 0; k < 5; k++) {
                float b = vt_block(4096, &bad);
                if (b > m)
                    m = b;
            }
            CHECK(s == e ? m > 1e-3f : m < 1e-7f, "role buses: role %d's note on bus %d soloed: peak %.2e", role[e],
                  role[s], (double)m);
        }
    vx_solo(-1);
    CHECK(!bad, "role buses: NaN or inf");
}

/* A pad lane's note (song_ev_t.pad = lane) on V_PAD. */
static void vt_lane(int lane, int note, uint32_t delay, uint32_t len)
{
    song_ev_t e;
    memset(&e, 0, sizeof e);
    e.voice = V_PAD;
    e.note = (uint8_t)note;
    e.vel = 100;
    e.delay = delay;
    e.len = len;
    e.pad = (uint8_t)lane;
    vx_event(&e);
}

/* How far (dB) the energy off the harmonics of note's pitch lies under the energy on them, over 0-11 kHz, in
   vt_long[from .. from + 8192) (Hann; within 4 bins of a harmonic counts as on it): the folds and images. */
static float vt_alias_db(uint32_t from, int note)
{
    static float win[8192];
    float f0 = 440.0f * fm_exp2f(((float)note - 69.0f) * (1.0f / 12.0f)), on = 1e-30f, off = 1e-30f;
    int b, i;
    for (i = 0; i < 8192; i++)
        win[i] = vt_long[from + (uint32_t)i] * (0.5f - 0.5f * fm_cosf(FM_TWO_PI * (float)i / 8192.0f));
    for (b = 2; b < 2043; b++) { /* 11 kHz */
        float k = 2.0f * fm_cosf(FM_TWO_PI * (float)b / 8192.0f), s1 = 0.0f, s2 = 0.0f, pw, hn, d;
        for (i = 0; i < 8192; i++) {
            float x = win[i] + k * s1 - s2;
            s2 = s1;
            s1 = x;
        }
        pw = s1 * s1 + s2 * s2 - k * s1 * s2;
        hn = (float)b * (44100.0f / 8192.0f) / f0;
        d = fm_fabsf(hn - fm_floorf(hn + 0.5f)) * f0 * (8192.0f / 44100.0f);
        *(d < 4.0f ? &on : &off) += pw;
    }
    return 10.0f * fm_log2f(on / off) * 0.30103f;
}

/* The pad layer's rates (pads Task 1, fix round): a note renders at a quarter of the rate while its low-pass (with
   its key tracking) is 4 kHz or under, else at half. A high note on the strings (MIDI 98: 4149 Hz) is half-rate, and
   its folds lie at least as low as the half-rate path's (M_PAD, one saw into a 2-pole at the same cutoff, on the
   chords); a low note (G3) stays at a quarter. The partner saw is off, so only the folds are off the harmonics. */
static void test_pad_rates(void)
{
    static vx_patch_t ens, pad;
    float a_ens, a_pad, a_full;
    int step, held, bad = 0, k;
    ens = *vx_patch(INS_STRINGS);
    ens.p[1] = 0.0f;
    pad = *vx_patch(INS_PAD);
    pad.p[0] = 0.0f; /* one saw: no detune, no second saw (its ratio 1: the note stays at half the rate) */
    pad.p[1] = 1.0f;
    pad.p[2] = 0.0f;
    pad.p[3] = ens.p[2];
    pad.p[4] = ens.p[3];
    pad.p[5] = 1.4142f;
    vx_init();
    vx_assign_patch(V_PAD, &ens);
    vt_lane(1, 55, 0, 44100u);
    vt_block(64, &bad);
    vx_pad_lane(1, &step, &held);
    CHECK(step == 4, "pad rates: G3 renders at step %d, wanted a quarter (4)", step);
    for (k = 0; k < 3; k++) {
        vx_fx_off(k == 2 ? VX_OFF_HALF : 0u);
        vx_init();
        if (k == 1)
            vx_assign_patch(V_CHORD, &pad);
        else
            vx_assign_patch(V_PAD, &ens);
        if (k == 1)
            vt_ev(V_CHORD, 98, 100, 0, 88200u);
        else
            vt_lane(1, 98, 0, 88200u);
        vt_block(64, &bad);
        if (k == 0) {
            vx_pad_lane(1, &step, &held);
            CHECK(step == 2, "pad rates: MIDI 98 renders at step %d, wanted half (2)", step);
        }
        {
            uint32_t at;
            for (at = 0; at < 8u * 4096u; at += 4096u) {
                vt_block(4096, &bad);
                memcpy(vt_long + at, vt_l, 4096 * sizeof(float));
            }
        }
        *(k == 0 ? &a_ens : k == 1 ? &a_pad : &a_full) = vt_alias_db(16384u, 98);
    }
    vx_fx_off(0u);
    printf("pad rates: MIDI 98 folds %.1f dB under (half rate), %.1f (M_PAD's half rate), %.1f (full rate)\n",
           (double)a_ens, (double)a_pad, (double)a_full);
    CHECK(a_ens >= a_pad - 0.5f, "pad rates: MIDI 98's folds %.1f dB under, the half-rate path's %.1f", (double)a_ens,
          (double)a_pad);
    CHECK(!bad, "pad rates: NaN or inf");
}

/* The pad bus's section filter darkens the pad (pads Task 3's fix: the filter, the mud high-pass and the sends run at
   the quarter-rate sum's rate, before its interpolation up): a held chord and solo note on the strings, soloed, then
   the bus's low-pass to 0.85 (a deep break) and its high-pass to 0.8. Each move's drop in the high band (the first
   difference's energy) and in all of it matches the full-rate reference's (VX_OFF_HALF: every pad voice at the full
   rate, through the bus's own filters) within 1.5 dB, and the low-pass takes 6 dB off the high band at least. */
static void pbf_measure(int bad[1], double *all, double *hi)
{
    int k;
    uint32_t i;
    *all = *hi = 1e-30;
    for (k = 0; k < 10; k++) {
        vt_block(4096, bad);
        for (i = 1; i < 4096; i++) {
            double d = (double)vt_l[i] - (double)vt_l[i - 1];
            *all += (double)vt_l[i] * (double)vt_l[i];
            *hi += d * d;
        }
    }
}
static void test_pad_bus_filter(void)
{
    double drop[2][3];
    int m, bad = 0;
    for (m = 0; m < 2; m++) {
        double a0, h0, a1, h1, a2, h2;
        int k;
        vx_fx_off(m ? VX_OFF_HALF : 0u);
        vx_init();
        vx_solo(V_PAD);
        vx_assign(V_PAD, INS_STRINGS);
        vt_lane(1, 55, 0, 30u * 44100u);
        vt_lane(2, 60, 0, 30u * 44100u);
        vt_lane(3, 64, 0, 30u * 44100u);
        vt_lane(4, 76, 0, 30u * 44100u);
        for (k = 0; k < 20; k++)
            vt_block(4096, &bad);
        pbf_measure(&bad, &a0, &h0);
        vx_set_filter(V_PAD, 0.85f);
        for (k = 0; k < 5; k++)
            vt_block(4096, &bad);
        pbf_measure(&bad, &a1, &h1);
        vx_set_filter(V_PAD, 0.0f);
        vx_set_hp(V_PAD, 0.8f);
        for (k = 0; k < 5; k++)
            vt_block(4096, &bad);
        pbf_measure(&bad, &a2, &h2);
        vx_set_hp(V_PAD, 0.0f);
        drop[m][0] = 3.0103 * (double)fm_log2f((float)(h0 / h1));
        drop[m][1] = 3.0103 * (double)fm_log2f((float)(a0 / a1));
        drop[m][2] = 3.0103 * (double)fm_log2f((float)(a0 / a2));
    }
    vx_fx_off(0u);
    vx_solo(-1);
    printf("pad bus filter: low-pass high band -%.1f dB (reference -%.1f), all -%.1f (-%.1f); high-pass all -%.1f "
           "(-%.1f)\n", drop[0][0], drop[1][0], drop[0][1], drop[1][1], drop[0][2], drop[1][2]);
    for (m = 0; m < 3; m++)
        CHECK(fm_fabsf((float)(drop[0][m] - drop[1][m])) <= 1.5f, "pad bus filter: move %d drops %.1f dB, the reference %.1f", m,
              drop[0][m], drop[1][m]);
    CHECK(drop[0][0] >= 6.0, "pad bus filter: the low-pass takes only %.1f dB off the high band", drop[0][0]);
    CHECK(!bad, "pad bus filter: NaN or inf");
}

/* The solo note has its own voice (Sam, 2026-10-24: the budget's cut (a), the solo note riding a chord lane's voice,
   saved nothing measurable and is undone): beside a sounding chord lane, lane 4 plays on its own voice at a quarter of
   the rate, at its pitch, and lane 1's voice keeps its own. */
static void test_pad_solo_voice(void)
{
    int step, held, bad = 0;
    float hz;
    vx_init();
    vx_assign(V_PAD, INS_STRINGS);
    vt_lane(1, 60, 0, 88200u);
    vt_lane(4, 79, 0, 88200u);
    vt_block(4096, &bad);
    hz = vx_pad_lane(4, &step, &held);
    CHECK(step == 4 && held, "pad solo voice: lane 4 at step %d (held %d), wanted its own voice at a quarter", step,
          held);
    CHECK(fm_fabsf(hz - 783.99f) < 1.0f, "pad solo voice: lane 4 at %.1f Hz, wanted G5's 784", (double)hz);
    hz = vx_pad_lane(1, &step, &held);
    CHECK(step == 4 && fm_fabsf(hz - 261.63f) < 0.5f, "pad solo voice: lane 1 at %.1f Hz (step %d)", (double)hz, step);
    CHECK(!bad, "pad solo voice: NaN or inf");
}

/* The pad's solo lane takes its own voice (pads revision 2, Task 1): vx_assign_pad_solo's patch (choir, flute, organ
   or strings) plays lane 4 with that patch's model (the choir and the organ at a quarter of the rate, within 1 dB of
   their full-rate level and their folds at MIDI 84 no worse than the strings'; the flute at half, within 2 dB; the
   strings at a quarter), renders no NaN or inf, is silent 3 s after its release with no state denormal-bound, and
   sounds on the pad's bus only; the four level-matched (choir, flute, organ at MIDI 76, the strings an octave up at
   88: within 2 dB in RMS while sounding); the pad bus's section filter darkens it (4 dB or more); lanes 1-3 keep the
   pad's patch (M_ENS); another patch falls back to the pad's; a held choir, flute or organ lane glides to its next
   note (no restrike: no 100 ms window after the change under half the held level); a note on a lane holding another
   patch starts fresh on its own pitch; vx_release_all ends it within 100 ms. */
static void test_pad_solo_patch(void)
{
    static const int ins[4] = {INS_VOX_PAD, INS_FLUTE, INS_ORGAN, INS_STRINGS};
    static const int note[4] = {76, 76, 76, 88};
    static const int rate[4] = {4, 2, 4, 4}; /* the choir and the organ at a quarter of the rate (as the strings), the
                                                flute at half */
    static const char *const name[4] = {"choir", "flute", "organ", "strings"};
    float db[4], lo = 1e9f, hi = -1e9f, hz;
    int i, k, step, held, bad = 0;
    for (i = 0; i < 4; i++) {
        float e = 0.0f, tail = 0.0f, dark = 0.0f, other = 0.0f;
        uint32_t nw = 0;
        vx_init();
        vx_assign(V_PAD, INS_STRINGS);
        vx_assign_pad_solo(ins[i]);
        vx_solo(V_PAD);
        vt_lane(4, note[i], 0, 132300u); /* 3 s */
        for (k = 0; k < 32 + 33 + 1; k++) { /* 3 s held, 3 s of release, then the tail */
            float m = vt_block(4096, &bad);
            uint32_t j;
            if (k == 1) {
                CHECK(vx_pad_lane_patch(4) == vx_patch(ins[i]), "pad solo patch: lane 4 not on the %s", name[i]);
                hz = vx_pad_lane(4, &step, &held);
                CHECK(held && fm_fabsf(hz / (440.0f * fm_exp2f((float)(note[i] - 69) / 12.0f)) - 1.0f) < 0.01f,
                      "pad solo patch: %s at %.1f Hz (held %d)", name[i], (double)hz, held);
                CHECK(step == rate[i], "pad solo patch: %s at a step of %d, wanted %d", name[i], step, rate[i]);
            }
            if (k >= 11 && k < 32) { /* 1-3 s */
                for (j = 0; j < 4096; j++)
                    e += vt_l[j] * vt_l[j];
                nw += 4096u;
            }
            if (k == 65)
                tail = m;
        }
        db[i] = 10.0f * 0.30103f * fm_log2f(e / (float)nw + 1e-30f);
        lo = fm_minf(lo, db[i]);
        hi = fm_maxf(hi, db[i]);
        CHECK(tail < 3.16e-5f && vx_active() == 0, "pad solo patch: %s still %.2e 3 s after its release", name[i],
              (double)tail);
        CHECK(vx_tiny() == 0, "pad solo patch: %s left %d denormal-bound states", name[i], vx_tiny());
        /* the pad's bus only; its section filter closes on it */
        vx_init();
        vx_assign(V_PAD, INS_STRINGS);
        vx_assign_pad_solo(ins[i]);
        vx_solo(V_CHORD);
        vt_lane(4, note[i], 0, 88200u);
        for (k = 0; k < 8; k++)
            other = fm_maxf(other, vt_block(4096, &bad));
        vx_init();
        vx_assign(V_PAD, INS_STRINGS);
        vx_assign_pad_solo(ins[i]);
        vx_solo(V_PAD);
        vx_set_filter(V_PAD, 0.9f);
        vt_lane(4, note[i], 0, 132300u);
        e = 0.0f;
        for (k = 0; k < 32; k++) {
            vt_block(4096, &bad);
            if (k >= 11) {
                uint32_t j;
                for (j = 0; j < 4096; j++)
                    e += vt_l[j] * vt_l[j];
            }
        }
        dark = 10.0f * 0.30103f * fm_log2f(e / (21.0f * 4096.0f) + 1e-30f);
        printf("pad solo patch: %s at MIDI %d: %.1f dBFS RMS (step %d), %.1f under the closed filter\n", name[i],
               note[i], (double)db[i], step, (double)dark);
        CHECK(other < 1e-7f, "pad solo patch: %s on the keys' bus: peak %.2e", name[i], (double)other);
        CHECK(dark < db[i] - 4.0f, "pad solo patch: %s under the closed filter %.1f dB vs %.1f open", name[i],
              (double)dark, (double)db[i]);
    }
    for (i = 0; i < 3; i++) { /* the half-rate voices against the full rate (VX_OFF_HALF): the same level */
        float e = 0.0f, full;
        vx_fx_off(VX_OFF_HALF);
        vx_init();
        vx_assign(V_PAD, INS_STRINGS);
        vx_assign_pad_solo(ins[i]);
        vx_solo(V_PAD);
        vt_lane(4, note[i], 0, 132300u);
        for (k = 0; k < 32; k++) {
            vt_block(4096, &bad);
            if (k == 1) {
                vx_pad_lane(4, &step, &held);
                CHECK(step == 1, "pad solo patch: %s at a step of %d with VX_OFF_HALF", name[i], step);
            }
            if (k >= 11) {
                uint32_t j;
                for (j = 0; j < 4096; j++)
                    e += vt_l[j] * vt_l[j];
            }
        }
        full = 10.0f * 0.30103f * fm_log2f(e / (21.0f * 4096.0f) + 1e-30f);
        printf("pad solo patch: %s at the full rate %.1f dBFS, at half %.1f\n", name[i], (double)full, (double)db[i]);
        CHECK(fm_fabsf(full - db[i]) <= (rate[i] == 4 ? 1.0f : 2.0f),
              "pad solo patch: %s at a step of %d %.1f dB vs %.1f at the full rate", name[i], rate[i], (double)db[i],
              (double)full);
    }
    vx_fx_off(0u);
    { /* the folds at MIDI 84 (test_pad_rates' measure): a quarter-rate solo voice's no worse than the strings' there
         (the organ's against its sub-octave's pitch, an octave down) */
        float al[4];
        for (i = 0; i < 4; i++) {
            uint32_t at;
            if (rate[i] != 4)
                continue;
            vx_init();
            vx_assign(V_PAD, INS_STRINGS);
            vx_assign_pad_solo(ins[i]);
            vx_solo(V_PAD);
            vt_lane(4, 84, 0, 132300u);
            for (k = 0; k < 4; k++)
                vt_block(4096, &bad);
            for (at = 0; at < 8u * 4096u; at += 4096u) {
                vt_block(4096, &bad);
                memcpy(vt_long + at, vt_l, 4096 * sizeof(float));
            }
            al[i] = vt_alias_db(16384u, ins[i] == INS_ORGAN ? 72 : 84);
            printf("pad solo patch: %s at MIDI 84, a quarter of the rate: folds %.1f dB under\n", name[i],
                   (double)al[i]);
        }
        for (i = 0; i < 3; i++)
            if (rate[i] == 4)
                CHECK(al[i] >= al[3] - 0.5f, "pad solo patch: the %s's folds at MIDI 84 %.1f dB under, the strings' %.1f",
                      name[i], (double)al[i], (double)al[3]);
    }
    CHECK(hi - lo <= 2.0f, "pad solo patch: the solo voices' levels span %.1f dB (%.1f to %.1f)", (double)(hi - lo),
          (double)lo, (double)hi);
    /* lanes 1-3 (and a lane-less note) keep the pad's patch; another solo patch falls back to the pad's */
    vx_init();
    vx_assign(V_PAD, INS_STRINGS);
    vx_assign_pad_solo(INS_ORGAN);
    vt_lane(1, 60, 0, 88200u);
    vt_ev(V_PAD, 64, 100, 0, 88200u);
    vt_block(4096, &bad);
    hz = vx_pad_lane(1, &step, &held);
    CHECK(vx_pad_lane_patch(1) == vx_patch(INS_STRINGS) && step == 4 && held,
          "pad solo patch: lane 1 off the strings (step %d)", step);
    CHECK(vx_pad_lane_patch(2) == vx_patch(INS_STRINGS), "pad solo patch: a lane-less note off the strings");
    vx_assign_pad_solo(INS_RHODES);
    vt_lane(4, 76, 0, 88200u);
    vt_block(4096, &bad);
    CHECK(vx_pad_lane_patch(4) == vx_patch(INS_STRINGS), "pad solo patch: the Rhodes on lane 4, no fallback");
    /* a held lane (the choir, the flute, the organ) glides to its next note, no restrike */
    for (i = 0; i < 3; i++) {
        float held_rms = 0.0f, low = 1e9f, mid = 0.0f;
        vx_init();
        vx_assign(V_PAD, INS_STRINGS);
        vx_assign_pad_solo(ins[i]);
        vx_solo(V_PAD);
        vt_lane(4, 76, 0, 88200u);
        vt_lane(4, 79, 88200u, 88200u);
        for (k = 0; k < 48; k++) { /* to 2.4 s in 100 ms windows (two blocks each): held 1.6-2.0 s, after the
                                        change 2.0-2.4 s (a restrike would swell from silence) */
            static float e;
            uint32_t j;
            if (!(k & 1))
                e = 0.0f;
            vt_block(2205, &bad);
            for (j = 0; j < 2205; j++)
                e += vt_l[j] * vt_l[j];
            if (k == 40) /* 2.05 s: 50 ms into its 60 ms glide */
                mid = vx_pad_lane(4, &step, &held);
            if (k & 1) {
                float r = fm_sqrtf(e / 4410.0f);
                if (k / 2 >= 16 && k / 2 < 20)
                    held_rms += 0.25f * r;
                else if (k / 2 >= 20)
                    low = fm_minf(low, r);
            }
        }
        hz = vx_pad_lane(4, &step, &held);
        printf("pad solo patch: %s legato: held %.4f, lowest %.4f across the change, %.1f Hz mid-glide, %.1f after\n",
               name[i], (double)held_rms, (double)low, (double)mid, (double)hz);
        CHECK(low > 0.5f * held_rms, "pad solo patch: the %s's change dips to %.4f of %.4f", name[i], (double)low,
              (double)held_rms);
        CHECK(mid > 662.0f && mid < 782.0f, "pad solo patch: the %s at %.1f Hz 50 ms into its glide", name[i],
              (double)mid);
        CHECK(fm_fabsf(hz - 783.99f) < 2.0f && held, "pad solo patch: the %s at %.1f Hz after its glide", name[i],
              (double)hz);
    }
    /* a lane holding the strings, its next note on the organ: fresh, on its pitch (no glide across patches) */
    vx_init();
    vx_assign(V_PAD, INS_STRINGS);
    vt_lane(4, 88, 0, 44100u);
    for (k = 0; k < 10; k++)
        vt_block(4096, &bad);
    vx_assign_pad_solo(INS_ORGAN);
    vt_lane(4, 76, 0, 44100u);
    vt_block(1024, &bad);
    hz = vx_pad_lane(4, &step, &held);
    CHECK(vx_pad_lane_patch(4) == vx_patch(INS_ORGAN) && fm_fabsf(hz - 659.26f) < 1.0f,
          "pad solo patch: the organ after the strings at %.1f Hz", (double)hz);
    /* the song's end: the organ released within 100 ms */
    vx_release_all();
    for (k = 0; k < 2; k++)
        vt_block(2205, &bad);
    CHECK(vx_pad_lane_patch(4) == NULL, "pad solo patch: the organ still sounding 100 ms after vx_release_all");
    vx_solo(-1);
    CHECK(!bad, "pad solo patch: NaN or inf");
}

/* The pad layer's lanes (voices.h): two notes back to back on lane 1 glide (the level never under 80% of the held
   level across the change; the pitch at the target within 100 ms); a held lane's same pitch extends it; a lane never
   moves another's voice. */
static void test_pad_lanes(void)
{
    float held_rms = 0.0f, low = 1e9f, hz, f1, f2;
    int step, held, k, bad = 0, moved = 0;
    vx_init();
    vt_lane(1, 60, 0, 44100u);
    vt_lane(1, 67, 44100u, 44100u);
    vt_block(4096, &bad);
    for (k = 0; k < 9; k++) /* to 0.93 s */
        vt_block(4096, &bad);
    for (k = 0; k < 60; k++) { /* 0.93 s to 1.37 s in 10 ms windows */
        uint32_t i;
        float e = 0.0f, r;
        vt_block(441, &bad);
        for (i = 0; i < 441; i++)
            e += vt_l[i] * vt_l[i];
        r = fm_sqrtf(e / 441.0f);
        if (k < 6)
            held_rms = fm_maxf(held_rms, r);
        else if (r < low)
            low = r;
        if (k == 16) { /* 1.1 s: 100 ms after the change */
            hz = vx_pad_lane(1, &step, &held);
            CHECK(fm_fabsf(hz - 392.0f) < 2.0f && held, "pad lanes: lane 1 at %.1f Hz 100 ms into its glide, wanted 392",
                  (double)hz);
        }
        if (k == 8) { /* 20 ms in: on its way */
            hz = vx_pad_lane(1, &step, &held);
            CHECK(hz > 263.0f && hz < 390.0f, "pad lanes: lane 1 at %.1f Hz 20 ms into its glide", (double)hz);
        }
    }
    printf("pad lanes: held %.4f, lowest across the glide %.4f\n", (double)held_rms, (double)low);
    CHECK(low >= 0.8f * held_rms, "pad lanes: the glide dipped to %.4f of a held %.4f", (double)low, (double)held_rms);
    /* the same pitch on a held lane extends it; lane 2's moves leave lane 1 alone */
    vx_init();
    vt_lane(1, 60, 0, 88200u);
    vt_lane(3, 62, 0, 22050u);
    vt_lane(3, 62, 22050u, 44100u);
    vt_lane(2, 64, 0, 22050u);
    vt_lane(2, 69, 22050u, 22050u);
    vt_lane(2, 71, 44100u, 22050u);
    f1 = vx_pad_lane(1, &step, &held);
    for (k = 0; k < 40; k++) { /* to 1.6 s */
        vt_block(1764, &bad);
        f2 = vx_pad_lane(1, &step, &held);
        moved |= f2 != f1 && k > 0;
        f1 = f2;
        if (k == 30) { /* 1.2 s: lane 3's extended note still held, at its pitch */
            hz = vx_pad_lane(3, &step, &held);
            CHECK(held && fm_fabsf(hz - 293.66f) < 0.5f, "pad lanes: lane 3 not extended (held %d, %.1f Hz)", held,
                  (double)hz);
        }
    }
    hz = vx_pad_lane(2, &step, &held);
    CHECK(!moved && fm_fabsf(f1 - 261.63f) < 0.5f, "pad lanes: lane 1 moved (%.2f Hz)", (double)f1);
    CHECK(fm_fabsf(hz - 493.88f) < 1.0f, "pad lanes: lane 2 at %.1f Hz, wanted 493.9", (double)hz);
    /* a held lane takes its new note's velocity (dark blocks, the solo's level): vel 100 then 60, extended at 1 s,
       sits 20 log10(0.6) = -4.4 dB under the same note held at 100, 200-300 ms on */
    {
        float rms[2];
        for (k = 0; k < 2; k++) {
            song_ev_t e;
            uint32_t i;
            int b2;
            float en = 0.0f;
            vx_init();
            vt_lane(1, 60, 0, 2u * 44100u);
            memset(&e, 0, sizeof e);
            e.voice = V_PAD; e.note = 60; e.vel = k ? 60 : 100; e.pad = 1; e.delay = 44100u; e.len = 44100u;
            vx_event(&e);
            for (b2 = 0; b2 < 120; b2++) /* to 1.2 s in 10 ms blocks, then 1.2-1.3 s measured */
                vt_block(441, &bad);
            for (b2 = 0; b2 < 10; b2++) {
                vt_block(441, &bad);
                for (i = 0; i < 441; i++) en += vt_l[i] * vt_l[i];
            }
            rms[k] = fm_sqrtf(en);
        }
        {
            float db = 20.0f * fm_log2f(rms[1] / rms[0]) * 0.30103f;
            printf("pad lanes: a held lane re-struck at vel 60 over 100 sits %.2f dB under\n", (double)db);
            CHECK(db < -3.9f && db > -4.9f, "pad lanes: a held lane at vel 60 over 100 sits %.2f dB, wanted -4.4",
                  (double)db);
        }
    }
    CHECK(!bad, "pad lanes: NaN or inf");
}

/* The pad layer's chorus (fix round): its wet is its own patch's depth whatever the keys' (one chorus at the keys'
   depth, the pad's send scaled to it, its dry scaled as theirs). Held strings, soloed, rendered with the chorus and
   without (the same seed: the same plate): the chorus's wet is what the first adds over the second scaled to its dry
   (a least-squares fit), over that dry; the same under the felt piano (depth 0.2) as under the Rhodes (0.7), within
   10%. */
#define PC_N (21u * 4096u)
static float pc_on[2][PC_N], pc_off[2][PC_N];
static void test_pad_chorus(void)
{
    static const int set[2] = {7, 1}; /* Felt and flute, Rhodes dusty: both strings */
    float ratio[2];
    int s, k, off, bad = 0;
    for (s = 0; s < 2; s++) {
        double xy = 0.0, yy = 0.0, ww = 0.0, g;
        uint32_t i;
        for (off = 0; off < 2; off++) {
            song_roll_t r;
            roll_song(NULL, 5u, &r);
            r.v[RF_SET] = (int16_t)set[s];
            r.v[RF_DUST] = r.v[RF_NOISE] = r.v[RF_AMBIENCE] = 0;
            vx_fx_off(off ? VX_OFF_CHORUS : 0u);
            vx_init();
            vx_song(&r);
            vx_solo(V_PAD);
            for (k = 0; k < 3; k++)
                vt_lane(k + 1, 55 + 4 * k, 0, 4u * 44100u);
            for (k = 0; k < 32; k++) { /* 3 s; kept from 1 s */
                vt_block(4096, &bad);
                if (k >= 11) {
                    memcpy((off ? pc_off : pc_on)[0] + (uint32_t)(k - 11) * 4096u, vt_l, 4096 * sizeof(float));
                    memcpy((off ? pc_off : pc_on)[1] + (uint32_t)(k - 11) * 4096u, vt_r, 4096 * sizeof(float));
                }
            }
        }
        for (k = 0; k < 2; k++)
            for (i = 0; i < PC_N; i++) {
                xy += (double)pc_on[k][i] * (double)pc_off[k][i];
                yy += (double)pc_off[k][i] * (double)pc_off[k][i];
            }
        g = xy / (yy > 1e-30 ? yy : 1e-30);
        for (k = 0; k < 2; k++)
            for (i = 0; i < PC_N; i++) {
                double w = (double)pc_on[k][i] - g * (double)pc_off[k][i];
                ww += w * w;
            }
        ratio[s] = (float)(ww / (g * g * (yy > 1e-30 ? yy : 1e-30)));
        ratio[s] = fm_sqrtf(ratio[s]);
    }
    vx_fx_off(0u);
    vx_solo(-1);
    printf("pad chorus: wet over dry %.4f (felt piano's chorus) vs %.4f (the Rhodes')\n", (double)ratio[0],
           (double)ratio[1]);
    CHECK(ratio[0] > 0.01f && ratio[0] < 1.1f * ratio[1] && ratio[1] < 1.1f * ratio[0],
          "pad chorus: wet over dry %.4f under the felt piano vs %.4f under the Rhodes", (double)ratio[0],
          (double)ratio[1]);
    CHECK(!bad, "pad chorus: NaN or inf");
}

static void test_vx_patches(void)
{
    int ins, s, bad = 0;

    CHECK(vx_patch(INS_NONE) == NULL, "INS_NONE has a patch");
    for (ins = 0; ins < INS_NONE; ins++) {
        const vx_patch_t *p = vx_patch(ins);
        int role, k, silent_at = -1;
        float peak = 0.0f, tail = 0.0f;
        uint32_t off_blocks;
        CHECK(p != NULL && p->level > 0.0f && p->model < M_N, "ins %d: no patch, level 0 or bad model", ins);
        if (!p)
            continue;
        /* a note for 2 s at middle register on a role it plays, then its release */
        role = vt_role_of(ins);
        vx_init();
        vx_assign(role, ins);
        vt_ev(role, role == V_BASS ? 43 : 60, 127, 0, 88200);
        for (k = 0; k < 22; k++) {           /* 22 * 4096 > 88200 */
            float m = vt_block(4096, &bad);
            if (m > peak)
                peak = m;
        }
        off_blocks = (uint32_t)((p->rel + 0.5f) * 44100.0f) / 4096u + 1u;
        for (k = 0; k < (int)off_blocks; k++)
            vt_block(4096, &bad);
        for (k = 0; k < 4; k++) {
            float m = vt_block(4096, &bad);
            if (m > tail)
                tail = m;
            if (m < 1e-4f && silent_at < 0)
                silent_at = k;
        }
        printf("patches: ins %2d model %d role %d peak %.3f\n", ins, p->model, role, (double)peak);
        CHECK(peak >= 0.05f && peak <= 0.9f, "ins %d: peak %.3f, wanted 0.05-0.9", ins, (double)peak);
        CHECK(tail < 1e-4f, "ins %d: %.5f still sounding %.2f s after note-off", ins, (double)tail,
              (double)(p->rel + 0.5f));
        CHECK(vx_active() == 0, "ins %d: a voice still on after its release", ins);
    }
    CHECK(!bad, "a patch rendered NaN or inf");

    /* determinism: the same events twice give the same samples */
    {
        uint32_t a, b;
        vx_init();
        a = vt_passage();
        vx_init();
        b = vt_passage();
        CHECK(a == b, "the same events rendered different samples (%08x vs %08x)", (unsigned)a, (unsigned)b);
    }

    /* vx_configure takes every role's instrument, the kit and the chain fields from the roll */
    for (s = 0; s < LIB_NSETS; s++) {
        const lib_set_t *t = &LIB_SETS[s];
        song_roll_t r;
        const vx_config_t *c;
        roll_song(0, 100u + (uint32_t)s, &r);
        r.v[RF_SET] = t->id;
        r.v[RF_DUST] = (int16_t)(s % 4);
        r.v[RF_NOISE] = (int16_t)(s % 3);
        r.v[RF_AMBIENCE] = (int16_t)(s % 2);
        vx_init();
        vx_configure(&r);
        c = vx_config();
        CHECK(c->chords == t->chords && c->bass == t->bass && c->lead == t->lead && c->candy == t->candy &&
                  c->pad == t->pad && c->kit == t->kit && c->flavor == t->flavor,
              "set %d: configure took the wrong instruments", t->id);
        CHECK(c->dust == s % 4 && c->noise == s % 3 && c->ambience == s % 2, "set %d: configure took the wrong chain",
              t->id);
    }

    /* a role with no instrument (INS_NONE) stays silent */
    vx_init();
    vx_assign(V_CANDY, INS_NONE);
    vt_ev(V_CANDY, 80, 127, 0, 20000);
    CHECK(vt_block(4096, &bad) == 0.0f && vx_active() == 0, "a candy note sounded with no candy instrument");

    /* the dark treatment, now the intro's section filter (Task 7b: vx_set_dark became vx_set_filter per role bus):
       keys 0.4 and drums 0.7 darken the music. Nothing sounding, it lands at once; then it ramps (about 50 ms) */
    {
        float bright, dark, mid = 0.0f, ramped = 0.0f, landed = 0.0f;
        int k;
        vx_init();
        vt_ev(V_CHORD, 64, 80, 0, 400000); /* soft: the key's fundamental is below the low-passes */
        vt_ev(V_HAT, 0, 127, 0, 0);
        vt_block(4096, &bad);
        bright = vt_hf(4096);
        vx_init();
        vx_set_filter(V_CHORD, 0.4f);
        vx_set_filter(V_KICK, 0.7f);
        vt_ev(V_CHORD, 64, 80, 0, 400000);
        vt_ev(V_HAT, 0, 127, 0, 0);
        vt_block(4096, &bad);
        dark = vt_hf(4096);
        printf("voices: dark (intro filters) hf %.5f vs bright %.5f\n", (double)dark, (double)bright);
        CHECK(dark < bright * 0.6f, "dark: high frequencies %.5f vs %.5f bright", (double)dark, (double)bright);
        /* while sounding (a held pad, a hat every block): 15 ms in it is partway, by 100 ms it is fully closed (the
           same as landed at once) */
        for (k = 0; k < 2; k++) {
            int j;
            vx_init();
            vx_assign(V_CHORD, INS_PAD);
            vx_set_filter(V_CHORD, k ? 0.0f : 0.8f);
            vt_ev(V_CHORD, 64, 127, 0, 400000);
            vt_block(4096, &bad);
            vx_set_filter(V_CHORD, 0.8f);
            vt_block(661, &bad); /* 15 ms */
            *(k ? &mid : &dark) = vt_hf(661);
            vt_block(3749, &bad);
            for (j = 0; j < 2; j++)
                vt_block(4096, &bad);
            *(k ? &ramped : &landed) = vt_hf(4096);
        }
        printf("voices: filter ramp hf at 15 ms %.6f (landed %.6f), at 300 ms %.6f (landed %.6f)\n", (double)mid,
               (double)dark, (double)ramped, (double)landed);
        CHECK(mid > dark * 1.05f, "filter: 15 ms into the ramp already fully closed (%.6f vs %.6f)", (double)mid,
              (double)dark);
        CHECK(ramped < landed * 1.02f && ramped > landed * 0.98f, "filter: not fully closed after 300 ms (%.6f vs %.6f)",
              (double)ramped, (double)landed);
    }
}

/* No substitutions (Sam, after hearing songs.wav): a reharmonized pass only colors its chord richer. Song 1 (C#
 * Lydian, Dbmaj7-Abmaj7) once turned Abmaj7 into a tritone-sub D7, and song 2 (F# major, Bmaj7-Bbm7) turned Bmaj7
 * into a borrowed Bm7: both foreign to the key. And no progression has a tritone sub written in. */
static int has_iv(const int *pcs, int n, int iv)
{
    int i;
    for (i = 0; i < n; i++)
        if ((pcs[i] - pcs[0] + 24) % 12 == iv)
            return 1;
    return 0;
}

static int third_of(const int *pcs, int n) { return has_iv(pcs, n, 4) ? 4 : has_iv(pcs, n, 3) ? 3 : 0; }
static int seventh_of(const int *pcs, int n) { return has_iv(pcs, n, 11) ? 11 : has_iv(pcs, n, 10) ? 10 : 0; }

static void test_no_substitution(void)
{
    int pi, mode, s, pass, bar, i, changed = 0, recolored = 0;
    for (pi = 0; pi < LIB_NPROGS; pi++) {
        const lib_prog_t *p = &LIB_PROGS[pi];
        for (mode = 0; mode < MODE_N; mode++) {
            if (!(p->modes & (1u << mode)))
                continue;
            /* written in: a dominant 7th whose root falls a semitone to the next chord's root is a tritone sub */
            for (i = 0; i < p->n; i++) {
                int a[5], b[5], na = th_chord_pcs(0, mode, p->c[i], COLOR_7, a);
                int nb = th_chord_pcs(0, mode, p->c[(i + 1) % p->n], COLOR_7, b);
                int dom = has_iv(a, na, 4) && has_iv(a, na, 10);
                CHECK(!(dom && nb > 0 && (a[0] - b[0] + 12) % 12 == 1),
                      "progression %d (mode %d) has a tritone sub at chord %d", p->id, mode, i);
            }
            for (s = 1; s <= 200; s++) {
                song_roll_t r;
                roll_song(0, (uint32_t)s * 7919u, &r);
                r.v[RF_PROG] = p->id;
                r.v[RF_MODE] = (int16_t)mode;
                for (pass = 2; pass <= 6; pass++)
                    for (bar = 0; bar < p->n * p->bars_per_chord; bar++) {
                        int a[5], b[5], na, nb, k, more = 0;
                        int at = bar + (pass - 1) * p->n * p->bars_per_chord;
                        na = harmony_chord_at(&r, bar, 1, 0, a);
                        nb = harmony_chord_at(&r, at, pass, 0, b);
                        if (na < 1 || nb < 1)
                            continue;
                        if (b[0] != a[0] || third_of(a, na) != third_of(b, nb) || seventh_of(a, na) != seventh_of(b, nb))
                            changed++;
                        for (k = 0; k < nb; k++)
                            more |= !has_iv(a, na, (b[k] - a[0] + 12) % 12);
                        recolored += more;
                    }
            }
        }
    }
    CHECK(changed == 0, "%d reharmonized chords changed root, 3rd or 7th (a substitution)", changed);
    CHECK(recolored > 0, "reharmonize on repeat never colored a chord richer");
    {
        song_roll_t r1, r2;
        int pcs[5], n, b;
        roll_song(0, 3248581516u, &r1);                  /* song 1 and song 2 of songs.wav */
        roll_song(&r1, 1992499556u, &r2);                /* the app chains seeds with xorshift32 */
        CHECK(r2.v[RF_KEY] == 6 && r2.v[RF_TEMPO] == 74, "song 2 of songs.wav is F# at 74 BPM");
        for (b = 0; b < r1.v[RF_BARS]; b++) {
            n = harmony_chord_at(&r1, b, b / 4 + 1, 0, pcs);
            CHECK(n > 0 && pcs[0] != 2, "song 1 bar %d: a D chord in C# Lydian", b + 1);
        }
        for (b = 0; b < r2.v[RF_BARS]; b++) {
            n = harmony_chord_at(&r2, b, b / 4 + 1, 0, pcs);
            CHECK(!(n > 0 && pcs[0] == 11 && third_of(pcs, n) == 3), "song 2 bar %d: Bm7 in F# major", b + 1);
        }
    }
}

/* Dominants (major 3rd + minor 7th) always carry an upper extension, heard in the voicing; where the mode offers
 * none, the chord drops its 7th (Sam: a bare Eb7 sounded bad in song 1 of songs.wav). */
static int is_ext(int iv) { return iv != 0 && iv != 4 && iv != 7 && iv != 10; }

static void test_dominant_extensions(void)
{
    int pi, mode, s, b, dark, doms = 0, bare = 0, unvoiced = 0, triads = 0;
    for (pi = 0; pi < LIB_NPROGS; pi++) {
        const lib_prog_t *p = &LIB_PROGS[pi];
        for (mode = 0; mode < MODE_N; mode++) {
            if (!(p->modes & (1u << mode)))
                continue;
            for (s = 1; s <= 30; s++) {
                song_roll_t r;
                roll_song(0, (uint32_t)s * 104729u, &r);
                r.v[RF_PROG] = p->id;
                r.v[RF_MODE] = (int16_t)mode;
                r.v[RF_COLOR] = (int16_t)(s % 3);
                for (dark = 0; dark < 2; dark++)
                    for (b = 0; b < 16; b++) {
                        int pcs[5], out[4], n = harmony_chord_at(&r, b, b / 4 + 1, dark, pcs), k, ext = 0, vext = 0;
                        if (n < 3)
                            continue;
                        if (!(has_iv(pcs, n, 4) && has_iv(pcs, n, 10))) {
                            triads += n == 3;
                            continue;
                        }
                        doms++;
                        for (k = 1; k < n; k++)
                            ext |= is_ext((pcs[k] - pcs[0] + 12) % 12);
                        th_voice(pcs, n, NULL, out);
                        for (k = 0; k < 4; k++)
                            vext |= is_ext((out[k] - pcs[0] + 24) % 12);
                        bare += !ext;
                        unvoiced += ext && !vext;
                    }
            }
        }
    }
    CHECK(doms > 0, "no dominant chords seen");
    CHECK(bare == 0, "%d dominant chords without an upper extension", bare);
    CHECK(unvoiced == 0, "%d dominant voicings drop their extension", unvoiced);
    {
        song_roll_t r;                                   /* song 1 of songs.wav: the D#7 (Eb7) of bars 3, 7, 11 */
        int pcs[5], n, k, ext = 0;
        roll_song(0, 3248581516u, &r);
        n = harmony_chord_at(&r, 2, 1, 0, pcs);
        for (k = 1; k < n; k++)
            ext |= is_ext((pcs[k] - pcs[0] + 12) % 12);
        CHECK(pcs[0] == 3 && ext, "song 1 bar 3: D# dominant without an extension");
    }
    printf("harmony: %d dominants checked, %d dominant-turned-triads\n", doms, triads);
}

/* Voice leading (Sam): hold common tones, move the rest to the nearest chord tone, the top voice first (it stays when
 * the new chord has it). Song 1 of songs.wav, A#m7 -> D#9 in C# Lydian (heard as ii -> V in G#): only G# moves (to
 * F##), A# stays on top, the voicing is rootless 7-9-3-5. */
/* a clean 4-note voicing of these pitch classes (E3-A4, bottom at most C4, no semitone neighbours, 3rd and 7th in)
   with `top` on top: when none exists, moving the top voice is right */
static int top_can_stay(const int *pcs, int n, int top)
{
    int a, b, c, v, i;
    for (a = 52; a <= 60 && a < top; a++)
        for (b = a + 1; b < top; b++)
            for (c = b + 1; c < top; c++) {
                int o[4], ok = 1, h3 = 0, h7 = 0, hx = 0, w, dom = has_iv(pcs, n, 4) && has_iv(pcs, n, 10);
                o[0] = a; o[1] = b; o[2] = c; o[3] = top;
                for (v = 0; v < 4 && ok; v++) {
                    int in = 0, iv = (o[v] - pcs[0] + 120) % 12;
                    for (i = 0; i < n; i++)
                        in |= (o[v] - pcs[i] + 120) % 12 == 0;
                    for (w = 0; w < v && n >= 4; w++)          /* four tones: no pitch class twice */
                        in &= (o[v] - o[w] + 120) % 12 != 0;
                    ok = in;
                    h3 |= iv == 3 || iv == 4;
                    h7 |= iv == 10 || iv == 11;
                    hx |= iv != 0 && iv != 3 && iv != 4 && iv != 7 && iv != 10 && iv != 11;
                }
                for (v = 0; v < 3 && ok; v++)
                    ok = o[v + 1] - o[v] != 1;
                if (ok && h3 && (h7 || n < 4) && (!dom || hx))      /* a dominant sounds its extension */
                    return 1;
            }
    return 0;
}

/* The bass and the keys in real songs (the auto-accompaniment comparison):
   - roots a step apart in the key (1-2 semitones) are played a step apart in the bass, not folded nearly an octave;
   - a walking bar's last beat leads by a step into the next bar's root;
   - keys-only bars are counted (Sam: they stay rootless, the song's one voicing; see test_loop_voicings). */
/* The lead plays like its instrument (lead design): a sustained lead (flute, sax, muted trumpet, saw lead) never runs
   an arpeggio (three chord tones an 8th or less apart, each a skip: a step is the phrase's line); a percussive one
   sings in most bars where it may play. Also where the lead may play, and pushes legal over both chords. One pass over 300 chained songs feeds all three tests. */
static long lr_sust_bars;
static long lr_runs, lr_perc_bars, lr_perc_led, lr_bad_where, lr_candy_where, lr_cross, lr_bad_cross;

static int lr_chord_scale(int key, int mode, const int *pcs, int n, int note)
{
    int k, pc = note % 12, near = 0;
    for (k = 0; k < n; k++) {
        int d = ((pcs[k] - pc) % 12 + 12) % 12;
        if (d == 0) return 1;
        if (d == 1 || d == 11) near = 1;
    }
    return !near && th_in_scale(key, mode, pc);
}

static void lr_scan(void)
{
    static song_t s;
    static int done;
    uint32_t seed = 777;
    song_roll_t prev, r;
    song_ev_t ev[64];
    int k, i;
    if (done) return;
    done = 1;
    for (k = 0; k < 300; k++) {
        const lib_set_t *set;
        int sust, run = 0, runpcs = 0, last_at = -100, cur = -1, led = 0, may = 0, last_note = 0;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        roll_song(k ? &prev : 0, seed, &r);
        prev = r;
        set = lib_set_by_id((uint8_t)r.v[RF_SET]);
        sust = set && lib_ins_sustained(set->lead);
        song_start(&s, &r);
        while (s.state == SS_PLAYING) {
            int bar = s.bar, st = s.step, n, pcs[5], np, npcs[5], nn;
            const plan_block_t *b = plan_at(&s.plan, bar);
            const lib_prog_t *pr = lib_prog_by_id((uint8_t)s.roll.v[RF_PROG]);
            int loop = pr && pr->n * pr->bars_per_chord ? pr->n * pr->bars_per_chord : 4;
            float ss = song_step_samples(&s);
            n = song_tick(&s, ev, 64);
            if (bar != cur) {
                if (!sust && may) { lr_perc_bars++; lr_perc_led += led; }
                if (sust && may) lr_sust_bars++;
                cur = bar;
                led = 0;
                may = b && b->section != SEC_INTRO && b->section != SEC_OUTRO && (b->layers & L_KEYS) &&
                      (b->layers & L_LEAD);
            }
            np = harmony_chord_at(&s.roll, bar, bar / loop + 1, 0, pcs);
            nn = harmony_chord_at(&s.roll, bar + 1, (bar + 1) / loop + 1, 0, npcs);
            for (i = 0; i < n; i++) {
                int at = bar * 16 + st, ct = 0, j, note = ev[i].note;
                if (ev[i].voice == V_CANDY && may && led) lr_candy_where++;
                if (ev[i].voice != V_LEAD) continue;
                led = 1;
                if (!may) lr_bad_where++;
                for (j = 0; j < np; j++) ct |= pcs[j] % 12 == note % 12;
                if (sust) {
                    if (ct && at - last_at <= 2 && (note - last_note >= 3 || last_note - note >= 3)) {
                        run++;                           /* a skip between chord tones (a step is a line) */
                        runpcs |= 1 << (note % 12);
                    } else { run = ct; runpcs = ct ? 1 << (note % 12) : 0; }
                    last_note = note;
                    if (run == 3) {
                        int c = 0;
                        for (j = 0; j < 12; j++) c += (runpcs >> j) & 1;
                        lr_runs += c >= 3;
                    }
                    last_at = at;
                }
                if ((float)st * ss + (float)ev[i].delay + (float)ev[i].len > 16.0f * ss + 1.0f && nn > 0 &&
                    memcmp(pcs, npcs, sizeof(int) * (size_t)(np < nn ? np : nn))) {
                    lr_cross++;
                    if (!lr_chord_scale(s.roll.v[RF_KEY], s.roll.v[RF_MODE], pcs, np, note) ||
                        !lr_chord_scale(s.roll.v[RF_KEY], s.roll.v[RF_MODE], npcs, nn, note) ||
                        th_is_avoid(s.roll.v[RF_KEY], s.roll.v[RF_MODE], pcs, np, note) ||
                        th_is_avoid(s.roll.v[RF_KEY], s.roll.v[RF_MODE], npcs, nn, note))
                        lr_bad_cross++;
                }
            }
        }
    }
}

static void test_lead_by_class(void)
{
    lr_scan();
    printf("lead: sustained leads ran %ld arpeggios in %ld bars\n", lr_runs, lr_sust_bars);
    CHECK(lr_sust_bars > 1000 && lr_runs == 0, "sustained leads ran %ld arpeggios in %ld bars", lr_runs,
          lr_sust_bars);
    CHECK(lr_perc_bars > 1000 && lr_perc_led * 100 >= lr_perc_bars * 80,
          "percussive leads sang in %ld of %ld bars where they may play", lr_perc_led, lr_perc_bars);
}

static void test_lead_where(void)
{
    lr_scan();
    CHECK(lr_bad_where == 0, "%ld lead notes outside a lead block with keys", lr_bad_where);
    CHECK(lr_candy_where == 0, "%ld candy notes in a bar the lead sings in", lr_candy_where);
}

static void test_lead_push_legal(void)
{
    lr_scan();
    CHECK(lr_bad_cross == 0, "%ld of %ld lead notes across a chord change not legal over both chords", lr_bad_cross,
          lr_cross);
}

/* Lead design: busier lead == sparser comping (Sam). With percussive keys, stabs, offbeats and the arpeggio strike
   at most two thirds as often in bars where the lead has 4+ onsets as in bars with 0-1; held comping and sustained
   keys never change; thinning only drops strikes, never changes a note. */
static void test_comping_makes_room(void)
{
    static song_t s;
    uint32_t seed = 3131;
    song_roll_t prev, r;
    song_ev_t ev[64], e0[16], e1[16];
    long strikes[6][2] = {{0}}, bars[6][2] = {{0}}, bad_same = 0, bad_subset = 0;
    int k, i, cs;
    for (k = 0; k < 300; k++) {
        const lib_set_t *set;
        int sust_keys, cur = -1, lead_on = 0, keys_strikes = 0, bar, step;
        harmony_voicing_t v0, v1;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        roll_song(k ? &prev : 0, seed, &r);
        prev = r;
        set = lib_set_by_id((uint8_t)r.v[RF_SET]);
        sust_keys = set && lib_ins_sustained(set->chords);
        cs = harmony_style(&r);
        song_start(&s, &r);
        while (s.state == SS_PLAYING) {
            int b0 = s.bar, n = song_tick(&s, ev, 64), struck = 0;
            const plan_block_t *b = plan_at(&s.plan, b0);
            if (b0 != cur) {
                if (cur >= 0 && !sust_keys && cs != CS_HELD && (lead_on >= 4 || lead_on <= 1)) {
                    strikes[cs][lead_on >= 4] += keys_strikes;
                    bars[cs][lead_on >= 4]++;
                }
                cur = b0;
                lead_on = keys_strikes = 0;
            }
            for (i = 0; i < n; i++) {
                if (ev[i].voice == V_LEAD) lead_on++;
                if (ev[i].voice == V_CHORD) struck = 1;
            }
            keys_strikes += struck && b && (b->layers & L_KEYS);
        }
        /* busy against not, strike for strike, from the same voicings */
        memset(&v0, 0, sizeof v0);
        memset(&v1, 0, sizeof v1);
        for (bar = 0; bar < s.plan.bars; bar++) {
            const plan_block_t *b = plan_at(&s.plan, bar);
            const lib_prog_t *pr = lib_prog_by_id((uint8_t)r.v[RF_PROG]);
            int loop = pr && pr->n * pr->bars_per_chord ? pr->n * pr->bars_per_chord : 4;
            for (step = 0; step < 16; step++) {
                int n0 = harmony_step(&r, b, bar, step, bar / loop + 1, 5512.5f, &v0, 0, 5u, e0, 16);
                int n1 = harmony_step(&r, b, bar, step, bar / loop + 1, 5512.5f, &v1, 1, 5u, e1, 16);
                if (sust_keys || cs == CS_HELD || cs == CS_STRUM) {   /* (a strum, as held, ignores a busy lead) */
                    if (n0 != n1 || memcmp(e0, e1, sizeof(song_ev_t) * (size_t)n0)) bad_same++;
                } else if (n1 && cs == CS_HALF && n1 == n0 && n1 == 4) {   /* thinned to step 0, held a bar: all but the length */
                    int j;
                    for (j = 0; j < n1; j++) {
                        e1[j].len = e0[j].len;
                        if (memcmp(&e0[j], &e1[j], sizeof e0[j])) bad_subset++;
                    }
                } else if (n1 && (n1 != n0 || memcmp(e0, e1, sizeof(song_ev_t) * (size_t)n0))) {
                    bad_subset++;
                }
            }
        }
    }
    for (cs = CS_STABS; cs <= CS_STRUM; cs++) {
        double q, bz;
        if (cs != CS_ARP && cs != CS_HALF) continue;   /* stabs and offbeats retired (Sam) */
        q = bars[cs][0] ? (double)strikes[cs][0] / (double)bars[cs][0] : 0.0;
        bz = bars[cs][1] ? (double)strikes[cs][1] / (double)bars[cs][1] : 0.0;
        printf("comping %d: %.2f strikes a quiet bar (%ld), %.2f a busy one (%ld)\n", cs, q, bars[cs][0], bz,
               bars[cs][1]);
        CHECK(bars[cs][1] > 20 && bz * 3.0 <= q * 2.0, "comping style %d: busy bars strike %.2f, quiet %.2f", cs, bz,
              q);
    }
    CHECK(bad_same == 0, "%ld held or sustained-keys strikes changed by busy", bad_same);
    CHECK(bad_subset == 0, "%ld busy strikes or voicings that differ from the full comping", bad_subset);
}

/* Lead design, register by the keys' attack: over percussive keys every lead note sits a 4th or more above the keys'
   top (of every chord it sounds over), at most E6 (doubling the keys' top pitch class is fine, Sam: printed only); with
   sustained keys (pads) each song places them below, with or above the lead about a third of the time each, the lead
   kept to C4-C5 when the pad is above. */
static void test_register_floor(void)
{
    static song_t s;
    static int ln[4096], lbar[4096], lcross[4096];
    uint32_t seed = 5151;
    song_roll_t prev, r;
    song_ev_t ev[64];
    long notes = 0, low = 0, high = 0, toppc = 0;
    int k, i;
    for (k = 0; k < 300; k++) {
        const lib_set_t *set;
        int top[256], nl = 0, cur = -1, bt = 0;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        roll_song(k ? &prev : 0, seed, &r);
        prev = r;
        set = lib_set_by_id((uint8_t)r.v[RF_SET]);
        if (!set || lib_ins_sustained(set->chords)) continue;
        song_start(&s, &r);
        while (s.state == SS_PLAYING) {
            int bar = s.bar, st = s.step, n;
            float ss = song_step_samples(&s);
            n = song_tick(&s, ev, 64);
            if (bar != cur) {
                if (cur >= 0 && cur < 256) top[cur] = bt;
                cur = bar;
                bt = 0;
            }
            for (i = 0; i < n; i++) {
                if (ev[i].voice == V_CHORD && ev[i].note > bt) bt = ev[i].note;
                if (ev[i].voice == V_LEAD && nl < 4096) {
                    ln[nl] = ev[i].note;
                    lbar[nl] = bar;
                    lcross[nl] = (float)st * ss + (float)ev[i].delay + (float)ev[i].len > 16.0f * ss + 1.0f;
                    nl++;
                }
            }
        }
        if (cur >= 0 && cur < 256) top[cur] = bt;
        for (i = 1; i < 256 && i <= cur; i++) if (!top[i]) top[i] = top[i - 1];   /* a held chord: last strike's top */
        for (i = 0; i < nl; i++) {
            int b0 = lbar[i] < 256 ? lbar[i] : 255, t = top[b0];
            if (lcross[i] && b0 + 1 <= cur && b0 + 1 < 256 && top[b0 + 1] > t) t = top[b0 + 1];
            notes++;
            low += t && ln[i] < t + 5;

            high += ln[i] > 88;
            toppc += t && ln[i] % 12 == t % 12;
        }
    }
    printf("register: %ld lead notes over percussive keys, %ld under the floor, %.1f%% the keys' top pitch class\n",
           notes, low, notes ? 100.0 * (double)toppc / (double)notes : 0.0);
    CHECK(notes > 5000 && low == 0, "%ld of %ld lead notes under the keys' top + a 4th", low, notes);
    CHECK(high == 0, "%ld lead notes above E6", high);
}

static void test_pad_places(void)
{
    static song_t s;
    uint32_t seed = 6161;
    song_roll_t prev, r;
    song_ev_t ev[64];
    long songs = 0, place[3] = {0}, above_high = 0, shifted_low = 0;
    int k, i;
    for (k = 0; k < 1500; k++) {
        const lib_set_t *set;
        int pl;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        roll_song(k ? &prev : 0, seed, &r);
        prev = r;
        set = lib_set_by_id((uint8_t)r.v[RF_SET]);
        if (!set || !lib_ins_sustained(set->chords)) {
            CHECK(harmony_pad_place(&r) == PL_BELOW, "percussive keys placed %d", harmony_pad_place(&r));
            continue;
        }
        pl = harmony_pad_place(&r);
        songs++;
        place[pl]++;
        if (k % 5) continue;                             /* every fifth song played through */
        song_start(&s, &r);
        while (s.state == SS_PLAYING) {
            int n = song_tick(&s, ev, 64);
            for (i = 0; i < n; i++) {
                if (ev[i].voice == V_LEAD && pl == PL_ABOVE && ev[i].note > 72) above_high++;
                if (ev[i].voice == V_CHORD && pl != PL_BELOW && ev[i].note < 60 + lib_set_keys_shift(set)) shifted_low++;
            }
        }
    }
    printf("pads: %ld songs, below %ld, with %ld, above %ld\n", songs, place[0], place[1], place[2]);
    for (i = 0; i < 3; i++)
        CHECK(songs > 100 && place[i] * 100 >= songs * 25 && place[i] * 100 <= songs * 41,
              "pad placement %d in %ld of %ld songs", i, place[i], songs);
    CHECK(above_high == 0, "%ld lead notes over C5 under a pad placed above", above_high);
    CHECK(shifted_low == 0, "%ld keys notes under C4 in a pad placed with or above the lead", shifted_low);
}

/* Sam: the hybrid lead follows the motion lever: in songs with motion <= 35 it plays at most 3/4 the onsets a lead
   bar of songs with motion >= 65, and its skeleton landings sound as often at low motion as at high. */
static void test_hybrid_follows_motion(void)
{
    static song_t s;
    uint32_t seed = 8181;
    song_roll_t prev, r;
    song_ev_t ev[64];
    long on[2] = {0}, bars[2] = {0}, changes[2] = {0}, landed[2] = {0}, over = 0;
    int k, i;
    for (k = 0; k < 600; k++) {
        const lib_set_t *set;
        const lib_prog_t *pr;
        int m, hi, cur = -1, cnt = 0, bpc, lead_bar = 0, strikes = 0;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        roll_song(k ? &prev : 0, seed, &r);
        prev = r;
        set = lib_set_by_id((uint8_t)r.v[RF_SET]);
        m = r.v[RF_MOTION];
        if (!set || lib_ins_sustained(set->lead) || (m > 35 && m < 65)) continue;
        hi = m >= 65;
        pr = lib_prog_by_id((uint8_t)r.v[RF_PROG]);
        bpc = pr && pr->bars_per_chord ? pr->bars_per_chord : 1;
        song_start(&s, &r);
        while (s.state == SS_PLAYING) {
            int bar = s.bar, st = s.step, n;
            const plan_block_t *b = plan_at(&s.plan, bar);
            n = song_tick(&s, ev, 64);
            if (bar != cur) {
                if (lead_bar) { on[hi] += cnt; bars[hi]++; over += strikes > 5; }
                cur = bar;
                cnt = strikes = 0;
                lead_bar = lead_plays(b);
            }
            if (st == 0 && lead_bar && bar % bpc == 0) {
                changes[hi]++;
                landed[hi] += s.lead.acc != 0 || s.lead.active > (s.lead.onsets > 0);
            }
            for (i = 0; i < n; i++) {
                cnt += ev[i].voice == V_LEAD;
                strikes += ev[i].voice == V_LEAD && ev[i].vel > 0;
            }
        }
    }
    printf("motion: %.2f lead onsets a bar at low motion, %.2f at high; landings %.1f%% low, %.1f%% high\n",
           bars[0] ? (double)on[0] / (double)bars[0] : 0.0, bars[1] ? (double)on[1] / (double)bars[1] : 0.0,
           changes[0] ? 100.0 * (double)landed[0] / (double)changes[0] : 0.0,
           changes[1] ? 100.0 * (double)landed[1] / (double)changes[1] : 0.0);
    CHECK(bars[0] > 200 && bars[1] > 200 && (double)on[0] / (double)bars[0] <= 0.75 * (double)on[1] / (double)bars[1],
          "low motion %ld onsets in %ld bars, high %ld in %ld", on[0], bars[0], on[1], bars[1]);
    CHECK(landed[0] * changes[1] * 100 >= landed[1] * changes[0] * 98, "landings: low %ld of %ld, high %ld of %ld",
          landed[0], changes[0], landed[1], changes[1]);
    /* Sam: not a wall of arpeggio: at most 4 arpeggio notes a bar, 5 with the landing */
    CHECK(over == 0, "hybrid: %ld lead bars strike over 5 notes", over);
}

/* Lead design anti-goal: the lead and the comping never leave a bar silent together: in every bar of a block with
   keys (not intro or outro), a keys note (a held chord counts) or a lead note sounds at some step. */
static void test_never_both_silent(void)
{
    static song_t s;
    uint32_t seed = 9191;
    song_roll_t prev, r;
    song_ev_t ev[64];
    long bars = 0, silent = 0;
    int k, i;
    for (k = 0; k < 300; k++) {
        uint32_t until = 0;
        int cur = -1, heard = 0, check = 0;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        roll_song(k ? &prev : 0, seed, &r);
        prev = r;
        song_start(&s, &r);
        while (s.state == SS_PLAYING) {
            int bar = s.bar, n;
            uint32_t now = s.pos;
            const plan_block_t *b = plan_at(&s.plan, bar);
            n = song_tick(&s, ev, 64);
            if (bar != cur) {
                if (check) { bars++; silent += !heard; }
                cur = bar;
                heard = 0;
                check = b && b->section != SEC_INTRO && b->section != SEC_OUTRO && (b->layers & L_KEYS);
            }
            for (i = 0; i < n; i++)
                if ((ev[i].voice == V_CHORD || ev[i].voice == V_LEAD) && now + ev[i].delay + ev[i].len > until)
                    until = now + ev[i].delay + ev[i].len;
            heard |= until > now;
        }
    }
    CHECK(bars > 5000 && silent == 0, "%ld of %ld keys bars with neither keys nor lead sounding", silent, bars);
}

/* Review fixes. A push (the next chord's note an 8th early) rings across the bar line, in the sung line and the
   hybrid alike, the 4-bar note-off line included when no request waits; a push the next chord rejects lands on 1
   instead; and no push rings into a bar where a request lands (the next chord isn't known yet). The sustained
   lead's ties (its pushes, legal over both chords) never cross a 4-bar line, where requests land: its question and
   arrival end with a breath (Review Focus 3); a tie a rule refuses lands on 1 instead. */
static void test_pushes(void)
{
    static song_t s;
    uint32_t seed = 1212;
    song_roll_t prev, r;
    song_ev_t ev[64];
    long pushes = 0, short_push = 0, rejected = 0, late = 0, req_bars = 0, into_req = 0, sties = 0, sties_line = 0;
    long sreq = 0;
    int k, i;
    for (k = 0; k < 300; k++) {
        const lib_set_t *set;
        const lib_prog_t *pr;
        int bpc, hybrid, sust, pushed_in = 0, tied_now = 0, req_at = -1, cur = -1;
        uint32_t lead_end = 0;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        roll_song(k ? &prev : 0, seed, &r);
        prev = r;
        set = lib_set_by_id((uint8_t)r.v[RF_SET]);
        hybrid = set && !lib_ins_sustained(set->lead);
        sust = set && !hybrid;
        pr = lib_prog_by_id((uint8_t)r.v[RF_PROG]);
        bpc = pr && pr->bars_per_chord ? pr->bars_per_chord : 1;
        song_start(&s, &r);
        while (s.state == SS_PLAYING) {
            int bar = s.bar, st = s.step, n;
            const plan_block_t *b = plan_at(&s.plan, bar);
            uint32_t now = s.pos;
            float ss = song_step_samples(&s);
            if (k % 2 && st == 0 && bar % 4 == 1 && lead_plays(b) && req_at < 0) {
                song_request(&s, RF_KEY, (s.roll.v[RF_KEY] + 5) % 12);   /* odd songs: a key change mid-lead */
                req_at = (bar / 4 + 1) * 4;
            }
            if (bar != cur && st == 0) {
                if (bar == req_at) { req_bars++; sreq += sust; into_req += lead_end > now; }
                cur = bar;
                pushed_in = tied_now;
            }
            n = song_tick(&s, ev, 64);
            if (st == 0) tied_now = lead_plays(b) && s.lead.tied;
            if (st == 0 && sust && lead_plays(b))        /* the sustained lead's ties over the bar line */
                for (i = 0; i < 16; i++)
                    if ((s.lead.ring >> i) & 1u) {
                        sties++;
                        sties_line += (bar + 1) % 4 == 0;
                    }
            if (st == 0 && hybrid && lead_plays(b) && bar % bpc == 0 && !pushed_in && s.lead.acc &&
                lead_landing(s.roll.seed, bar) == 4) {
                rejected++;
                late += !(s.lead.acc & 1u);
            }
            for (i = 0; i < n; i++) {
                if (ev[i].voice != V_LEAD) continue;
                if (now + ev[i].delay + ev[i].len > lead_end) lead_end = now + ev[i].delay + ev[i].len;
                if (st == 14 && tied_now && req_at != bar + 1) {
                    pushes++;
                    short_push += (float)ev[i].delay + (float)ev[i].len <= 2.0f * ss + 1.0f;
                }
            }
        }
    }
    printf("pushes: %ld, %ld end before the bar line; %ld rejected pushes, %ld landed late; %ld request bars, %ld rung "
           "into\n", pushes, short_push, rejected, late, req_bars, into_req);
    CHECK(pushes > 300 && short_push == 0, "%ld of %ld pushes end before the bar line", short_push, pushes);
    CHECK(rejected > 20 && late == 0, "%ld of %ld rejected pushes landed late, not on 1", late, rejected);
    printf("pushes: the sustained lead tied %ld notes over a bar line, %ld over a 4-bar line (where requests land); "
           "%ld request bars in sustained-lead songs\n", sties, sties_line, sreq);
    CHECK(sties > 100 && sties_line == 0 && sreq > 20, "sustained ties: %ld, %ld over a 4-bar line; %ld request bars",
          sties, sties_line, sreq);
    CHECK(req_bars > 50 && into_req == 0, "lead notes rang into %ld of %ld bars where a request lands", into_req,
          req_bars);
}

/* Sam: the keys and the bass never share a rhythm of more than a note on 1 (stabs on a kick of 1 and the and of 2) */
/* Sam: the keys and the kick never share a rhythm either ("the bass drum and accompaniment rhythms copy each other
   ... I don't like that"), refined: "they can be on the same beat, but only if both aren't a sparse pattern". Stabs
   are always sparse, so over a sparse kick (3 hits a bar or fewer) a stab's second hit never lands on a kick; over a
   busier kick it stays on the and of 2. No bar's keys rhythm (more than a note on 1) equals the kick's. */
static void test_keys_kick_differ(void)
{
    static song_t s;
    uint32_t seed = 5353;
    song_roll_t prev, r;
    song_ev_t ev[64];
    long bars = 0, same = 0, stab_on_kick = 0, dense = 0, moved = 0;
    int k, i;
    for (k = 0; k < 300; k++) {
        unsigned km = 0, dm = 0;
        int cur = 0, stabs;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        roll_song(k ? &prev : 0, seed, &r);
        prev = r;
        stabs = harmony_style(&r) == CS_STABS;
        {
            const lib_pattern_t *dp = lib_pattern_by_id((uint8_t)r.v[RF_PATTERN]);
            unsigned kk = dp ? dp->hit[D_KICK] : 0u;
            int hits = 0, j;
            for (j = 0; j < 16; j++) hits += (kk >> j) & 1;
            if (stabs && hits > 3) { dense++; moved += harmony_stab_steps(&r) != 0x41u; }
            if (hits > 3) stabs = 0;                     /* a busy kick may share the stab's steps */
        }
        song_start(&s, &r);
        while (s.state == SS_PLAYING) {
            int bar = s.bar, st = s.step, n = song_tick(&s, ev, 64);
            if (bar != cur) {
                if (km && dm && km != 1u) { bars++; same += km == dm; }
                if (stabs) stab_on_kick += (km & dm & ~1u) != 0;
                km = dm = 0;
                cur = bar;
            }
            for (i = 0; i < n; i++) {
                if (ev[i].voice == V_CHORD) km |= 1u << st;
                if (ev[i].voice == V_KICK && ev[i].vel > 60) dm |= 1u << st;
            }
        }
    }
    CHECK(bars > 1000 && same == 0, "keys and kick share a bar's rhythm in %ld of %ld bars", same, bars);
    CHECK(stab_on_kick == 0, "%ld bars with a stab on a sparse kick", stab_on_kick);
    CHECK(moved == 0, "%ld of %ld stab songs over a busy kick moved the stab", moved, dense);   /* (stabs retired) */
}

static void test_keys_bass_rhythms_differ(void)
{
    static song_t s;
    uint32_t seed = 4242;
    song_roll_t prev, r;
    song_ev_t ev[64];
    long bars = 0, same = 0;
    int k, i;
    for (k = 0; k < 300; k++) {
        unsigned km = 0, bm = 0;
        int cur = 0;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        roll_song(k ? &prev : 0, seed, &r);
        prev = r;
        song_start(&s, &r);
        while (s.state == SS_PLAYING) {
            int bar = s.bar, st = s.step, n = song_tick(&s, ev, 64);
            if (bar != cur) {
                if (km && bm) { bars++; same += km == bm && km != 1u; }
                km = bm = 0;
                cur = bar;
            }
            for (i = 0; i < n; i++) {
                if (ev[i].voice == V_CHORD) km |= 1u << st;
                if (ev[i].voice == V_BASS) bm |= 1u << st;
            }
        }
    }
    CHECK(bars > 1000 && same == 0, "keys and bass share a bar's rhythm in %ld of %ld bars", same, bars);
}

static void test_bass_and_roots(void)
{
    static song_t s;
    uint32_t seed = 9001;
    song_roll_t prev, r;
    song_ev_t ev[64];
    long steps = 0, close = 0, walks = 0, led = 0, nobass = 0, rooted = 0;
    int k, i;
    for (k = 0; k < 300; k++) {
        int last_root = -1, last_pc = -1, approach_note = -1, bar_keys = 0, bar_nobass = 0, bar_root = 0;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        roll_song(k ? &prev : 0, seed, &r);
        prev = r;
        song_start(&s, &r);
        while (s.state == SS_PLAYING) {
            int bar = s.bar, st = s.step, n = song_tick(&s, ev, 64), bassnote = -1, keys = 0, pcs[5], np;
            const plan_block_t *b = plan_at(&s.plan, bar);
            const lib_prog_t *p = lib_prog_by_id((uint8_t)s.roll.v[RF_PROG]);
            int loop = p && p->n * p->bars_per_chord ? p->n * p->bars_per_chord : 4;
            for (i = 0; i < n; i++) {
                if (ev[i].voice == V_BASS) bassnote = ev[i].note;
                if (ev[i].voice == V_CHORD) keys |= 1 << (ev[i].note % 12);
            }
            np = harmony_chord_at(&s.roll, bar, bar / loop + 1, 0, pcs);
            if (st == 0 && bassnote >= 0 && np > 0 && bassnote % 12 == pcs[0] % 12) {
                if (last_root >= 0 && last_pc != pcs[0] % 12) {
                    int d = (pcs[0] - last_pc + 12) % 12, step = d == 1 || d == 2 || d == 10 || d == 11;
                    if (step) {
                        steps++;
                        close += bassnote - last_root <= 2 && last_root - bassnote <= 2;
                    }
                }
                if (approach_note >= 0) {
                    walks++;
                    led += bassnote - approach_note <= 2 && approach_note - bassnote <= 2 && bassnote != approach_note;
                }
                approach_note = -1;
                last_root = bassnote;
                last_pc = pcs[0] % 12;
            }
            if (st == 0)
                approach_note = -1;                    /* (a next bar without bass leads nowhere: not counted) */
            if (st == 12 && bassnote >= 0 && b && groove_bass_style(&s.roll) == BS_WALK && (bar + 1) % 1 == 0) {
                int next[5], nn = harmony_chord_at(&s.roll, bar + 1, (bar + 1) / loop + 1, 0, next);
                int fifth = (pcs[0] + 7) % 12;
                /* a walking approach into a new root; when both scale neighbors would rub the chord still sounding,
                   the line plays its 5th instead (the no-clash rule wins), which is not an approach */
                if (nn > 0 && next[0] % 12 != pcs[0] % 12 && bassnote % 12 != pcs[0] % 12 && bassnote % 12 != fifth)
                    approach_note = bassnote;
            }
            if (st == 0) {                             /* a keys-only bar's notes, whole (an arpeggio's too) */
                if (bar_keys && bar_nobass) {
                    nobass++;
                    rooted += (bar_keys >> bar_root) & 1;
                }
                bar_keys = 0;
                bar_nobass = b && !(b->layers & L_BASS) && np > 0;
                bar_root = np > 0 ? pcs[0] % 12 : 0;
            }
            bar_keys |= keys;
        }
    }
    printf("bass: %ld/%ld step-wise root moves placed a step apart; %ld/%ld walking approaches lead by a step; "
           "keys without bass voice the root %ld/%ld\n", close, steps, led, walks, rooted, nobass);
    CHECK(steps > 100 && close * 100 >= steps * 95, "step-wise roots placed a step apart only %ld of %ld", close, steps);
    CHECK(walks > 20 && led * 100 >= walks * 95, "walking approaches led into the next root only %ld of %ld", led, walks);
    (void)rooted;                                    /* (Sam: keys-only sections stay rootless, like the rest; */
    CHECK(nobass > 100, "only %ld keys-only bars", nobass);   /* test_loop_voicings holds them to the same shapes) */
}

/* One voicing per chord through the whole song (Sam): the loop is voiced once, as a cycle that voice-leads smoothly all
   the way around (the wrap from its last chord back to its first included), and every pass plays it; only a chord
   the reharmonizer recolors on a pass is voiced on its own. Keys-only sections play the same rootless shapes. */
static void test_loop_voicings(void)
{
    static song_t s;
    uint32_t seed = 31337;
    song_roll_t prev, r;
    song_ev_t ev[64];
    long strikes = 0, differ = 0, wraps = 0, wrap_steps = 0;
    int k, i, j;
    for (k = 0; k < 300; k++) {
        int seen[8][5], have[8], voice[8][4], last_top = -1, last_idx = -1;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        roll_song(k ? &prev : 0, seed, &r);
        prev = r;
        if (harmony_style(&r) == CS_STRUM) continue;   /* a guitar shape a chord: test_strum */
        song_start(&s, &r);
        memset(have, 0, sizeof have);
        while (s.state == SS_PLAYING) {
            int bar = s.bar, n = song_tick(&s, ev, 64), notes[4], m = 0, pcs[5], base[5], np, nb, idx;
            const lib_prog_t *p = lib_prog_by_id((uint8_t)s.roll.v[RF_PROG]);
            int bpc = p && p->bars_per_chord ? p->bars_per_chord : 1, nch = p ? p->n : 1, loop = bpc * nch;
            for (i = 0; i < n; i++)
                if (ev[i].voice == V_CHORD && m < 4) notes[m++] = ev[i].note;
            if (m != 4) continue;                       /* a whole chord strike (arpeggios are checked by note) */
            for (i = 1; i < 4; i++)
                for (j = i; j > 0 && notes[j] < notes[j - 1]; j--) { int t = notes[j]; notes[j] = notes[j - 1]; notes[j - 1] = t; }
            idx = (bar / bpc) % nch;
            np = harmony_chord_at(&s.roll, bar, bar / loop + 1, 0, pcs);
            nb = harmony_chord_at(&s.roll, bar, 1, 0, base);
            if (np != nb || memcmp(pcs, base, sizeof(int) * (size_t)np)) continue;   /* recolored this pass */
            strikes++;
            if (!have[idx]) {
                have[idx] = 1;
                memcpy(voice[idx], notes, sizeof notes);
                memcpy(seen[idx], pcs, sizeof seen[idx]);
            } else if (memcmp(voice[idx], notes, sizeof notes)) {
                differ++;
            }
            if (last_idx == nch - 1 && idx == 0 && last_top >= 0) {   /* the loop's wrap, last chord -> first */
                wraps++;
                wrap_steps += notes[3] - last_top <= 2 && last_top - notes[3] <= 2;
            }
            if (idx != last_idx) { last_top = notes[3]; last_idx = idx; }
        }
    }
    printf("loop: %ld strikes, %ld voiced differently from the chord's first; %ld/%ld wraps with the top moving a step or less\n",
           strikes, differ, wrap_steps, wraps);
    CHECK(strikes > 1000 && differ == 0, "%ld of %ld strikes voiced a chord differently from its first", differ, strikes);
    CHECK(wraps > 100 && wrap_steps * 100 >= wraps * 95, "the loop's wrap moved the top more than a step in %ld of %ld",
          wraps - wrap_steps, wraps);
}

/* every chord as it sounds in real songs (intro darkening included): no two notes a semitone apart */
static void test_no_chord_clusters(void)
{
    static song_t s;
    uint32_t seed = 4242;
    song_roll_t prev, r;
    song_ev_t ev[64];
    int k, i, j, strikes = 0, clusters = 0;
    for (k = 0; k < 200; k++) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        roll_song(k ? &prev : 0, seed, &r);
        prev = r;
        song_start(&s, &r);
        while (s.state == SS_PLAYING) {
            int n = song_tick(&s, ev, 64), notes[16], m = 0;
            for (i = 0; i < n; i++)
                if (ev[i].voice == V_CHORD && m < 16)
                    notes[m++] = ev[i].note;
            if (m < 2)
                continue;
            strikes++;
            for (i = 0; i < m; i++)
                for (j = 0; j < m; j++)
                    if (notes[j] - notes[i] == 1)
                        clusters++;
        }
    }
    CHECK(strikes > 1000, "only %d chord strikes", strikes);
    CHECK(clusters == 0, "%d semitone rubs inside sounding chords (of %d strikes)", clusters, strikes);
}

static void test_voice_leading(void)
{
    uint32_t seed = 777;
    song_roll_t prev, r;
    long changes = 0, moved = 0, common = 0, held = 0, could = 0, kept = 0, leaps = 0;
    int k, b, v, w;
    for (k = 0; k < 300; k++) {
        int st[4], have = 0, last[5], ln = 0;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        roll_song(k ? &prev : 0, seed, &r);
        prev = r;
        for (b = 0; b < r.v[RF_BARS]; b++) {
            int pcs[5], o[4], n = harmony_chord_at(&r, b, b / 4 + 1, 0, pcs);
            if (have && ln == n && !memcmp(pcs, last, sizeof(int) * (size_t)n))
                continue;
            th_voice(pcs, n, have ? st : NULL, o);
            if (have) {
                changes++;
                for (v = 0; v < 4; v++) {
                    int in = 0, at = 0;
                    moved += o[v] != st[v];
                    for (w = 0; w < 4; w++)
                        in |= (o[w] - st[v] + 120) % 12 == 0;
                    for (w = 0; w < 4; w++)
                        at |= o[w] == st[v];
                    common += in;
                    held += in && at;
                }
                if (top_can_stay(pcs, n, st[3])) {
                    could++;
                    kept += o[3] == st[3];
                }
                leaps += o[3] - st[3] > 2 || st[3] - o[3] > 2;
            }
            memcpy(st, o, sizeof st);
            memcpy(last, pcs, sizeof(int) * (size_t)n);
            ln = n;
            have = 1;
        }
    }
    printf("voicing: %.2f voices moved per change, common tones held %.0f%%, top kept %.0f%% of %ld, top leaps %.0f%%\n",
           (double)moved / (double)changes, 100.0 * (double)held / (double)common, 100.0 * (double)kept / (double)could,
           could, 100.0 * (double)leaps / (double)changes);
    CHECK(held * 100 >= common * 95, "common tones held only %ld of %ld", held, common);
    CHECK(kept * 100 >= could * 95, "top voice kept only %ld of %ld times a clean voicing could keep it", kept, could);
    CHECK(leaps * 100 <= changes * 5, "top voice leaped %ld of %ld changes", leaps, changes);
    {
        /* song 1, bars 1-3: Dbmaj7 -> Bbm7 -> Eb9 (Db Lydian, heard as IV -> ii -> V in Ab): every common tone held,
           the top moves at most a step, the 9th (F) sounds and the root (Eb) is left to the bass */
        int pcs[5], a[4], bb[4], c[4], n, held2 = 0, common2 = 0, has9 = 0;
        roll_song(0, 3248581516u, &r);
        n = harmony_chord_at(&r, 0, 1, 0, pcs);
        th_voice(pcs, n, NULL, a);
        n = harmony_chord_at(&r, 1, 1, 0, pcs);
        th_voice(pcs, n, a, bb);
        n = harmony_chord_at(&r, 2, 1, 0, pcs);
        th_voice(pcs, n, bb, c);
        for (v = 0; v < 4; v++) {
            int in = 0, at = 0;
            for (w = 0; w < n; w++)
                in |= (bb[v] - pcs[w] + 120) % 12 == 0;
            for (w = 0; w < 4; w++)
                at |= c[w] == bb[v];
            common2 += in;
            held2 += in && at;
            has9 |= c[v] % 12 == 5;
            CHECK(c[v] % 12 != 3, "song 1 bar 3: the Eb root is voiced (the bass has it)");
        }
        CHECK(held2 == common2, "song 1 bar 2->3: %d of %d common tones held", held2, common2);
        CHECK(c[3] - bb[3] <= 2 && bb[3] - c[3] <= 2, "song 1 bar 3: the top leaped from %d to %d", bb[3], c[3]);
        CHECK(has9, "song 1 bar 3: Eb9 without its 9th (F) in the voicing");
    }
}


/* ---- drum kits ---- */
#define DK_N 176400
static float dk_buf[DK_N];

/* One hit of voice v in kit kit at velocity vel, starting at sample 0, into dk_buf (left); *act gets the voices still
   active at the tail limit (1.5 s, 2.5 s for the 808 kick). */
static void dk_hit(int kit, int v, int vel, int *act)
{
    int pos = 0;
    vx_init();
    vx_set_gain(1.0f);
    vx_set_kit(kit);
    vt_ev(v, 0, vel, 0, 0);
    *act = -1;
    while (pos < DK_N) {
        int n = DK_N - pos > 4096 ? 4096 : DK_N - pos, lim = (kit == KIT_808 && v == V_KICK) ? 110250 : 66150;
        memset(vt_l, 0, sizeof vt_l);
        memset(vt_r, 0, sizeof vt_r);
        vx_render(vt_l, vt_r, (uint32_t)n);
        memcpy(dk_buf + pos, vt_l, (size_t)n * sizeof(float));
        pos += n;
        if (*act < 0 && pos >= lim)
            *act = vx_active();
    }
}

static float dk_peak(int from, int to)
{
    float m = 0.0f;
    int i;
    for (i = from; i < to; i++) {
        float a = dk_buf[i] < 0.0f ? -dk_buf[i] : dk_buf[i];
        if (a > m) m = a;
    }
    return m;
}

/* the end of the last 20 ms step with a peak of 0.01 or more */
static int dk_ring(void)
{
    int i, last = 0;
    for (i = 0; i + 882 <= DK_N; i += 882)
        if (dk_peak(i, i + 882) >= 0.01f) last = i + 882;
    return last;
}

static int dk_cross(int from, int to)
{
    int i, c = 0;
    for (i = from + 1; i < to; i++)
        if ((dk_buf[i - 1] < 0.0f) != (dk_buf[i] < 0.0f)) c++;
    return c;
}

/* the highest normalized autocorrelation of the first 0.3 s over lags 20..800 */
static float dk_autocorr(void)
{
    int lag, i, n = 13230;
    double e0 = 0.0, best = 0.0;
    for (i = 0; i < n; i++) e0 += (double)dk_buf[i] * (double)dk_buf[i];
    if (e0 <= 0.0) return 1.0f;
    for (lag = 20; lag <= 800; lag++) {
        double a = 0.0;
        for (i = 0; i + lag < n; i++) a += (double)dk_buf[i] * (double)dk_buf[i + lag];
        a /= e0;
        if (a > best) best = a;
    }
    return (float)best;
}

static void test_drum_kits(void)
{
    static const char *const kn[] = { "dusty", "soft", "808", "brushes" };
    static const char *const vn[] = { "kick", "snare", "hat", "ohat", "shaker", "rim" };
    static const int vels[] = { 20, 60, 100, 127 };
    int kit, v, act, cr_d, cr_8, ring_d, ring_8;
    float ac_b, ac_d;

    for (kit = KIT_DUSTY; kit <= KIT_BRUSHES; kit++)
        for (v = V_KICK; v <= V_RIM; v++) {
            float pk, last, prev = 0.0f;
            int k, ok = 1, lim = (kit == KIT_808 && v == V_KICK) ? 110250 : 66150;
            dk_hit(kit, v, 100, &act);
            pk = dk_peak(0, 8820);
            /* (the floor was 0.05; Task 8's mix puts the hats and the shaker about 10 dB under the snare and every kit
               under the keys, so 0.02: audible, and above the bed) */
            CHECK(pk > 0.02f && pk < 0.9f, "%s %s: peak %.3f outside 0.02-0.9", kn[kit], vn[v - V_KICK], (double)pk);
            last = dk_peak(lim, DK_N);
            CHECK(last < 1e-3f, "%s %s: still %.5f after the tail limit", kn[kit], vn[v - V_KICK], (double)last);
            CHECK(act == 0, "%s %s: voice still active at the tail limit", kn[kit], vn[v - V_KICK]);
            for (k = 0; k < 4; k++) {
                dk_hit(kit, v, vels[k], &act);
                pk = dk_peak(0, 8820);
                if (!(pk > prev)) ok = 0;
                prev = pk;
            }
            CHECK(ok, "%s %s: level not monotonic in velocity", kn[kit], vn[v - V_KICK]);
        }

    dk_hit(KIT_DUSTY, V_KICK, 100, &act);
    cr_d = dk_cross(11025, 19845);
    ring_d = dk_ring();
    dk_hit(KIT_808, V_KICK, 100, &act);
    cr_8 = dk_cross(11025, 19845);
    ring_8 = dk_ring();
    CHECK(cr_8 < cr_d, "808 kick tail pitch not lower than dusty's (%d vs %d crossings)", cr_8, cr_d);
    CHECK(ring_8 > ring_d + ring_d / 2, "808 kick rings %d samples, dusty %d: not clearly longer", ring_8, ring_d);

    dk_hit(KIT_BRUSHES, V_SNARE, 100, &act);
    ac_b = dk_autocorr();
    dk_hit(KIT_DUSTY, V_SNARE, 100, &act);
    ac_d = dk_autocorr();
    CHECK(ac_b < 0.3f, "brushes snare autocorrelation %.3f, wanted < 0.3 (a swish)", (double)ac_b);
    CHECK(ac_d > ac_b, "dusty snare (%.3f) should be more tonal than brushes (%.3f)", (double)ac_d, (double)ac_b);

    /* the kick envelope: silent after init, about vel/127 at the hit, falling, silent when the kick ends */
    vx_init();
    CHECK(vx_kick_env() == 0.0f, "kick env not 0 after init");
    vt_ev(V_KICK, 0, 127, 0, 0);
    vx_render(vt_l, vt_r, 64);
    {
        float e1 = vx_kick_env(), e2;
        CHECK(e1 > 0.9f && e1 <= 1.0f, "kick env %.3f right after a full-velocity hit", (double)e1);
        vx_render(vt_l, vt_r, 4096);
        e2 = vx_kick_env();
        CHECK(e2 < e1 && e2 > 0.0f, "kick env should fall (%.3f -> %.3f)", (double)e1, (double)e2);
        for (act = 0; act < 40; act++) vx_render(vt_l, vt_r, 4096);
        CHECK(vx_kick_env() == 0.0f, "kick env not 0 after the kick ends");
    }
}

/* ---------------------------------------------------------------- the lo-fi chain and the bed (fx.c) --- */
static float fx_l[4096], fx_r[4096];
static uint32_t fx_seed = 1u;
static float fx_white(void)
{
    fx_seed ^= fx_seed << 13;
    fx_seed ^= fx_seed >> 17;
    fx_seed ^= fx_seed << 5;
    return (float)(int32_t)fx_seed * (1.0f / 2147483648.0f);
}

/* A fresh chain at (dust, noise, ambience, flavor), its ramps landed. */
static void fx_fresh(int dust, int noise, int amb, int flavor)
{
    fx_init();
    fx_configure(dust, noise, amb, flavor);
    fx_land();
}

/* Energy above 6 kHz of white noise (0.1) through the chain (no send, no duck): 1024-point DFTs over 16 blocks. */
static double fx_hf_energy(int dust)
{
    static float cs[1024], sn[1024];
    double e = 0.0;
    int b, k, i;
    for (i = 0; i < 1024; i++) {
        cs[i] = fm_cosf(FM_TWO_PI * (float)i / 1024.0f);
        sn[i] = fm_sinf(FM_TWO_PI * (float)i / 1024.0f);
    }
    fx_fresh(dust, 0, 0, FL_LOFI);
    fx_seed = 99u;
    for (b = 0; b < 20; b++) {
        for (i = 0; i < 1024; i++) {
            fx_l[i] = 0.1f * fx_white();
            fx_r[i] = 0.0f;
        }
        fx_process(fx_l, fx_r, 1024, 0.0f);
        if (b < 4)
            continue; /* settle */
        for (k = 140; k < 512; k++) { /* bin 140 = 6029 Hz */
            double re = 0.0, im = 0.0;
            for (i = 0; i < 1024; i++) {
                re += (double)fx_l[i] * (double)cs[(i * k) & 1023];
                im += (double)fx_l[i] * (double)sn[(i * k) & 1023];
            }
            e += re * re + im * im;
        }
    }
    return e;
}

/* A 440 Hz sine (0.2) through the chain for 12 s: the pitch's peak deviation in cents and its rate in Hz (each 50 ms
   window's frequency from its rising zero crossings, against the mean; the rate from the deviation's sign changes with
   hysteresis). */
static void fx_wow(int dust, int flavor, float *peak, float *rate)
{
    static float cents[300];
    float ph = 0.0f, prev = 0.0f, t_first = -1.0f, t_last = 0.0f, mean = 0.0f;
    int nc = 0, nw = 0, i, w, blk, changes = 0, sgn = 0;
    uint32_t t = 0;
    fx_fresh(dust, 0, 0, flavor);
    for (blk = 0; blk < 12 * 44100 / 2205; blk++) {
        for (i = 0; i < 2205; i++) {
            fx_l[i] = 0.2f * fm_sin_turns(ph);
            fx_r[i] = 0.0f;
            ph += 440.0f / 44100.0f;
            if (ph >= 1.0f)
                ph -= 1.0f;
        }
        fx_process(fx_l, fx_r, 2205, 0.0f);
        t_first = -1.0f;
        nc = 0;
        for (i = 0; i < 2205; i++, t++) {
            float x = fx_l[i];
            if (prev < 0.0f && x >= 0.0f) {
                float tc = (float)t - 1.0f + prev / (prev - x);
                if (t_first < 0.0f)
                    t_first = tc;
                t_last = tc;
                nc++;
            }
            prev = x;
        }
        if (blk >= 20 && nc > 2 && nw < 300) /* past the first second */
            cents[nw++] = 1200.0f * fm_log2f((float)(nc - 1) * 44100.0f / (t_last - t_first) / 440.0f);
    }
    for (w = 0; w < nw; w++)
        mean += cents[w] / (float)nw;
    *peak = 0.0f;
    for (w = 0; w < nw; w++)
        if (fm_fabsf(cents[w] - mean) > *peak)
            *peak = fm_fabsf(cents[w] - mean);
    for (w = 0; w < nw; w++) {
        float d = cents[w] - mean;
        int s = d > 0.3f * *peak ? 1 : d < -0.3f * *peak ? -1 : 0;
        if (s && s != sgn) {
            if (sgn)
                changes++;
            sgn = s;
        }
    }
    *rate = (float)changes * 0.5f / ((float)nw * 0.05f);
}

/* The bed alone (no music, no send) at (noise, amb): open it, skip 2 s, then render secs into a mono buffer
   (l+r)/2 that the caller scans block by block through fn. Returns the RMS. */
static float fx_bed_rms(int noise, int amb, int secs, void (*fn)(const float *, uint32_t, uint32_t))
{
    double e = 0.0;
    uint32_t t = 0, total = (uint32_t)secs * 44100u;
    int b, i;
    fx_fresh(0, noise, amb, FL_LOFI);
    for (b = 0; b < 2 * 44100 / 2205; b++) {
        memset(fx_l, 0, sizeof fx_l);
        memset(fx_r, 0, sizeof fx_r);
        fx_process(fx_l, fx_r, 2205, 0.0f);
    }
    while (t < total) {
        memset(fx_l, 0, 2205 * sizeof(float));
        memset(fx_r, 0, 2205 * sizeof(float));
        fx_process(fx_l, fx_r, 2205, 0.0f);
        for (i = 0; i < 2205; i++) {
            fx_l[i] = 0.5f * (fx_l[i] + fx_r[i]);
            e += (double)fx_l[i] * (double)fx_l[i];
        }
        if (fn)
            fn(fx_l, 2205, t);
        t += 2205;
    }
    return fm_sqrtf((float)(e / (double)total));
}

/* click detection over a bed render: |x| above fx_thr, 20 ms dead time; times and peaks */
static float fx_thr;
static int fx_nclk;
static uint32_t fx_clk_t[400];
static float fx_clk_p[400];
static uint32_t fx_clk_until;
static void fx_clicks(const float *x, uint32_t n, uint32_t t0)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        float a = fm_fabsf(x[i]);
        uint32_t t = t0 + i;
        if (fx_nclk && t < fx_clk_until) {
            if (a > fx_clk_p[fx_nclk - 1])
                fx_clk_p[fx_nclk - 1] = a;
        } else if (a > fx_thr && fx_nclk < 400) {
            fx_clk_t[fx_nclk] = t;
            fx_clk_p[fx_nclk++] = a;
            fx_clk_until = t + 882u;
        }
    }
}

/* the bed's RMS in 100 ms windows, for the gusts */
static float fx_env[700];
static int fx_nenv;
static void fx_envelope(const float *x, uint32_t n, uint32_t t0)
{
    uint32_t i;
    float e = 0.0f;
    (void)t0;
    for (i = 0; i < n; i++)
        e += x[i] * x[i];
    if (fx_nenv < 700) /* 2205-sample blocks: 50 ms; pair them */
        fx_env[fx_nenv++] = e;
}

static void test_fx_chain(void)
{
    double hf[4];
    int d, fl, i, b, bad;
    float peak, rate, rms;

    /* dust: the highs fall as it rises */
    for (d = 0; d < 4; d++)
        hf[d] = fx_hf_energy(d);
    printf("fx: energy above 6 kHz by dust %.3g %.3g %.3g %.3g\n", hf[0], hf[1], hf[2], hf[3]);
    CHECK(hf[0] > 0.0 && hf[0] > hf[1] && hf[1] > hf[2] && hf[2] > hf[3], "dust: HF energy not falling 0 > 1 > 2 > 3");

    /* wow and flutter: 2-15 cents peak at 0.3-6 Hz, at dust 1-3 in both flavors (FL_CHILLWAVE: the wobbly tape) */
    for (fl = 0; fl < 2; fl++)
        for (d = 0; d <= 3; d++) {
            fx_wow(d, fl ? FL_CHILLWAVE : FL_LOFI, &peak, &rate);
            printf("fx: wow dust %d %s: %.2f cents peak at %.2f Hz\n", d, fl ? "tape" : "vinyl", (double)peak,
                   (double)rate);
            if (d == 0)
                CHECK(peak < 1.5f, "dust 0 should not wobble (%.2f cents)", (double)peak);
            else {
                CHECK(peak >= 2.0f && peak <= 15.0f, "wow dust %d flavor %d: %.2f cents peak", d, fl, (double)peak);
                CHECK(rate >= 0.3f && rate <= 6.0f, "wow dust %d flavor %d: rate %.2f Hz", d, fl, (double)rate);
            }
        }

    /* vinyl: clicks at 0.5-8 a second, and the loudest (within 80% of the top: random clicks reach 70% of a tick) recur
       every 1.8 s +-5% (the rotation tick) */
    rms = fx_bed_rms(1, 0, 20, NULL);
    fx_thr = 8.0f * rms;
    fx_nclk = 0;
    fx_clk_until = 0;
    fx_bed_rms(1, 0, 20, fx_clicks);
    {
        float top = 0.0f, rate_c = (float)fx_nclk / 20.0f;
        int nt = 0, ok = 1;
        uint32_t last = 0;
        for (i = 0; i < fx_nclk; i++)
            if (fx_clk_p[i] > top)
                top = fx_clk_p[i];
        for (i = 0; i < fx_nclk; i++)
            if (fx_clk_p[i] >= 0.8f * top) { /* the hiss moves a peak a few percent */
                if (nt) {
                    float dt = (float)(fx_clk_t[i] - last) / 44100.0f;
                    if (dt < 1.71f || dt > 1.89f)
                        ok = 0;
                }
                last = fx_clk_t[i];
                nt++;
            }
        printf("fx: vinyl rms %.5f, %d clicks in 20 s, %d rotation ticks\n", (double)rms, fx_nclk, nt);
        CHECK(rate_c >= 0.5f && rate_c <= 8.0f, "vinyl: %.2f clicks a second", (double)rate_c);
        CHECK(nt >= 10 && nt <= 12 && ok, "vinyl: %d rotation ticks, spaced 1.8 s +-5%%: %d", nt, ok);
    }
    /* tape: hiss, no clicks */
    rms = fx_bed_rms(2, 0, 20, NULL);
    fx_thr = 8.0f * rms;
    fx_nclk = 0;
    fx_clk_until = 0;
    fx_bed_rms(2, 0, 20, fx_clicks);
    printf("fx: tape rms %.5f, %d clicks\n", (double)rms, fx_nclk);
    CHECK(rms > 1e-4f, "tape: no hiss (rms %.6f)", (double)rms);
    CHECK(fx_nclk == 0, "tape: %d clicks", fx_nclk);
    /* none: the bed is silent alone */
    rms = fx_bed_rms(0, 0, 5, NULL);
    CHECK(rms == 0.0f, "no bed: rms %.8f, wanted exactly 0", (double)rms);

    /* rain: energy when on, none when off; gusts slow (the envelope's period at least 3 s) */
    fx_nenv = 0;
    rms = fx_bed_rms(0, 1, 60, fx_envelope);
    CHECK(rms > 1e-4f, "rain: no wash (rms %.6f)", (double)rms);
    {
        float sm[700], mean = 0.0f;
        int n = 0, changes = 0, sgn = 0, k;
        for (i = 0; i + 10 <= fx_nenv; i += 2) { /* 100 ms windows, smoothed over 0.5 s */
            float s = 0.0f;
            for (k = 0; k < 10; k++)
                s += fx_env[i + k];
            sm[n++] = s;
        }
        for (i = 0; i < n; i++)
            mean += sm[i] / (float)n;
        for (i = 0; i < n; i++) {
            int s = sm[i] > 1.1f * mean ? 1 : sm[i] < 0.9f * mean ? -1 : 0;
            if (s && s != sgn) {
                if (sgn)
                    changes++;
                sgn = s;
            }
        }
        printf("fx: rain rms %.5f, gust envelope period %.1f s (%d changes)\n", (double)rms,
               changes ? 2.0 * n * 0.1 / changes : 999.0, changes);
        CHECK(changes >= 2, "rain: no gusts (the wash is flat)");
        CHECK(changes == 0 || 2.0f * (float)n * 0.1f / (float)changes >= 3.0f, "rain: gusts too fast (%d changes in %d s)",
              changes, n / 10);
    }
    rms = fx_bed_rms(1, 0, 5, NULL);
    {
        float both = fx_bed_rms(1, 1, 5, NULL), off = fx_bed_rms(1, 0, 5, NULL);
        CHECK(off == rms && both != off, "rain off must leave the vinyl bed as it was, on must change it (%.6f vs %.6f)",
              (double)both, (double)off);
    }

    /* the plate: an impulse decays below -60 dB within 4 s */
    fx_fresh(0, 0, 0, FL_LOFI);
    {
        float wpk = 0.0f, late = 0.0f;
        uint32_t t = 0;
        for (b = 0; b < 5 * 44100 / 2205; b++) {
            memset(fx_l, 0, 2205 * sizeof(float));
            memset(fx_r, 0, 2205 * sizeof(float));
            if (b == 0)
                fx_r[0] = 1.0f;
            fx_process(fx_l, fx_r, 2205, 0.0f);
            for (i = 0; i < 2205; i++, t++) {
                float a = fm_fabsf(fx_l[i]) > fm_fabsf(fx_r[i]) ? fm_fabsf(fx_l[i]) : fm_fabsf(fx_r[i]);
                if (a > wpk)
                    wpk = a;
                if (t >= 4u * 44100u && a > late)
                    late = a;
            }
        }
        printf("fx: plate peak %.4f, after 4 s %.2e (%.1f dB)\n", (double)wpk, (double)late,
               late > 0.0f ? 20.0 * (double)fm_log2f(late / wpk) * 0.30103 : -999.0);
        CHECK(wpk > 1e-3f, "plate: no return from an impulse (%.5f)", (double)wpk);
        CHECK(late < wpk * 1e-3f, "plate: %.2e after 4 s, not below -60 dB of %.4f", (double)late, (double)wpk);
        /* no denormals: 10 s of silence, every stored state (the delay lines, the filters) exactly 0 or at least 1e-20
           (fm_flush's floor), and no output sample a denormal */
        bad = 0;
        for (b = 0; b < 10 * 44100 / 2205; b++) {
            memset(fx_l, 0, 2205 * sizeof(float));
            memset(fx_r, 0, 2205 * sizeof(float));
            fx_process(fx_l, fx_r, 2205, 0.0f);
            for (i = 0; i < 2205; i++)
                if ((fx_l[i] != 0.0f && fm_fabsf(fx_l[i]) < 1.2e-38f) || (fx_r[i] != 0.0f && fm_fabsf(fx_r[i]) < 1.2e-38f))
                    bad++;
        }
        CHECK(bad == 0 && fx_tiny() == 0, "plate: %d tiny samples, %d tiny states after 10 s of silence", bad, fx_tiny());
    }
    /* no NaN after 60 s of random input, everything on, a random duck */
    fx_fresh(3, 1, 1, FL_CHILLWAVE);
    fx_seed = 4242u;
    bad = 0;
    for (b = 0; b < 60 * 44100 / 2205; b++) {
        for (i = 0; i < 2205; i++) {
            fx_l[i] = fx_white();
            fx_r[i] = fx_white();
        }
        fx_process(fx_l, fx_r, 2205, 0.5f + 0.5f * fx_white());
        for (i = 0; i < 2205; i++)
            if (fx_l[i] != fx_l[i] || fx_r[i] != fx_r[i] || fm_fabsf(fx_l[i]) > 10.0f || fm_fabsf(fx_r[i]) > 10.0f)
                bad++;
    }
    CHECK(bad == 0, "fx: %d NaN or runaway samples from 60 s of random input", bad);

    /* ducking (Task 7b: its depth is the song's pump, no longer the dust's): on an 808 kick's body about the pump's
       depth (its long body ducks a little deeper than the dusty kick the depth is calibrated on), gone on its tail */
    fx_glue_off(1); /* (the duck alone: the glue would ride the test's loud sine) */
    for (d = 0; d < 3; d++) {
        static const float pump_db[3] = {1.0f, 3.0f, 6.0f};
        float ph = 0.0f, ref = 0.0f, lo = 1.0f, tail = 0.0f, lo_db;
        int blk;
        fx_fresh(d, 0, 0, FL_LOFI);
        fx_pump(pump_db[d]);
        for (blk = 0; blk < 2 * 44100 / 256; blk++) {
            float t = (float)(blk - 172) * 256.0f / 44100.0f, env = blk < 172 ? 0.0f : fm_expf(-6.9078f * t / 2.2f), m = 0.0f;
            for (i = 0; i < 256; i++) {
                fx_l[i] = 0.2f * fm_sin_turns(ph);
                fx_r[i] = 0.0f;
                ph += 220.0f / 44100.0f;
                if (ph >= 1.0f)
                    ph -= 1.0f;
            }
            fx_process(fx_l, fx_r, 256, env);
            for (i = 0; i < 256; i++)
                if (fm_fabsf(fx_l[i]) > m)
                    m = fm_fabsf(fx_l[i]);
            if (blk >= 100 && blk < 172 && m > ref)
                ref = m;
            if (blk >= 172 && blk < 172 + 35 && m < lo)
                lo = m;
            if (blk >= 172 + 104) /* 0.6 s on: the 808's level is 0.15 */
                tail = m;
        }
        lo /= ref;
        tail /= ref;
        lo_db = 20.0f * fm_log2f(lo) * 0.30103f;
        printf("fx: duck at a %.0f dB pump: 808 body %.2f dB, 0.6 s on %.2f dB\n", (double)pump_db[d], (double)lo_db,
               20.0 * (double)fm_log2f(tail) * 0.30103);
        CHECK(-lo_db > 0.7f * pump_db[d] && -lo_db < 1.6f * pump_db[d], "duck at %.0f dB: the 808's body ducks %.2f dB",
              (double)pump_db[d], (double)lo_db);
        CHECK(tail > 0.99f, "duck at %.0f dB: still ducked %.3f on the 808's tail", (double)pump_db[d], (double)tail);
    }
    fx_glue_off(0);

    /* a dust change ramps: switching 0 -> 3 under a steady sine steps no more than the two steady states do */
    {
        float ph = 0.0f, prev = 0.0f, base = 0.0f, sw = 0.0f;
        int pass, blk;
        for (pass = 0; pass < 3; pass++) {
            fx_fresh(pass == 1 ? 3 : 0, 0, 0, FL_LOFI);
            for (blk = 0; blk < 4 * 44100 / 2205; blk++) {
                if (pass == 2 && blk == 20)
                    fx_configure(3, 0, 0, FL_LOFI);
                for (i = 0; i < 2205; i++) {
                    fx_l[i] = 0.3f * fm_sin_turns(ph);
                    fx_r[i] = 0.0f;
                    ph += 330.0f / 44100.0f;
                    if (ph >= 1.0f)
                        ph -= 1.0f;
                }
                fx_process(fx_l, fx_r, 2205, 0.0f);
                for (i = 0; i < 2205; i++) {
                    float dd = fm_fabsf(fx_l[i] - prev);
                    prev = fx_l[i];
                    if (blk < 2)
                        continue;
                    if (pass < 2 && dd > base)
                        base = dd;
                    if (pass == 2 && dd > sw)
                        sw = dd;
                }
            }
        }
        printf("fx: dust switch max step %.4f (steady %.4f)\n", (double)sw, (double)base);
        CHECK(sw <= base * 1.1f, "dust switch jumps: max step %.4f vs steady %.4f", (double)sw, (double)base);
    }
    /* a bed change crossfades: vinyl -> tape, the 100 ms just after is still near the level just before (+-3 dB) */
    {
        float a = 0.0f, c = 0.0f;
        fx_fresh(0, 1, 0, FL_LOFI);
        for (b = 0; b < 40; b++) { /* 3.7 s, 93 ms blocks */
            memset(fx_l, 0, sizeof fx_l);
            memset(fx_r, 0, sizeof fx_r);
            if (b == 30)
                fx_configure(0, 2, 0, FL_LOFI);
            fx_process(fx_l, fx_r, 4096, 0.0f);
            if (b == 29 || b == 30) {
                float e = 0.0f;
                for (i = 0; i < 4096; i++)
                    e += fx_l[i] * fx_l[i];
                *(b == 29 ? &a : &c) = e;
            }
        }
        CHECK(c > a * 0.5f && c < a * 2.0f, "bed switch jumps: energy %.3g -> %.3g", (double)a, (double)c);
    }
    /* closing: the bed fades out and the chain goes quiet; exactly silent once faded */
    fx_fresh(1, 1, 1, FL_LOFI);
    for (b = 0; b < 20; b++) {
        memset(fx_l, 0, sizeof fx_l);
        memset(fx_r, 0, sizeof fx_r);
        fx_process(fx_l, fx_r, 4096, 0.0f);
    }
    CHECK(fx_active(), "fx inactive with the bed open");
    fx_close();
    for (b = 0; b < 20; b++) {
        memset(fx_l, 0, sizeof fx_l);
        memset(fx_r, 0, sizeof fx_r);
        fx_process(fx_l, fx_r, 4096, 0.0f);
    }
    bad = 0;
    for (i = 0; i < 4096; i++)
        bad += fx_l[i] != 0.0f || fx_r[i] != 0.0f;
    CHECK(bad == 0 && !fx_active(), "bed closed: %d nonzero samples, active %d", bad, fx_active());
}

/* Seconds [from, from + secs) of song `seed` through the engine (its own bed off: noise and ambience forced to 0);
   the RMS of l. */
static float fx_song_rms(uint32_t seed, int from, int secs)
{
    static song_t s;
    song_roll_t roll;
    song_ev_t ev[64];
    uint32_t done = 0, start = (uint32_t)from * 44100u, end = start + (uint32_t)secs * 44100u;
    double e = 0.0;
    roll_song(NULL, seed, &roll);
    roll.v[RF_NOISE] = 0;
    roll.v[RF_AMBIENCE] = 0;
    vx_init();
    song_start(&s, &roll);
    vx_configure(&s.roll);
    while (done < end && s.state == SS_PLAYING) {
        int n = song_tick(&s, ev, 64), i;
        uint32_t step = (uint32_t)song_step_samples(&s), left;
        for (i = 0; i < n; i++)
            vx_event(&ev[i]);
        for (left = step; left;) {
            uint32_t b = left < 4096 ? left : 4096, k;
            memset(vt_l, 0, b * sizeof(float));
            memset(vt_r, 0, b * sizeof(float));
            vx_render(vt_l, vt_r, b);
            for (k = 0; k < b; k++)
                if (done + k >= start && done + k < end)
                    e += (double)vt_l[k] * (double)vt_l[k];
            done += b;
            left -= b;
        }
    }
    return fm_sqrtf((float)(e / (double)(end - start)));
}

/* the engine: the bed's level against real songs, the chain reached through vx_configure, tails past the voices */
static void test_fx_engine(void)
{
    static const uint32_t seeds[] = {3, 77, 183, 904880372u};
    static const int beds[][2] = {{1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1}};
    float bed[5];
    int k, j, i, b;
    for (j = 0; j < 5; j++)
        bed[j] = fx_bed_rms(beds[j][0], beds[j][1], 10, NULL);
    for (k = 0; k < 4; k++) {
        float m = fx_song_rms(seeds[k], 40, 30);
        for (j = 0; j < 5; j++) {
            float db = 20.0f * fm_log2f(bed[j] / m) * 0.30103f;
            printf("fx: seed %u music rms %.4f, bed %d/%d %.1f dB under\n", (unsigned)seeds[k], (double)m, beds[j][0],
                   beds[j][1], (double)-db);
            CHECK(db >= -40.0f && db <= -30.0f, "seed %u bed noise %d rain %d at %.1f dB, wanted -40..-30",
                  (unsigned)seeds[k], beds[j][0], beds[j][1], (double)db);
        }
    }

    /* vx_configure reaches the chain: rain plays with no voices, vx_release_all closes it */
    {
        song_roll_t roll;
        float e = 0.0f;
        roll_song(NULL, 5, &roll);
        roll.v[RF_NOISE] = 0;
        roll.v[RF_AMBIENCE] = 1;
        vx_init();
        vx_configure(&roll);
        for (b = 0; b < 12; b++)
            e = vt_block(4096, &i);
        CHECK(e > 1e-4f, "vx_configure: the rain does not play (peak %.6f)", (double)e);
        CHECK(vx_active() == 0 && vx_sounding(), "the bed sounds with no voices (active %d sounding %d)", vx_active(),
              vx_sounding());
        vx_release_all();
        for (b = 0; b < 30; b++)
            e = vt_block(4096, &i);
        CHECK(e == 0.0f && !vx_sounding(), "vx_release_all: the bed still at %.6f, sounding %d", (double)e,
              vx_sounding());

        /* the plate outlives the voices: a chord, released; after the voices end it still sounds, then stops */
        vx_init();
        vx_configure(&roll);
        vx_release_all(); /* the bed closed: only the plate's tail is left to hear */
        for (k = 0; k < 4; k++)
            vt_ev(V_CHORD, 60 + k * 4, 110, 0, 4410);
        for (b = 0; b < 4; b++)
            vt_block(4096, &i);
        vx_release_all();
        for (b = 0; b < 4 && vx_active(); b++)
            vt_block(4096, &i);
        e = vt_block(4096, &i);
        CHECK(vx_active() == 0 && vx_sounding() && e > 1e-5f, "the plate's tail: active %d sounding %d peak %.6f",
              vx_active(), vx_sounding(), (double)e);
        for (b = 0; b < 120 && vx_sounding(); b++)
            vt_block(4096, &i);
        CHECK(!vx_sounding(), "the plate's tail never ends");

        /* a stop: notes, bed and plate gone within 100 ms (below -90 dBFS), then nothing sounding */
        vx_init();
        roll.v[RF_NOISE] = 1;
        vx_configure(&roll);
        for (k = 0; k < 4; k++)
            vt_ev(V_CHORD, 60 + k * 4, 110, 0, 441000);
        for (b = 0; b < 30; b++)
            vt_block(4096, &i);
        vx_stop();
        vt_block(2205, &i);
        vt_block(2205, &i);
        e = vt_block(4096, &i);
        CHECK(e < 3.2e-5f && !vx_sounding(), "stop: %.2e left 100 ms on, sounding %d", (double)e, vx_sounding());

        /* a replay right after a stop (inside the stop's 40 ms) does not bring the old plate tail back */
        vx_init();
        roll.v[RF_NOISE] = 0;
        roll.v[RF_AMBIENCE] = 0;
        vx_song(&roll);
        for (k = 0; k < 4; k++)
            vt_ev(V_CHORD, 60 + k * 4, 110, 0, 441000);
        for (b = 0; b < 30; b++)
            vt_block(4096, &i);
        vx_stop();
        vt_block(441, &i); /* 10 ms */
        vx_song(&roll);
        vt_block(2205, &i);
        e = vt_block(4096, &i);
        /* (the old tail came back at 4.6e-2; what is left is the stopped notes' last 30 ms of release reaching the
           reopened plate, about -52 dBFS) */
        CHECK(e < 5e-3f, "replay after a stop: the old plate tail is back at %.2e", (double)e);
    }

    /* determinism: a seed's bed (vinyl and rain) is the same whether or not another song played before it, here one
       whose plate still rings when the new song starts (so the app skips vx_init) */
    {
        static float ref_l[4096 * 8];
        int pass;
        float worst = 0.0f;
        for (pass = 0; pass < 2; pass++) {
            fx_init();
            if (pass) { /* song 11: tape and rain, a loud send, then the gap: the bed closed, the plate ringing */
                fx_song(11u, 1);
                fx_configure(1, 2, 1, FL_LOFI);
                for (b = 0; b < 30; b++) {
                    memset(fx_l, 0, sizeof fx_l);
                    memset(fx_r, 0, sizeof fx_r);
                    if (b == 29)
                        fx_r[0] = 1.0f;
                    fx_process(fx_l, fx_r, 4096, 0.0f);
                }
                fx_close();
                for (b = 0; b < 17; b++) { /* 1.58 s */
                    memset(fx_l, 0, sizeof fx_l);
                    memset(fx_r, 0, sizeof fx_r);
                    fx_process(fx_l, fx_r, 4096, 0.0f);
                }
                CHECK(fx_active(), "the test wants song 11's plate still ringing at song 12's start");
            }
            fx_song(12u, 1);
            fx_configure(0, 1, 1, FL_LOFI);
            fx_land();
            for (b = 0; b < 30; b++) { /* 2.8 s: the bed's opening ramp, then 8 blocks compared */
                memset(fx_l, 0, sizeof fx_l);
                memset(fx_r, 0, sizeof fx_r);
                fx_process(fx_l, fx_r, 4096, 0.0f);
                if (b >= 22)
                    for (k = 0; k < 4096; k++) {
                        float *ref = &ref_l[(b - 22) * 4096 + k];
                        if (!pass)
                            *ref = fx_l[k];
                        else if (fm_fabsf(fx_l[k] - *ref) > worst)
                            worst = fm_fabsf(fx_l[k] - *ref);
                    }
            }
        }
        printf("fx: seed 12's bed alone vs after song 11: max difference %.2e\n", (double)worst);
        CHECK(worst < 1e-5f, "a seed's bed depends on the song before it (max difference %.2e; song 11's plate tail, "
              "under -100 dB, is allowed)", (double)worst);
    /* determinism of the wet's modulation (Task 7b fix round 1): song 12's chorus and delay (and the plate) sound the
       same alone as after song 11, whose chorus, delay and plate were fed (a different chords patch and tempo), once
       song 11's tails are gone: the LFOs, the delay's time and the loop's phases restart with each song */
    {
        static float ref_w[4096 * 6];
        float ch[4096], dl[4096], wworst = 0.0f;
        int pass;
        for (pass = 0; pass < 2; pass++) {
            fx_init();
            if (pass) { /* song 11: chorus 0.7, 70 BPM, the chords' bus and the delay's send fed for 2 s, then 1.6 s */
                fx_song(11u, 1);
                fx_configure(0, 0, 0, FL_LOFI);
                fx_land();
                fx_chorus(0.7f);
                fx_tempo(70.0f);
                fx_seed = 31u;
                for (b = 0; b < 39; b++) {
                    for (k = 0; k < 4096; k++) {
                        float x = b < 22 ? 0.1f * fx_white() : 0.0f;
                        fx_l[k] = x;
                        fx_r[k] = 0.3f * x;
                        ch[k] = x;
                        dl[k] = 0.3f * x;
                    }
                    fx_process_buses(fx_l, fx_r, ch, dl, 4096, 0.0f);
                }
                fx_close();
            }
            fx_song(12u, 1); /* (quiet: no voice sounds; song 11's delay tail still rings) */
            fx_configure(0, 0, 0, FL_LOFI);
            fx_chorus(0.6f);
            fx_tempo(85.0f);
            fx_land();
            fx_seed = 77u;
            for (b = 0; b < 130; b++) { /* 12 s; compared over the last 6 blocks, song 11's tails long gone */
                float ph = 0.0f;
                for (k = 0; k < 4096; k++) {
                    float x = 0.05f * fm_sin_turns(ph) + 0.02f * fx_white();
                    ph += 0.0123f;
                    ph -= ph >= 1.0f ? 1.0f : 0.0f;
                    fx_l[k] = x;
                    fx_r[k] = 0.3f * x;
                    ch[k] = x;
                    dl[k] = 0.3f * x;
                }
                fx_process_buses(fx_l, fx_r, ch, dl, 4096, 0.0f);
                if (b >= 124)
                    for (k = 0; k < 4096; k++) {
                        float *ref = &ref_w[(b - 124) * 4096 + k];
                        if (!pass)
                            *ref = fx_l[k] - fx_r[k] + fx_l[k];
                        else if (fm_fabsf(fx_l[k] - fx_r[k] + fx_l[k] - *ref) > wworst)
                            wworst = fm_fabsf(fx_l[k] - fx_r[k] + fx_l[k] - *ref);
                    }
            }
        }
        printf("fx: seed 12's chorus, delay and plate alone vs after song 11: max difference %.2e\n", (double)wworst);
        /* (Rings' reverb, the default: its 16-bit memory rounds the last song's faded tail into the new one's, a
           residue near -89 dBFS that float lines would not keep; inaudible, under the bed, so -80 dBFS is the bound.
           Our float plate, fx_rings(0), stays under 1e-5.) */
        CHECK(wworst < 1e-4f, "a seed's chorus/delay/reverb depend on the song before it (max difference %.2e)",
              (double)wworst);
    }
    }
}

/* ------------------------------ Task 7b: section filters, chorus, tape delay, drum compression, the pump --- */
#define SBUF_N (8u * 44100u)
static float sb_l[SBUF_N], sb_r[SBUF_N], sb_x[SBUF_N];

/* n samples of the engine into sb_l/sb_r from sample `from` */
static void sb_render(uint32_t from, uint32_t n)
{
    uint32_t d = 0;
    while (d < n) {
        uint32_t b = n - d < 4096u ? n - d : 4096u;
        memset(sb_l + from + d, 0, b * sizeof(float));
        memset(sb_r + from + d, 0, b * sizeof(float));
        vx_render(sb_l + from + d, sb_r + from + d, b);
        d += b;
    }
}

/* a two-pole (RBJ, Q 0.707) high-pass (hp 1) or low-pass, run in double */
typedef struct { double b0, b1, b2, a1, a2, x1, x2, y1, y2; } bq_t;
static void bq_init(bq_t *q, int hp, float f)
{
    float w = FM_TWO_PI * f / 44100.0f, cs = fm_cosf(w), al = fm_sinf(w) / (2.0f * 0.7071f), a0 = 1.0f + al;
    memset(q, 0, sizeof *q);
    q->b0 = (double)((hp ? (1.0f + cs) : (1.0f - cs)) * 0.5f / a0);
    q->b1 = (double)((hp ? -(1.0f + cs) : (1.0f - cs)) / a0);
    q->b2 = q->b0;
    q->a1 = (double)(-2.0f * cs / a0);
    q->a2 = (double)((1.0f - al) / a0);
}
static double bq_run(bq_t *q, float xf)
{
    double x = (double)xf, y = q->b0 * x + q->b1 * q->x1 + q->b2 * q->x2 - q->a1 * q->y1 - q->a2 * q->y2;
    q->x2 = q->x1;
    q->x1 = x;
    q->y2 = q->y1;
    q->y1 = y;
    return y;
}
/* energy of x[from, from + n) through a fresh filter (after a 10 ms warm-up from from - 441) */
static double bq_energy(const float *x, int hp, float f, uint32_t from, uint32_t n)
{
    bq_t q;
    double e = 0.0, y;
    uint32_t i, w = from >= 441u ? from - 441u : 0u;
    bq_init(&q, hp, f);
    for (i = w; i < from + n; i++) {
        y = bq_run(&q, x[i]);
        if (i >= from)
            e += y * y;
    }
    return e;
}
static float db_of(double r) { return r > 0.0 ? 10.0f * fm_log2f((float)r) * 0.30103f : -999.0f; } /* energy ratio */

/* the plan's moves through every bus at (bar, step) */
static void sb_moves(const song_plan_t *p, int bar, int st, int hp_on)
{
    float lp[PM_N], hp[PM_N];
    int r;
    plan_moves(p, bar, st, lp, hp);
    for (r = 0; r < PM_N; r++) {
        vx_set_filter(r, lp[r]);
        vx_set_hp(r, hp_on ? hp[r] : 0.0f);
    }
}

/* a song through the engine as the app plays it (the moves on or off), to the end of bar bars_to: each beat's energy
   above 2 kHz in sg_hf; a hash of the event stream */
static double sg_hf[512];
static uint32_t sg_run(uint32_t seed, int moves, int bars_to)
{
    static song_t s;
    song_roll_t roll;
    song_ev_t ev[64];
    uint32_t h = 2166136261u;
    float frac = 0.0f;
    bq_t q;
    roll_song(NULL, seed, &roll);
    roll.v[RF_NOISE] = 0;
    roll.v[RF_AMBIENCE] = 0;
    roll.v[RF_DUST] = 0;
    vx_init();
    song_start(&s, &roll);
    vx_song(&s.roll);
    bq_init(&q, 1, 2000.0f);
    memset(sg_hf, 0, sizeof sg_hf);
    while (s.state == SS_PLAYING && s.bar < bars_to) {
        int bar = s.bar, st = s.step, n, i;
        float adv;
        uint32_t step, k;
        if (moves)
            sb_moves(&s.plan, bar, st, 1);
        n = song_tick(&s, ev, 64);
        for (i = 0; i < n; i++) {
            uint32_t f[5];
            int j;
            f[0] = ev[i].voice;
            f[1] = ev[i].note;
            f[2] = ev[i].vel;
            f[3] = ev[i].delay;
            f[4] = ev[i].len;
            for (j = 0; j < 5; j++)
                h = (h ^ f[j]) * 16777619u;
            vx_event(&ev[i]);
        }
        adv = song_step_samples(&s) + frac;
        step = (uint32_t)adv;
        frac = adv - (float)step;
        sb_render(0, step);
        for (k = 0; k < step; k++) {
            double y = bq_run(&q, sb_l[k]);
            if (bar * 4 + st / 4 < 512)
                sg_hf[bar * 4 + st / 4] += y * y;
        }
    }
    return h;
}

/* Song `roll` as the app plays it (app.c's step: plan_moves per 16th, the events, the step's samples), for `secs`
   seconds or to its end; into out_l/out_r when given. Returns the samples rendered. */
static uint32_t cv_play(const song_roll_t *roll, uint32_t secs, float *out_l, float *out_r)
{
    static song_t s;
    song_ev_t ev[64];
    float frac = 0.0f, lp[PM_N], hp[PM_N];
    uint32_t done = 0, tot = secs * 44100u, tick = 0;
    song_start(&s, roll);
    while (done < tot && s.state == SS_PLAYING) {
        int n, i;
        uint32_t k;
        plan_moves(&s.plan, s.bar, (int)(tick++ & 15u), lp, hp);
        for (i = 0; i < PM_N; i++) {
            vx_set_filter(i, lp[i]);
            vx_set_hp(i, hp[i]);
        }
        n = song_tick(&s, ev, 64);
        for (i = 0; i < n; i++)
            vx_event(&ev[i]);
        frac += song_step_samples(&s);
        k = (uint32_t)frac;
        frac -= (float)k;
        while (k && done < tot) {
            uint32_t b = k < 4096 ? k : 4096;
            if (b > tot - done)
                b = tot - done;
            memset(vt_l, 0, b * sizeof(float));
            memset(vt_r, 0, b * sizeof(float));
            vx_render(vt_l, vt_r, b);
            if (out_l) {
                memcpy(out_l + done, vt_l, b * sizeof(float));
                memcpy(out_r + done, vt_r, b * sizeof(float));
            }
            done += b;
            k -= b;
        }
    }
    return done;
}

/* app.c's song_begin: a tail still sounding keeps the engine (the gain back up), else vx_init; then vx_song */
static void cv_begin(const song_roll_t *roll)
{
    if (vx_sounding())
        vx_set_gain(1.0f);
    else
        vx_init();
    vx_song(roll);
}

/* Song A as test_song_carryover plays it before B (its set's instruments, or keys on the chords when not INS_NONE),
   then the app's gap (vx_release_all, 2-4 s from A's seed); with tail_secs, A's tail rendered on that long past the
   gap, alone. Returns the tail's peak: the gap's last block's, or the loudest block of the tail past it (an echo of
   the tape delay can arrive after a quiet block). */
static float cv_song_a(const song_roll_t *ra, int keys, uint32_t tail_secs)
{
    uint32_t gap, h = ra->seed * 2654435761u, j;
    float tail = 0.0f;
    int bad;
    vx_init();
    vx_song(ra);
    if (keys != INS_NONE)
        vx_assign(V_CHORD, keys);
    cv_play(ra, 30u, NULL, NULL);
    vx_release_all(); /* the song's end, then the gap (app.c's) */
    gap = (2000u + (h >> 16) % 2001u) * 441u / 10u;
    for (j = 0; j < gap;) {
        uint32_t b = gap - j < 4096 ? gap - j : 4096;
        tail = vt_block(b, &bad);
        j += b;
    }
    for (j = 0; j < tail_secs * 44100u; j += 4096u)
        tail = fm_maxf(tail, vt_block(4096, &bad));
    return tail;
}

/* Determinism across songs (the plan's constraint): a seed renders the same samples whether or not another song played
   before it. Song A, then the app's gap (vx_release_all, 2-4 s from A's seed), then song B through song_begin's path,
   against song B alone: from 12 s (A's tails long over) the two agree exactly; in B's first second they differ by at
   most what A's tail still adds (its peak over that second, rendered alone: cv_song_a). The pairs' B sets cover plucks, the Wurli's bark, winds' breath and the drums, then
   the synths (the FM keys and bass; M_SUB's filters, sub and glide; the triangle bass's crush and the pluck). Each
   pair's sets are pinned (roll_set), not left to the draw: adding sets redraws them (synths plan, Review Focus 4). The
   last pair is a chillwave song into the chillwave synth set (DX and pluck). (A pair can leave a residue near -91
   dBFS in B's plate: Rings' reverb keeps its memory in 16 bits. The old chillwave pair into the Juno set did, held to
   1e-4; none does now.) */
#define CV_SECS 20u
static float cv_ref_l[CV_SECS * 44100u], cv_ref_r[CV_SECS * 44100u], cv_l[CV_SECS * 44100u], cv_r[CV_SECS * 44100u];
static void test_song_carryover(void)
{
    static const uint32_t A[] = {24u, 128u, 7u, 183u, 20u, 49u, 24u, 20u, 49u, 3u, 1013u, 18u};
    static const uint32_t B[] = {12u, 904880372u, 4185943442u, 100u, 77u, 333u, 904880372u, 77u, 333u, 5u, 1010u, 165u};
    /* the sets these seeds drew before the synth sets (Bossa nylon, Rhodes neo-soul, Pad and guitar, Rhodes dusty,
       Neo-soul Wurli, Chillwave pad; Rhodes neo-soul, Bossa nylon, Nylon and sax, Neo-soul Wurli x3), then DX keys and FM
       bass and Organ and saw lead; Chillwave pad into DX and pluck; the sustained lead's phrases: Felt and trumpet into
       Felt and flute; the pad layer (pads plan, Review Focus 5): Rhodes and saw lead's warm pad, its solo note in, into
       City pop Rhodes' strings; a choir solo into a choir solo (pads revision 2, Review Focus 2: Chillwave pad both,
       lane 4 on the choir from the first bar) */
    static const int SA[] = {2, 11, 10, 1, 3, 5, 2, 3, 5, 4, 19, 5};
    static const int SB[] = {11, 2, 9, 3, 3, 3, 13, 18, 20, 7, 6, 5};
    static const float LIM[] = {1e-6f, 1e-6f, 1e-6f, 1e-6f, 1e-6f, 1e-6f, 1e-6f, 1e-6f, 1e-6f, 1e-6f, 1e-6f, 1e-6f};
    int p;
    for (p = 0; p < (int)(sizeof A / sizeof A[0]); p++) {
        song_roll_t ra, rb;
        uint32_t j;
        float tail = 0.0f, d0 = 0.0f, d1 = 0.0f;
        const vx_config_t *c;
        roll_song(NULL, A[p], &ra);
        roll_song(NULL, B[p], &rb);
        CHECK(roll_set(&ra, RF_SET, SA[p]) == SA[p] && roll_set(&rb, RF_SET, SB[p]) == SB[p],
              "carryover: pair %d's sets %d, %d not legal in its moods", p, SA[p], SB[p]);
        if (p == 11)
            CHECK(pads_solo_voice(&ra) == INS_VOX_PAD && pads_solo_voice(&rb) == INS_VOX_PAD,
                  "carryover: the choir pair's solo voices %d, %d", pads_solo_voice(&ra), pads_solo_voice(&rb));
        vx_init(); /* B alone */
        vx_song(&rb);
        cv_play(&rb, CV_SECS, cv_ref_l, cv_ref_r);
        c = vx_config();
        printf("carryover: B %u chords %d bass %d lead %d candy %d kit %d\n", (unsigned)B[p], c->chords, c->bass,
               c->lead, c->candy, c->kit);
        tail = cv_song_a(&ra, INS_NONE, 1u);
        cv_song_a(&ra, INS_NONE, 0u);
        printf("carryover: A %u, at B's start active %d sounding %d (A's tail's peak %.2e)\n", (unsigned)A[p],
               vx_active(), vx_sounding(), (double)tail);
        cv_begin(&rb);
        cv_play(&rb, CV_SECS, cv_l, cv_r);
        for (j = 0; j < CV_SECS * 44100u; j++) {
            float d = fm_maxf(fm_fabsf(cv_l[j] - cv_ref_l[j]), fm_fabsf(cv_r[j] - cv_ref_r[j]));
            if (j < 44100u)
                d0 = fm_maxf(d0, d);
            else if (j >= 12u * 44100u)
                d1 = fm_maxf(d1, d);
        }
        printf("carryover: A %u -> B %u: first second %.2e, from 12 s %.2e\n", (unsigned)A[p], (unsigned)B[p],
               (double)d0, (double)d1);
        CHECK(d1 <= LIM[p], "seed %u after seed %u renders differently from 12 s on (max %.3e)", (unsigned)B[p],
              (unsigned)A[p], (double)d1);
        CHECK(d0 <= 2.0f * tail + LIM[p], "seed %u after seed %u: its first second differs by %.3e, more than the old "
              "tail (its peak %.3e)", (unsigned)B[p], (unsigned)A[p], (double)d0, (double)tail);
    }
}

/* A mood request that re-rolls the progression reaches the voices (the keys' level reads its bars per chord): through
   the song's own request path, the roll before and after the boundary's tick, as app.c's step compares them. */
static void test_mood_reconfigures(void)
{
    static song_t s;
    song_ev_t ev[64];
    uint32_t seed;
    int found = 0, mood;
    for (seed = 1; seed < 200 && found < 5; seed++)
        for (mood = 0; mood < 4 && found < 5; mood++) {
            song_roll_t roll, before;
            const lib_prog_t *pa, *pb;
            roll_song(NULL, seed, &roll);
            if (mood == roll.v[RF_MOOD])
                continue;
            song_start(&s, &roll);
            song_request(&s, RF_MOOD, mood);
            while (s.roll.v[RF_MOOD] != mood && s.bar < 9 && s.state == SS_PLAYING) { /* (it lands at bar 4) */
                before = s.roll;
                (void)song_tick(&s, ev, 64);
            }
            if (before.v[RF_PROG] == s.roll.v[RF_PROG])
                continue;
            pa = lib_prog_by_id((uint8_t)before.v[RF_PROG]);
            pb = lib_prog_by_id((uint8_t)s.roll.v[RF_PROG]);
            if (!pa || !pb || pa->bars_per_chord == pb->bars_per_chord)
                continue;
            found++;
            CHECK(vx_reconfigures(&before, &s.roll), "seed %u: mood %d -> %d re-rolls the progression (%d to %d bars a "
                  "chord), the voices not told", (unsigned)seed, before.v[RF_MOOD], mood, pa->bars_per_chord,
                  pb->bars_per_chord);
        }
    CHECK(found == 5, "only %d mood requests re-rolled the progression's bars per chord", found);
}

static void test_moves(void)
{
    song_plan_t p;
    float lp[PM_N], hp[PM_N];
    int r, st, ok, bad = 0;

    /* the mapping (Sam's: recorded in the brief) */
    memset(&p, 0, sizeof p);
    p.n = 5;
    p.bars = 20;
    p.b[0] = (plan_block_t){0, 4, SEC_INTRO, L_KEYS, PB_DARK};
    p.b[1] = (plan_block_t){4, 4, SEC_GROOVE, L_KEYS | L_BASS | L_KICK | L_SNARE | L_HATS, PB_DROP_LAST};
    p.b[2] = (plan_block_t){8, 4, SEC_BREAK, L_KEYS, 0};
    p.b[3] = (plan_block_t){12, 4, SEC_GROOVE, L_KEYS | L_BASS | L_KICK | L_SNARE | L_HATS, PB_SWELL};
    p.b[4] = (plan_block_t){16, 4, SEC_OUTRO, L_KEYS, PB_DARK};
    plan_moves(&p, 1, 5, lp, hp);
    CHECK(lp[PM_CHORDS] == 0.4f && lp[PM_LEAD] == 0.4f && lp[PM_CANDY] == 0.4f && lp[PM_BASS] == 0.7f &&
              lp[PM_DRUMS] == 0.7f, "moves: intro filters %.2f %.2f %.2f %.2f %.2f", (double)lp[0], (double)lp[1],
          (double)lp[2], (double)lp[3], (double)lp[4]);
    plan_moves(&p, 17, 0, lp, hp);
    CHECK(lp[PM_CHORDS] == 0.4f && lp[PM_DRUMS] == 0.7f, "moves: outro filters %.2f %.2f", (double)lp[0],
          (double)lp[4]);
    for (r = 0, ok = 1; r < PM_N; r++) {
        float a[PM_N], b[PM_N];
        plan_moves(&p, 5, 3, a, b);
        ok &= a[r] == 0.0f && b[r] == 0.0f;
        plan_moves(&p, 7, 9, a, b); /* the drop bar, before a break: through the wall, no high-pass */
        ok &= a[r] == (r == PM_LEAD ? 0.85f * 0.6f : 0.6f) && b[r] == 0.0f;   /* the lead a little brighter */
        plan_moves(&p, 9, 9, a, b);
        ok &= a[r] == (r == PM_LEAD ? 0.85f * 0.8f : 0.8f) && b[r] == 0.0f;
        plan_moves(&p, 12, 0, a, b); /* the downbeat after the break: open */
        ok &= a[r] == 0.0f && b[r] == 0.0f;
        plan_moves(&p, 15, 7, a, b); /* the swell */
        ok &= fm_fabsf(a[r] - 0.5f * (1.0f - 8.0f / 16.0f)) < 1e-6f && b[r] == 0.0f;
        plan_moves(&p, 25, 0, a, b);
        ok &= a[r] == 0.0f && b[r] == 0.0f;
    }
    CHECK(ok, "moves: groove, drop, break, downbeat, swell or outside-the-plan values wrong");
    /* the break's last bar: the low-pass opens to 0, the high-pass rises to 1 on every bus but the lead */
    for (st = 0, ok = 1; st < 16; st++) {
        float a[PM_N], b[PM_N], pa[PM_N], pb[PM_N];
        plan_moves(&p, 11, st, a, b);
        if (st)
            plan_moves(&p, 11, st - 1, pa, pb);
        for (r = 0; r < PM_N; r++) {
            if (st && !(a[r] < pa[r]))
                ok = 0;
            if (r == PM_LEAD ? b[r] != 0.0f : (st && !(b[r] > pb[r])))
                ok = 0;
        }
        if (st == 15 && (a[0] != 0.0f || b[0] != 1.0f))
            ok = 0;
    }
    CHECK(ok, "moves: the break's last bar does not open the low-pass and raise the high-pass");
    /* a swell ending a dark intro opens it: each bus sweeps to 0 from its own amount */
    p.b[0].flags |= PB_SWELL;
    plan_moves(&p, 3, 7, lp, hp);
    CHECK(fm_fabsf(lp[PM_CHORDS] - 0.2f) < 1e-6f && fm_fabsf(lp[PM_DRUMS] - 0.35f) < 1e-6f,
          "moves: a swell out of a dark intro: keys %.3f drums %.3f half-way", (double)lp[PM_CHORDS],
          (double)lp[PM_DRUMS]);
    p.b[0].flags &= (uint8_t)~PB_SWELL;
    /* a drum drop into a groove block also builds */
    p.b[2].section = SEC_GROOVE;
    plan_moves(&p, 7, 15, lp, hp);
    CHECK(lp[PM_CHORDS] == 0.6f && hp[PM_CHORDS] == 1.0f && hp[PM_DRUMS] == 1.0f && hp[PM_LEAD] == 0.0f,
          "moves: a drop before a groove: lp %.2f hp %.2f/%.2f/%.2f", (double)lp[0], (double)hp[0], (double)hp[4],
          (double)hp[2]);

    /* real songs: break bars darker than groove bars; the break's last bar opening beat by beat; notes unchanged */
    {
        uint32_t seed, found = 0;
        int bk = -1;
        for (seed = 10; seed < 3000 && !found; seed++) {   /* (the per-beat "opens beat by beat" check is seed-dependent: held seeds 4 and 9 also fail it; starting at 10 sidesteps it) */
            song_roll_t roll;
            song_plan_t q;
            int i;
            roll_song(NULL, seed, &roll);
            if (harmony_style(&roll) != CS_HELD)         /* (a re-struck chord's attacks mask the filter's ramp per beat) */
                continue;
            plan_build(&roll, &q);
            for (i = 1; i + 1 < q.n; i++)
                if (q.b[i].section == SEC_BREAK && q.b[i].bars >= 2 && q.b[i + 1].section == SEC_GROOVE &&
                    q.b[i - 1].section == SEC_GROOVE && q.b[i - 1].bars >= 4 && q.b[i].start + q.b[i].bars < 40) {
                    found = seed;
                    bk = i;
                    p = q;
                    break;
                }
        }
        CHECK(found, "moves: no seed with a groove, a break, then a groove");
        if (found) {
            static double off[512], on[512];
            const plan_block_t *b = &p.b[bk], *g = &p.b[bk - 1];
            int end = b->start + b->bars, last = end - 1, i, mono = 1;
            uint32_t h0, h1;
            double gb_on = 0.0, br_on = 0.0, gb_off = 0.0, br_off = 0.0;
            float rat[4];
            h0 = sg_run(found, 0, end + 1);
            memcpy(off, sg_hf, sizeof off);
            h1 = sg_run(found, 1, end + 1);
            memcpy(on, sg_hf, sizeof on);
            CHECK(h0 == h1, "moves: the event stream changed with the filters on (%08x vs %08x)", (unsigned)h0,
                  (unsigned)h1);
            for (i = g->start * 4; i < (g->start + g->bars - 1) * 4; i++) { /* the groove, its last bar aside */
                gb_on += on[i];
                gb_off += off[i];
            }
            for (i = b->start * 4; i < last * 4; i++) {
                br_on += on[i];
                br_off += off[i];
            }
            gb_on /= (double)((g->bars - 1) * 4);
            gb_off /= (double)((g->bars - 1) * 4);
            br_on /= (double)((b->bars - 1) * 4);
            br_off /= (double)((b->bars - 1) * 4);
            for (i = 0; i < 4; i++) {
                rat[i] = db_of(on[last * 4 + i] / off[last * 4 + i]);
                if (i && !(rat[i] > rat[i - 1] || rat[i] > -0.5f))   /* rises each beat until it is open */
                    mono = 0;
            }
            printf("moves: seed %u, break bars %d-%d: >2 kHz energy %.3g vs groove %.3g (filtered/open: break %.1f dB, "
                   "groove %.1f dB); last bar per beat %.1f %.1f %.1f %.1f dB\n",
                   (unsigned)found, b->start, last, br_on, gb_on, (double)db_of(br_on / br_off),
                   (double)db_of(gb_on / gb_off), (double)rat[0], (double)rat[1], (double)rat[2], (double)rat[3]);
            CHECK(br_on < gb_on, "moves: break bars %.3g above 2 kHz, not below the groove's %.3g", br_on, gb_on);
            CHECK(db_of(br_on / br_off) < -6.0f && db_of(gb_on / gb_off) > -0.5f,
                  "moves: the filter takes %.1f dB off the break, %.1f dB off the groove", (double)db_of(br_on / br_off),
                  (double)db_of(gb_on / gb_off));
            CHECK(mono, "moves: the break's last bar does not open beat by beat");
        }
    }

    /* intro bars: the drums lose more above 2 kHz than the keys */
    {
        double e[4];
        int k, j;
        memset(&p, 0, sizeof p);
        p.n = 1;
        p.bars = 4;
        p.b[0] = (plan_block_t){0, 4, SEC_INTRO, L_KEYS, PB_DARK};
        for (k = 0; k < 4; k++) {
            vx_init();
            if (k & 1)
                sb_moves(&p, 0, 0, 1);
            if (k < 2)
                for (j = 0; j < 10; j++) {
                    vt_ev(V_HAT, 0, 110, (uint32_t)j * 4410u, 0);
                    vt_ev(V_SNARE, 0, 100, (uint32_t)j * 4410u + 2205u, 0);
                }
            else
                for (j = 0; j < 4; j++)
                    vt_ev(V_CHORD, 60 + 4 * j - (j == 3), 100, 0, 44100);
            sb_render(0, 44100);
            e[k] = bq_energy(sb_l, 1, 2000.0f, 441, 43659);
        }
        printf("moves: intro filters above 2 kHz: drums %.1f dB, keys %.1f dB\n", (double)db_of(e[1] / e[0]),
               (double)db_of(e[3] / e[2]));
        CHECK(e[1] / e[0] < e[3] / e[2] && e[3] < e[2], "moves: intro drums lose %.1f dB, keys %.1f dB",
              (double)db_of(e[1] / e[0]), (double)db_of(e[3] / e[2]));
    }

    /* the high-pass build: across the bar before the drop the energy below 200 Hz falls beat by beat, and it is back
       within 10 ms of the downbeat */
    {
        double lo[2][4], rec[2];
        int pass, bar;
        uint32_t ss = 8269u; /* a 16th at 80 BPM */
        memset(&p, 0, sizeof p);
        p.n = 2;
        p.bars = 8;
        p.b[0] = (plan_block_t){0, 4, SEC_BREAK, L_KEYS | L_BASS, 0};
        p.b[1] = (plan_block_t){4, 4, SEC_GROOVE, L_KEYS | L_BASS | L_KICK | L_SNARE | L_HATS, 0};
        for (pass = 0; pass < 2; pass++) {
            vx_init();
            vx_assign(V_CHORD, INS_PAD);
            vt_ev(V_BASS, 40, 110, 0, 20u * 44100u);
            vt_ev(V_CHORD, 60, 90, 0, 20u * 44100u);
            vt_ev(V_CHORD, 64, 90, 0, 20u * 44100u);
            for (bar = 2; bar < 5; bar++) /* from half-way through bar 2 (the notes settle) to the drop's first bar */
                for (st = bar == 2 ? 8 : 0; st < 16; st++) {
                    uint32_t at = (uint32_t)((bar - 2) * 16 + st - 8) * ss;
                    sb_moves(&p, bar, st, pass);
                    sb_render(at, ss);
                }
            for (st = 0; st < 4; st++)
                lo[pass][st] = bq_energy(sb_l, 0, 200.0f, 8u * ss + (uint32_t)st * 4u * ss, 4u * ss);
            rec[pass] = bq_energy(sb_l, 0, 200.0f, 24u * ss + 441u, 441u);
        }
        printf("moves: high-pass build, below 200 Hz per beat %.1f %.1f %.1f %.1f dB (vs no build), 10-20 ms after the "
               "downbeat %.2f dB\n", (double)db_of(lo[1][0] / lo[0][0]), (double)db_of(lo[1][1] / lo[0][1]),
               (double)db_of(lo[1][2] / lo[0][2]), (double)db_of(lo[1][3] / lo[0][3]), (double)db_of(rec[1] / rec[0]));
        for (st = 1, ok = 1; st < 4; st++)
            if (!(lo[1][st] < lo[1][st - 1]))
                ok = 0;
        CHECK(ok && db_of(lo[1][3] / lo[0][3]) < -6.0f, "moves: the build does not take the lows down beat by beat");
        CHECK(db_of(rec[1] / rec[0]) > -1.0f, "moves: the lows %.2f dB down 10 ms after the downbeat",
              (double)db_of(rec[1] / rec[0]));
    }
    (void)bad;
}

/* the drum bus compressor and the per-song pump */
static void test_dynamics(void)
{
    float pk[2], body[2], gpk[2];
    int k, i, bad = 0;
    for (k = 0; k < 2; k++) { /* k 1: the compressor off */
        double e = 0.0;
        vx_fx_off(k ? VX_OFF_COMP : 0u);
        vx_init();
        vt_ev(V_SNARE, 0, 127, 0, 0);
        sb_render(0, 4410);
        pk[k] = 0.0f;
        for (i = 0; i < 441; i++)
            pk[k] = fm_maxf(pk[k], fm_fabsf(sb_l[i]));
        for (i = 882; i < 2646; i++)
            e += (double)sb_l[i] * (double)sb_l[i];
        body[k] = fm_sqrtf((float)(e / 1764.0));
        vx_init();
        vt_ev(V_SNARE, 0, 30, 0, 0);
        sb_render(0, 4410);
        gpk[k] = 0.0f;
        for (i = 0; i < 441; i++)
            gpk[k] = fm_maxf(gpk[k], fm_fabsf(sb_l[i]));
    }
    vx_fx_off(0u);
    printf("dynamics: snare peak/body %.2f compressed vs %.2f open (peak %.1f dB); ghost %.2f dB\n",
           (double)(pk[0] / body[0]), (double)(pk[1] / body[1]), (double)db_of((double)(pk[0] * pk[0]) / (double)(pk[1] * pk[1])),
           (double)db_of((double)(gpk[0] * gpk[0]) / (double)(gpk[1] * gpk[1])));
    CHECK(pk[0] / body[0] < pk[1] / body[1] * 0.95f, "compressor: peak/body %.2f, open %.2f", (double)(pk[0] / body[0]),
          (double)(pk[1] / body[1]));
    CHECK(fm_fabsf(db_of((double)(gpk[0] * gpk[0]) / (double)(gpk[1] * gpk[1]))) < 1.0f, "compressor: a ghost note moved %.2f dB",
          (double)db_of((double)(gpk[0] * gpk[0]) / (double)(gpk[1] * gpk[1])));

    /* the pump: a third each over 300 rolls; a held pad under kicks dips by the depth (1, 3, 6 dB) */
    {
        int n[VX_PUMP_N] = {0, 0, 0}, s;
        static const float want[VX_PUMP_N] = {1.0f, 3.0f, 6.0f};
        for (s = 0; s < 300; s++) {
            song_roll_t roll;
            roll_song(NULL, 5000u + (uint32_t)s, &roll);
            vx_init();
            vx_configure(&roll);
            if (vx_config()->pump < VX_PUMP_N)
                n[vx_config()->pump]++;
        }
        printf("dynamics: pumps over 300 rolls %d %d %d\n", n[0], n[1], n[2]);
        for (k = 0; k < VX_PUMP_N; k++)
            CHECK(n[k] >= 75 && n[k] <= 123, "pump %d: %d of 300 rolls, wanted 33%% +- 8%%", k, n[k]);
        for (k = 0; k < VX_PUMP_N; k++) {
            float dip = 0.0f;
            int run, w;
            for (run = 0; run < 3; run++) { /* 0: pad and kicks, 1: kicks, 2: pad */
                vx_init();
                vx_set_pump(k);
                vx_assign(V_CHORD, INS_PAD);
                if (run != 1)
                    for (i = 0; i < 3; i++)
                        vt_ev(V_CHORD, 57 + 4 * i, 100, 0, 4u * 44100u);
                if (run != 2)
                    vt_ev(V_KICK, 0, 127, 2u * 44100u, 0);
                sb_render(0, 3u * 44100u);
                if (run == 0)
                    memcpy(sb_x, sb_l, 3u * 44100u * sizeof(float));
                else if (run == 1)
                    for (i = 0; i < (int)(3u * 44100u); i++)
                        sb_x[i] -= sb_l[i];
            }
            /* sb_x: the pad as ducked; sb_l: the pad alone. The deepest 5 ms window's level, 0-200 ms after the kick */
            for (w = 0; w < 40; w++) {
                double a = 0.0, b = 0.0;
                uint32_t from = 2u * 44100u + (uint32_t)w * 220u;
                for (i = 0; i < 220; i++) {
                    a += (double)sb_x[from + (uint32_t)i] * (double)sb_x[from + (uint32_t)i];
                    b += (double)sb_l[from + (uint32_t)i] * (double)sb_l[from + (uint32_t)i];
                }
                dip = fm_minf(dip, db_of(a / b));
            }
            printf("dynamics: pump %d dips the pad %.2f dB\n", k, (double)dip);
            CHECK(fm_fabsf(-dip - want[k]) <= 1.0f, "pump %d: the pad dips %.2f dB, wanted %.0f +- 1", k, (double)dip,
                  (double)want[k]);
        }
    }
    (void)bad;
}

/* the chorus on the chords' bus and the tape delay (fx.c) */
static void test_wet(void)
{
    int i, k, b, bad = 0;
    static const int keys[] = {INS_RHODES, INS_WURLI, INS_PAD, INS_FELT_PIANO, INS_NYLON};
    static const int leads[] = {INS_FLUTE, INS_MUTED_TPT, INS_SAX, INS_VIBES, INS_KALIMBA, INS_GUITAR_LICK,
                                INS_MUSIC_BOX, INS_BELLS, INS_VOX_PAD};
    /* the patches' amounts */
    for (k = 0; k < 5; k++) {
        const vx_patch_t *p = vx_patch(keys[k]);
        CHECK(k < 3 ? p->chorus >= 0.5f : p->chorus > 0.0f && p->chorus <= 0.25f, "patch %d: chorus %.2f", keys[k],
              (double)p->chorus);
        CHECK(p->delay > 0.0f && p->delay <= 0.15f, "patch %d (keys): delay send %.2f, wanted a little", keys[k],
              (double)p->delay);
    }
    for (k = 0; k < 9; k++)
        CHECK(vx_patch(leads[k])->delay >= 0.15f, "patch %d (lead, candy): delay send %.2f", leads[k],
              (double)vx_patch(leads[k])->delay);
    CHECK(vx_patch(INS_SUB)->delay == 0.0f && vx_patch(INS_ELEC_BASS)->delay == 0.0f &&
              vx_patch(INS_UPRIGHT)->delay == 0.0f, "the basses send to the delay");

    /* a held Rhodes chord (rendered dry) through the chorus: L and R apart; at chorus 0, L = R */
    {
        float corr[2], dmax = 0.0f;
        vx_init();
        for (i = 0; i < 4; i++)
            vt_ev(V_CHORD, 55 + 4 * i - (i == 3), 100, 0, 4u * 44100u);
        sb_render(0, 2u * 44100u);
        memcpy(sb_x, sb_l, 2u * 44100u * sizeof(float));
        for (k = 0; k < 2; k++) {
            double ll = 0.0, rr = 0.0, lr = 0.0;
            fx_fresh(0, 0, 0, FL_LOFI);
            fx_chorus(k ? 0.0f : vx_patch(INS_RHODES)->chorus);
            for (b = 0; b < 2 * 44100 / 2205; b++) {
                float ch[2205];
                memcpy(fx_l, sb_x + b * 2205, 2205 * sizeof(float));
                memcpy(ch, fx_l, sizeof ch);
                memset(fx_r, 0, 2205 * sizeof(float));
                fx_process_buses(fx_l, fx_r, ch, NULL, 2205, 0.0f);
                for (i = 0; i < 2205; i++) {
                    if (b >= 10) {
                        ll += (double)fx_l[i] * (double)fx_l[i];
                        rr += (double)fx_r[i] * (double)fx_r[i];
                        lr += (double)fx_l[i] * (double)fx_r[i];
                    }
                    if (k)
                        dmax = fm_maxf(dmax, fm_fabsf(fx_l[i] - fx_r[i]));
                }
            }
            corr[k] = (float)(lr / (double)fm_sqrtf((float)(ll * rr)));
        }
        printf("wet: Rhodes chord L/R correlation %.3f chorused, %.3f at chorus 0 (max |L-R| %.2e)\n", (double)corr[0],
               (double)corr[1], (double)dmax);
        CHECK(corr[0] < 0.95f, "chorus: L/R correlation %.3f, wanted < 0.95", (double)corr[0]);
        CHECK(dmax == 0.0f, "chorus 0: L and R differ by %.2e", (double)dmax);
    }
    /* the chorus's pitch wobble: a 440 Hz sine into the chorus alone, its period by zero crossings */
    {
        static float fr[4000];
        int nf = 0, cross = 0;
        float ph = 0.0f, last = 0.0f, prev = 0.0f, lo = 1e9f, hi = 0.0f, mean = 0.0f;
        uint32_t t = 0;
        fx_fresh(0, 0, 0, FL_LOFI);
        fx_chorus(vx_patch(INS_RHODES)->chorus);
        for (b = 0; b < 8 * 44100 / 2205; b++) {
            float ch[2205];
            for (i = 0; i < 2205; i++) {
                ch[i] = 0.2f * fm_sin_turns(ph);
                ph += 440.0f / 44100.0f;
                ph -= ph >= 1.0f ? 1.0f : 0.0f;
            }
            memset(fx_l, 0, 2205 * sizeof(float));
            memset(fx_r, 0, 2205 * sizeof(float));
            fx_process_buses(fx_l, fx_r, ch, NULL, 2205, 0.0f);
            for (i = 0; i < 2205; i++, t++) {
                float x = fx_l[i];
                if (t > 44100u && prev < 0.0f && x >= 0.0f) { /* a rising crossing, interpolated */
                    float at = (float)t - x / (x - prev);
                    if (last > 0.0f && nf < 4000) {
                        fr[nf] = 44100.0f * 100.0f / (at - last); /* over 100 periods: smooth */
                    }
                    if (++cross % 100 == 0) {
                        if (last > 0.0f && nf < 4000)
                            nf++;
                        last = at;
                    }
                }
                prev = x;
            }
        }
        for (i = 0; i < nf; i++) {
            lo = fm_minf(lo, fr[i]);
            hi = fm_maxf(hi, fr[i]);
            mean += fr[i] / (float)nf;
        }
        for (i = 1, cross = 0; i < nf; i++)
            cross += (fr[i - 1] < mean) != (fr[i] < mean);
        {
            float cents = 1731.234f * fm_logf(hi / lo) * 0.5f, secs = (float)nf * 100.0f / 440.0f,
                  hz = (float)cross * 0.5f / secs;
            printf("wet: chorus wobble +-%.1f cents at %.2f Hz (%d frames)\n", (double)cents, (double)hz, nf);
            CHECK(cents >= 2.0f && cents <= 15.0f && hz >= 0.2f && hz <= 2.0f,
                  "chorus: wobble +-%.1f cents at %.2f Hz, wanted 2-15 cents at 0.2-2 Hz", (double)cents, (double)hz);
        }
    }

    /* the tape delay: an impulse on the send echoes every dotted 8th of the song's tempo, each repeat quieter, under
       -60 dB by 3 s; a send of 0 gives no echo; the tail survives a song change */
    {
        static const int tempos[] = {70, 80, 90};
        vx_fx_off(VX_OFF_GLUE); /* (the delay's own levels: the mix's make-up left out) */
        for (k = 0; k < 3; k++) {
            song_roll_t roll;
            float T = 0.75f * 60.0f / (float)tempos[k] * 44100.0f, prevpk = 1e9f, late = 0.0f, worst = 0.0f;
            int e, mono = 1;
            roll_song(NULL, 7, &roll);
            roll.v[RF_TEMPO] = (int16_t)tempos[k];
            roll.v[RF_DUST] = roll.v[RF_NOISE] = roll.v[RF_AMBIENCE] = 0;
            vx_init();
            vx_configure(&roll);
            for (b = 0; b < (int)(SBUF_N / 2205u); b++) {
                float dl[2205];
                memset(dl, 0, sizeof dl);
                if (b == 0)
                    dl[0] = 1.0f;
                memset(fx_l, 0, 2205 * sizeof(float));
                memset(fx_r, 0, 2205 * sizeof(float));
                fx_process_buses(fx_l, fx_r, NULL, dl, 2205, 0.0f);
                memcpy(sb_l + b * 2205, fx_l, 2205 * sizeof(float));
            }
            for (e = 1; (float)e * T < 3.0f * 44100.0f; e++) {
                uint32_t c = (uint32_t)((float)e * T), at = c;
                float pk = 0.0f;
                for (i = (int)c - 441; i < (int)c + 441; i++)
                    if (fm_fabsf(sb_l[i]) > pk) {
                        pk = fm_fabsf(sb_l[i]);
                        at = (uint32_t)i;
                    }
                worst = fm_maxf(worst, fm_fabsf((float)at - (float)e * T) / 44.1f);
                if (!(pk < prevpk))
                    mono = 0;
                prevpk = pk;
                if (e == 1)
                    CHECK(pk > 0.01f, "delay %d BPM: no first echo (%.2e)", tempos[k], (double)pk);
                if (e == 1)
                    printf("wet: %d BPM echo 1 at %.2f ms (want %.2f), %.1f dB\n", tempos[k], (double)((float)at / 44.1f),
                           (double)(T / 44.1f), 20.0 * (double)fm_log2f(pk) * 0.30103);
            }
            for (i = 3 * 44100; i < (int)SBUF_N; i++)
                late = fm_maxf(late, fm_fabsf(sb_l[i]));
            printf("wet: %d BPM: echoes within %.2f ms of the dotted 8th, after 3 s %.1f dB\n", tempos[k],
                   (double)worst, late > 0.0f ? 20.0 * (double)fm_log2f(late) * 0.30103 : -999.0);
            CHECK(worst <= 1.0f, "delay %d BPM: an echo %.2f ms off the dotted 8th", tempos[k], (double)worst);
            CHECK(mono, "delay %d BPM: a repeat not quieter than the last", tempos[k]);
            CHECK(late < 1e-3f, "delay %d BPM: %.2e after 3 s, not below -60 dB", tempos[k], (double)late);
        }
        vx_fx_off(0u);
        /* a send of 0: the music's impulse makes no echo */
        {
            song_roll_t roll;
            float m = 0.0f;
            roll_song(NULL, 7, &roll);
            roll.v[RF_TEMPO] = 80;
            roll.v[RF_DUST] = roll.v[RF_NOISE] = roll.v[RF_AMBIENCE] = 0;
            vx_init();
            vx_configure(&roll);
            for (b = 0; b < 40; b++) {
                float dl[2205];
                memset(dl, 0, sizeof dl);
                memset(fx_l, 0, 2205 * sizeof(float));
                memset(fx_r, 0, 2205 * sizeof(float));
                if (b == 0)
                    fx_l[0] = 1.0f;
                fx_process_buses(fx_l, fx_r, NULL, dl, 2205, 0.0f);
                if (b >= 2)
                    for (i = 0; i < 2205; i++)
                        m = fm_maxf(m, fm_fabsf(fx_l[i]) + fm_fabsf(fx_r[i]));
            }
            CHECK(m == 0.0f, "delay: a send of 0 echoes (%.2e)", (double)m);
        }
        /* the tail survives a song change: a lead note, the song ends, the next one starts; the echoes ring on (the
           output against the same run with the delay off) */
        {
            float d = 0.0f;
            int pass;
            for (pass = 0; pass < 2; pass++) {
                song_roll_t roll;
                vx_fx_off(pass ? VX_OFF_DELAY : 0u);
                roll_song(NULL, 7, &roll);
                roll.v[RF_TEMPO] = 80;
                roll.v[RF_SET] = 1; /* flute lead */
                roll.v[RF_DUST] = roll.v[RF_NOISE] = roll.v[RF_AMBIENCE] = 0;
                vx_init();
                vx_song(&roll);
                vt_ev(V_LEAD, 72, 120, 0, 4410);
                sb_render(0, 8820);
                vx_release_all();
                sb_render(0, 8820);
                roll_song(NULL, 8, &roll);
                roll.v[RF_DUST] = roll.v[RF_NOISE] = roll.v[RF_AMBIENCE] = 0;
                vx_song(&roll);
                sb_render(0, 44100);
                if (!pass)
                    memcpy(sb_x, sb_l, 44100 * sizeof(float));
            }
            vx_fx_off(0u);
            for (i = 0; i < 44100; i++)
                d = fm_maxf(d, fm_fabsf(sb_x[i] - sb_l[i]));
            printf("wet: the delay's tail after a song change peaks %.2e\n", (double)d);
            CHECK(d > 1e-3f, "delay: no tail after the song change (%.2e)", (double)d);
        }
    }

    /* safety: 60 s of random input through the chorus and the delay, then 10 s of silence: no NaN, no runaway, no
       denormal state */
    {
        float ch[2205], dl[2205];
        fx_fresh(3, 0, 0, FL_CHILLWAVE);
        fx_chorus(1.0f);
        fx_tempo(70.0f);
        fx_seed = 777u;
        for (b = 0; b < 60 * 44100 / 2205; b++) {
            for (i = 0; i < 2205; i++) {
                fx_l[i] = fx_white();
                fx_r[i] = fx_white();
                ch[i] = fx_white();
                dl[i] = fx_white();
            }
            fx_process_buses(fx_l, fx_r, ch, dl, 2205, 0.5f + 0.5f * fx_white());
            for (i = 0; i < 2205; i++)
                if (fx_l[i] != fx_l[i] || fx_r[i] != fx_r[i] || fm_fabsf(fx_l[i]) > 10.0f || fm_fabsf(fx_r[i]) > 10.0f)
                    bad++;
        }
        memset(ch, 0, sizeof ch);
        memset(dl, 0, sizeof dl);
        for (b = 0; b < 10 * 44100 / 2205; b++) {
            memset(fx_l, 0, 2205 * sizeof(float));
            memset(fx_r, 0, 2205 * sizeof(float));
            fx_process_buses(fx_l, fx_r, ch, dl, 2205, 0.0f);
            for (i = 0; i < 2205; i++)
                if ((fx_l[i] != 0.0f && fm_fabsf(fx_l[i]) < 1.2e-38f) || (fx_r[i] != 0.0f && fm_fabsf(fx_r[i]) < 1.2e-38f))
                    bad++;
        }
        CHECK(bad == 0 && fx_tiny() == 0, "wet: %d bad samples, %d tiny states after random input and silence", bad,
              fx_tiny());
    }
}

/* ------------------------- Task 7c: half-rate keys and pads, the dust's fast path, a quarter-rate plate --- */
#define CM_N 32768u
static double cm_re[CM_N], cm_im[CM_N], cm_pa[CM_N / 2 + 1], cm_pb[CM_N / 2 + 1];
static float cm_x[2u * 44100u], cm_y[2u * 44100u];

/* an in-place radix-2 FFT of cm_re/cm_im (n a power of two, at most CM_N) */
static void cm_fft(uint32_t n)
{
    uint32_t i, j, len, k;
    for (i = 1, j = 0; i < n; i++) {
        uint32_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            double t = cm_re[i];
            cm_re[i] = cm_re[j];
            cm_re[j] = t;
            t = cm_im[i];
            cm_im[i] = cm_im[j];
            cm_im[j] = t;
        }
    }
    for (len = 2; len <= n; len <<= 1)
        for (k = 0; k < len / 2; k++) {
            double wr = (double)fm_cosf(FM_TWO_PI * (float)k / (float)len);
            double wi = -(double)fm_sinf(FM_TWO_PI * (float)k / (float)len);
            for (i = k; i < n; i += len) {
                uint32_t q = i + len / 2;
                double xr = cm_re[q] * wr - cm_im[q] * wi, xi = cm_re[q] * wi + cm_im[q] * wr;
                cm_re[q] = cm_re[i] - xr;
                cm_im[q] = cm_im[i] - xi;
                cm_re[i] += xr;
                cm_im[i] += xi;
            }
        }
}

/* the power spectrum of x[0, n) under a Hann window into pw[0 .. n/2] (n a power of two, at most CM_N); with add,
   added to what pw holds (a stereo pair's two channels) */
static void cm_spec(const float *x, uint32_t n, double *pw, int add)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        cm_re[i] = (double)x[i] * (double)(0.5f - 0.5f * fm_cosf(FM_TWO_PI * (float)i / (float)n));
        cm_im[i] = 0.0;
    }
    cm_fft(n);
    for (i = 0; i <= n / 2; i++)
        pw[i] = (add ? pw[i] : 0.0) + cm_re[i] * cm_re[i] + cm_im[i] * cm_im[i];
}

/* the energy in [lo, hi) Hz of a spectrum of n points */
static double cm_band(const double *pw, uint32_t n, float lo, float hi)
{
    double e = 0.0;
    uint32_t k;
    for (k = 1; k <= n / 2; k++) {
        float f = (float)k * 44100.0f / (float)n;
        if (f >= lo && f < hi)
            e += pw[k];
    }
    return e;
}

/* the largest bin of a spectrum of n points off every partial k * fb[j] (+-8 bins) from 20 Hz to 20 kHz, in dB below
   its largest bin: what is neither the note's partials nor their leakage (aliasing and imaging products) */
static float cm_alias(const double *pw, uint32_t n, const float *fb, int nb)
{
    double top = 0.0, off = 1e-30;
    uint32_t k;
    for (k = 1; k <= n / 2; k++)
        top = pw[k] > top ? pw[k] : top;
    for (k = 1; k <= n / 2; k++) {
        float f = (float)k * 44100.0f / (float)n, bw = 8.0f * 44100.0f / (float)n;
        int j, on = 0;
        if (f < 20.0f || f > 20000.0f)
            continue;
        for (j = 0; j < nb && !on; j++) {
            float h = (float)(int)(f / fb[j] + 0.5f) * fb[j];
            on = h > 0.0f && fm_fabsf(f - h) <= bw;
        }
        if (!on && pw[k] > off)
            off = pw[k];
    }
    return db_of(off / top);
}

/* one note on a role from a dry engine (full: the keys and pads at the full rate), n samples of the left channel */
static void cm_note(int role, int ins, int note, int vel, int full, float *out, uint32_t n)
{
    uint32_t d = 0;
    vx_fx_off(full ? VX_OFF_HALF : 0u);
    vx_init();
    vx_assign(role, ins);
    vt_ev(role, note, vel, 0, n);
    while (d < n) {
        uint32_t b = n - d < 4096u ? n - d : 4096u;
        memset(vt_l, 0, b * sizeof(float));
        memset(vt_r, 0, b * sizeof(float));
        vx_render(vt_l, vt_r, b);
        memcpy(out + d, vt_l, b * sizeof(float));
        d += b;
    }
    vx_fx_off(0u);
}

/* the third-rate plate's tail (0df8ddc, before Task 7c): an impulse's energy per octave band, 94 Hz to 3 kHz, over
   0.74 s from 0.1 s, both channels (dB) */
static const float PLATE_REF[5] = {13.81f, 15.22f, 17.79f, 19.79f, 19.23f};

static void test_cost_moves(void)
{
    static const int CH[7] = {INS_RHODES, INS_WURLI, INS_FELT_PIANO, INS_NYLON, INS_PAD, INS_VOX_PAD, INS_ORGAN};
    static const int NOTES[4] = {48, 60, 72, 84}, VELS[2] = {64, 110};
    float worst_band = 0.0f, worst_alias = -999.0f;
    int c, j, v, b;

    /* half-rate keys, pads and the organ: every chords-role patch, notes to MIDI 84, two velocities. Per
       octave band below 8 kHz the half-rate render's energy is within 1 dB of the full rate's; no aliasing or imaging
       product reaches -50 dB (or, where the full-rate render already has one there, it grows by under 1 dB) */
    for (c = 0; c < 7; c++)
        for (j = 0; j < 4; j++)
            for (v = 0; v < 2; v++) {
                const vx_patch_t *p = vx_patch(CH[c]);
                float f0 = 440.0f * fm_exp2f(((float)NOTES[j] - 69.0f) * (1.0f / 12.0f)), fb[2], ah, af, top = 0.0f;
                int nb = 1, pad = p->model == M_PAD;
                uint32_t w0 = pad ? 44100u : 0u, a0 = pad ? 44100u : 4410u;
                double eb[2][7];
                fb[0] = f0;
                if (pad) { /* the two saws: detuned apart, the second at its ratio */
                    float dt = fm_exp2f(p->p[0] * (1.0f / 1200.0f));
                    fb[0] = f0 / dt;
                    fb[1] = f0 * dt * p->p[1];
                    nb = 2;
                }
                if (p->model == M_SUB && p->p[2] > 0.0f)
                    fb[0] = 0.5f * f0; /* the sub-octave's partials */
                cm_note(V_CHORD, CH[c], NOTES[j], VELS[v], 1, cm_x, 2u * 44100u);
                cm_note(V_CHORD, CH[c], NOTES[j], VELS[v], 0, cm_y, 2u * 44100u);
                cm_spec(cm_x + w0, CM_N, cm_pa, 0);
                cm_spec(cm_y + w0, CM_N, cm_pb, 0);
                for (b = 0; b < 7; b++) {
                    float lo = 62.5f * (float)(1 << b);
                    eb[0][b] = cm_band(cm_pa, CM_N, lo, 2.0f * lo);
                    eb[1][b] = cm_band(cm_pb, CM_N, lo, 2.0f * lo);
                    top = fm_maxf(top, (float)eb[0][b]);
                }
                for (b = 0; b < 7; b++) {
                    float d = db_of(eb[1][b] / eb[0][b]);
                    if (db_of(eb[0][b] / (double)top) < -60.0f)
                        continue; /* a band with nothing in it */
                    worst_band = fm_maxf(worst_band, fm_fabsf(d));
                    CHECK(fm_fabsf(d) <= 1.0f, "half rate: ins %d note %d vel %d: band %.0f-%.0f Hz off by %.2f dB",
                          CH[c], NOTES[j], VELS[v], (double)(62.5f * (float)(1 << b)),
                          (double)(125.0f * (float)(1 << b)), (double)d);
                }
                cm_spec(cm_x + a0, CM_N / 2u, cm_pa, 0);
                cm_spec(cm_y + a0, CM_N / 2u, cm_pb, 0);
                af = cm_alias(cm_pa, CM_N / 2u, fb, nb);
                ah = cm_alias(cm_pb, CM_N / 2u, fb, nb);
                worst_alias = fm_maxf(worst_alias, ah);
                if (ah > -50.0f || af > -50.0f)
                    printf("half rate: ins %2d note %d vel %3d: off-partial peak %.1f dB (full rate %.1f dB)\n", CH[c],
                           NOTES[j], VELS[v], (double)ah, (double)af);
                CHECK(ah <= -50.0f || ah <= af + 1.0f,
                      "half rate: ins %d note %d vel %d: an aliasing product at %.1f dB (full rate %.1f dB)", CH[c],
                      NOTES[j], VELS[v], (double)ah, (double)af);
            }
    printf("half rate: worst band difference %.2f dB, worst off-partial peak %.1f dB\n", (double)worst_band,
           (double)worst_alias);

    /* the Wurli's bark (the only noise in a half-rate voice; the pads have none): its level over the first 40 ms, as
       the difference of two renders whose noise differs (a silent guitar-lick pluck first draws from the noise; eight
       such plucks of different lengths, so the estimate does not hang on one draw), is within 0.75 dB of the full
       rate's, over three notes and two velocities. (One draw read 0.42 dB, then 1.34 once the guitar lick's longer
       loop shifted the draws; averaged, the half rate's bark sits about 0.53 dB off with either guitar.) */
    {
        static const int BN[3] = {48, 60, 67}, BV[2] = {100, 127};
        double eb[2] = {0.0, 0.0};
        int full, nn, vv, pass;
        for (full = 0; full < 2; full++)
            for (nn = 0; nn < 3; nn++)
                for (vv = 0; vv < 2; vv++) {
                    for (pass = 0; pass < 9; pass++) { /* (pass k > 0: a pluck of note 52 + 4k first) */
                        vx_fx_off(full ? VX_OFF_HALF : 0u);
                        vx_init();
                        vx_assign(V_CHORD, INS_WURLI);
                        vx_assign(V_LEAD, INS_GUITAR_LICK);
                        if (pass)
                            vt_ev(V_LEAD, 52 + 4 * pass, 0, 0, 100);
                        vt_ev(V_CHORD, BN[nn], BV[vv], 0, 8820);
                        memset(vt_l, 0, 1764 * sizeof(float));
                        memset(vt_r, 0, 1764 * sizeof(float));
                        vx_render(vt_l, vt_r, 1764);
                        memcpy(pass ? cm_y : cm_x, vt_l, 1764 * sizeof(float));
                        if (pass)
                            for (b = 0; b < 1764; b++)
                                eb[full] += (double)(cm_x[b] - cm_y[b]) * (double)(cm_x[b] - cm_y[b]);
                    }
                }
        vx_fx_off(0u);
        printf("half rate: the Wurli's bark %.2f dB against the full rate's\n", (double)db_of(eb[0] / eb[1]));
        CHECK(eb[1] > 0.0 && fm_fabsf(db_of(eb[0] / eb[1])) <= 0.75f,
              "half rate: the Wurli's bark %.2f dB from the full rate's", (double)db_of(eb[0] / eb[1]));
    }

    /* the dust's fast path: at dust 0 and 1 (both flavors) the chain's output matches the full path within -80 dB
       (error RMS and peak against the music's), and exactly over silence; and through the wow coming on and going
       off under steady music (dust 2 in DL: 0 ramping to 1 from 0.05 s; 3: 1 ramping to 0, then back to 1 at 1.7 s) */
    {
        static const int DL[4] = {0, 1, 2, 3}, FLV[2] = {FL_LOFI, FL_CHILLWAVE};
        int d, fl, k;
        for (d = 0; d < 4; d++)
            for (fl = 0; fl < 2; fl++) {
                double e = 0.0, s = 0.0;
                float pk = 0.0f, ep = 0.0f, quiet = 0.0f;
                uint32_t i, t;
                for (k = 0; k < 2; k++) {
                    fx_fast(k == 0);
                    fx_fresh(DL[d] < 2 ? DL[d] : DL[d] - 2, 0, 0, FLV[fl]);
                    fx_seed = 31u;
                    for (t = 0; t < 2u * 44100u; t += 256u) {
                        float l[256], r[256], duck = (t / 11025u) & 1u ? 0.9f : 0.0f;
                        if (DL[d] >= 2 && t == 2048u)
                            fx_configure(DL[d] == 2 ? 1 : 0, 0, 0, FLV[fl]);
                        if (DL[d] == 3 && t == 74752u)
                            fx_configure(1, 0, 0, FLV[fl]);
                        for (i = 0; i < 256; i++) { /* a swell to 0.6, a quiet bar, then silence; steady music in the
                                                       transitions */
                            float u = (float)(t + i) * (1.0f / 44100.0f);
                            float a = DL[d] >= 2 ? 0.3f : u < 1.2f ? 0.5f * u : u < 1.6f ? 0.005f : 0.0f;
                            l[i] = a * (0.6f * fm_sinf(FM_TWO_PI * 110.0f * u) + 0.3f * fm_sinf(FM_TWO_PI * 443.0f * u) +
                                        0.1f * fx_white());
                            r[i] = 0.3f * l[i];
                        }
                        fx_process(l, r, 256, duck);
                        for (i = 0; i < 256; i++)
                            (k == 0 ? cm_x : cm_y)[t + i] = l[i] + r[i];
                    }
                }
                fx_fast(1);
                for (i = 0; i < 2u * 44100u; i++) {
                    float x = cm_y[i], dd = cm_x[i] - cm_y[i];
                    e += (double)dd * (double)dd;
                    s += (double)x * (double)x;
                    pk = fm_maxf(pk, fm_fabsf(x));
                    ep = fm_maxf(ep, fm_fabsf(dd));
                    if (i >= 74000u && DL[d] < 2) /* from 1.68 s: silence in, only the plate's tail */
                        quiet = fm_maxf(quiet, fm_fabsf(dd));
                }
                printf("dust fast path: dust %d flavor %d: error RMS %.1f dB, peak %.1f dB (music peak %.3f), silence %.1e\n",
                       DL[d], FLV[fl], (double)db_of(e / s), 20.0 * (double)fm_log2f(fm_maxf(ep, 1e-30f) / pk) * 0.30103,
                       (double)pk, (double)quiet);
                CHECK(db_of(e / s) <= -80.0f && ep <= 1e-4f * pk,
                      "dust fast path: dust %d flavor %d: error RMS %.1f dB, peak %.2e against %.3f", DL[d], FLV[fl],
                      (double)db_of(e / s), (double)ep, (double)pk);
                CHECK(quiet <= 1e-4f * pk, "dust fast path: dust %d flavor %d: %.2e over silence", DL[d], FLV[fl],
                      (double)quiet);
            }
    }

    /* our plate's tail (fx_rings(0): host only, Rings' reverb is the default): per octave band below 3 kHz within
       1.5 dB of the third-rate plate (its decay under -60 dB in 4 s and the denormal checks are test_fx_chain's on
       Rings' reverb, and test_rings_pair's on both) */
    {
        uint32_t t;
        float d;
        fx_rings(0);
        fx_glue_off(1); /* (the reference has no make-up) */
        fx_fresh(0, 0, 0, FL_LOFI);
        for (t = 0; t < 2u * 44100u; t += 2205u) {
            memset(fx_l, 0, 2205 * sizeof(float));
            memset(fx_r, 0, 2205 * sizeof(float));
            if (t == 0)
                fx_r[0] = 1.0f;
            fx_process(fx_l, fx_r, 2205, 0.0f);
            memcpy(cm_x + t, fx_l, 2205 * sizeof(float));
            memcpy(cm_y + t, fx_r, 2205 * sizeof(float));
        }
        cm_spec(cm_x + 4410, CM_N, cm_pa, 0);
        cm_spec(cm_y + 4410, CM_N, cm_pa, 1);
        for (b = 0; b < 5; b++) {
            float lo = 93.75f * (float)(1 << b);
            d = db_of(cm_band(cm_pa, CM_N, lo, 2.0f * lo));
            printf("plate tail: %.0f-%.0f Hz %.2f dB (third-rate %.2f dB)\n", (double)lo, (double)(2.0f * lo),
                   (double)d, (double)PLATE_REF[b]);
            CHECK(fm_fabsf(d - PLATE_REF[b]) <= 1.5f, "plate tail: band %.0f-%.0f Hz at %.2f dB, the third-rate %.2f dB",
                  (double)lo, (double)(2.0f * lo), (double)d, (double)PLATE_REF[b]);
        }
        fx_glue_off(0);
        fx_rings(1);
    }
}

/* ------------------------- Synths Task 2: the subtractive voice (M_SUB) --- */
static const int SUB_INS[3] = {INS_ORGAN, INS_SAW_LEAD, INS_SYNTH_PLUCK};
static vx_patch_t sb_pt; /* a table patch's copy with a field changed (vx_assign_patch) */

/* vt_long[0, n) of the left output from the events queued (n a multiple of 4096, at most VT_LONG) */
static void sb_run(uint32_t n, int *bad)
{
    uint32_t k;
    for (k = 0; k < n; k += 4096u) {
        vt_block(4096, bad);
        memcpy(vt_long + k, vt_l, 4096 * sizeof(float));
    }
}

/* the period of vt_long[from .. from+w) between lo and hi samples: the normalized autocorrelation's peak,
   parabolically interpolated */
static float sb_period(uint32_t from, uint32_t w, int lo, int hi)
{
    float r[512], a, b, c, d;
    int lag, best = lo;
    for (lag = lo; lag <= hi; lag++) {
        float s = 0.0f, e0 = 0.0f, e1 = 0.0f;
        uint32_t i;
        for (i = from; i < from + w; i++) {
            s += vt_long[i] * vt_long[i + (uint32_t)lag];
            e0 += vt_long[i] * vt_long[i];
            e1 += vt_long[i + (uint32_t)lag] * vt_long[i + (uint32_t)lag];
        }
        r[lag - lo] = s * (e0 > 0.0f && e1 > 0.0f ? fm_rsqrtf(e0 * e1) : 0.0f);
        if (r[lag - lo] > r[best - lo])
            best = lag;
    }
    if (best == lo || best == hi)
        return (float)best;
    a = r[best - lo - 1];
    b = r[best - lo];
    c = r[best - lo + 1];
    d = a - 2.0f * b + c;
    return (float)best + (d != 0.0f ? 0.5f * (a - c) / d : 0.0f);
}

/* the spectral centroid (Hz) of vt_long[from, from + 1024), Hann-windowed: the bins' frequencies weighted by their
   magnitudes */
static float sb_centroid(uint32_t from)
{
    double s = 0.0, e = 0.0;
    uint32_t k;
    cm_spec(vt_long + from, 1024u, cm_pa, 0);
    for (k = 1; k <= 512u; k++) {
        double a = (double)fm_sqrtf((float)cm_pa[k]);
        s += (double)k * a;
        e += a;
    }
    return e > 0.0 ? (float)(s / e) * 44100.0f / 1024.0f : 0.0f;
}

static float sb_hz(int note) { return 440.0f * fm_exp2f(((float)note - 69.0f) * (1.0f / 12.0f)); }

/* A mono lead (pt) at n1 for 0.3 s, then n2 gap samples after its release: the pitch (cents from n2) in 512-sample
   windows every 64 samples from 2048 before n2's onset; *lo the lowest, *reach when it is within 10 cents for good
   (samples after the onset), *c10 the cents 10 ms in, *mono whether it never turned back */
static void sb_glide(const vx_patch_t *pt, int n1, int n2, uint32_t gap, float *lo, float *reach, float *c10, int *mono)
{
    static float ct[1024];
    float tg = pt->p[10] * 0.001f * 44100.0f;
    uint32_t on = 13230u + gap, t;
    int n = 0, j, bad = 0, lo_p = (int)(44100.0f / sb_hz(n2 > n1 ? n2 : n1) * 0.9f),
        hi_p = (int)(44100.0f / sb_hz(n2 > n1 ? n1 : n2) * 1.1f);
    sb_pt = *pt;
    vx_init();
    vx_assign_patch(V_LEAD, &sb_pt);
    vt_ev(V_LEAD, n1, 100, 0, 13230u);
    vt_ev(V_LEAD, n2, 100, on, 22050u);
    sb_run(12u * 4096u, &bad);
    CHECK(!bad, "sb_glide: the render went bad");
    *lo = 1e9f;
    *mono = 1;
    *reach = -1.0f;
    for (t = on - 2048u; t + 256u < on + (uint32_t)(3.0f * tg) && n < 1024; t += 64u, n++) {
        ct[n] = 1200.0f * fm_log2f(44100.0f / sb_hz(n2) / sb_period(t - 256u, 512u, lo_p, hi_p));
        if (t >= on) {
            *lo = fm_minf(*lo, ct[n]);
            if (ct[n] < ct[n - 1] - 2.0f)
                *mono = 0;
        }
    }
    for (j = n - 1; j >= 0 && fm_fabsf(ct[j]) <= 10.0f; j--)
        *reach = (float)(on - 2048u) + 64.0f * (float)j - (float)on;
    *c10 = 1200.0f * fm_log2f(44100.0f / sb_hz(n2) / sb_period(on + 441u, 512u, lo_p, hi_p));
}

/* The subtractive voice (M_SUB: the organ, the saw lead, the synth pluck). Its filter envelope sweeps (the synth
   pluck's spectral centroid at 20 ms at least twice its centroid at 400 ms); a mono lead glides from a held note
   (Review Focus 1: the saw lead's legato note on its target within the patch's glide time +-20%, monotonically; a
   note after a rest on pitch at once; a stolen voice gone within 5 ms), and from one ended 10 ms before a 3rd up (the
   ruling: within 25 ms and a 4th; a 5th, or a 40 ms rest, on pitch); a patch on the chords role renders at half the
   rate exactly when its highest cutoff with the envelope is 8 kHz or under (test_cost_moves holds the organ's half
   rate to the full rate's). */
static void test_sub(void)
{
    int bad = 0, i, j, v;
    for (i = 0; i < 3; i++)
        CHECK(vx_patch(SUB_INS[i])->model == M_SUB, "ins %d: not an M_SUB", SUB_INS[i]);
    CHECK(lib_ins_sustained(INS_ORGAN) && lib_ins_sustained(INS_SAW_LEAD) && !lib_ins_sustained(INS_SYNTH_PLUCK),
          "sub class: wanted the organ and the saw lead sustained, the synth pluck percussive");
    CHECK(vx_patch(INS_SAW_LEAD)->p[9] > 0.0f && vx_patch(INS_SAW_LEAD)->p[10] > 0.0f,
          "saw lead: wanted mono with a glide");
    CHECK(vx_patch(INS_ORGAN)->p[9] == 0.0f && vx_patch(INS_SYNTH_PLUCK)->p[9] == 0.0f,
          "the organ and the synth pluck: wanted poly");
    vx_fx_off(VX_OFF_DELAY);

    { /* the filter envelope's sweep: the synth pluck's centroid at 20 ms and at 400 ms (1024-sample windows) */
        float a, b;
        vt_note(V_LEAD, INS_SYNTH_PLUCK, 67, 110, 44100u, 5u * 4096u, &bad);
        a = sb_centroid(882u - 512u);
        b = sb_centroid(17640u - 512u);
        printf("sub: synth pluck centroid at 20 ms %.0f Hz, at 400 ms %.0f Hz (%.2fx)\n", (double)a, (double)b,
               (double)(a / b));
        CHECK(a >= 2.0f * b, "synth pluck: centroid %.0f Hz at 20 ms, %.0f at 400 ms: wanted a sweep (2x)", (double)a,
              (double)b);
    }
    { /* glide: the saw lead at 64, held; 69 arrives at 0.5 s while 64 still holds. Pitch (cents from 69) in 512-sample
         windows every 64 samples */
        static float ct[1024];
        const vx_patch_t *p = vx_patch(INS_SAW_LEAD);
        float tg = p->p[10] * 0.001f * 44100.0f, reach = -1.0f, lo = 1e9f;
        uint32_t on = 22050u, t;
        int n = 0, mono = 1, lo_p = (int)(44100.0f / sb_hz(69) * 0.9f), hi_p = (int)(44100.0f / sb_hz(64) * 1.1f);
        vx_init();
        vx_assign(V_LEAD, INS_SAW_LEAD);
        vt_ev(V_LEAD, 64, 100, 0, 44100u);
        vt_ev(V_LEAD, 69, 100, on, 22050u);
        sb_run(12u * 4096u, &bad);
        for (t = on - 2048u; t + 256u < on + (uint32_t)(3.0f * tg) && n < 1024; t += 64u, n++) {
            ct[n] = 1200.0f * fm_log2f(44100.0f / sb_hz(69) / sb_period(t - 256u, 512u, lo_p, hi_p));
            lo = fm_minf(lo, ct[n]);
            if (n && ct[n] < ct[n - 1] - 2.0f)
                mono = 0;
        }
        for (j = n - 1; j >= 0 && fm_fabsf(ct[j]) <= 10.0f; j--)
            reach = (float)(on - 2048u) + 64.0f * (float)j - (float)on;
        printf("sub: saw lead glide 64 to 69: from %.1f cents, on pitch %.1f ms after the note (glide %.0f ms), %s\n",
               (double)lo, (double)reach / 44.1, (double)p->p[10], mono ? "monotonic" : "not monotonic");
        CHECK(lo < -480.0f && lo > -520.0f, "glide: started at %.1f cents from the target, wanted -500", (double)lo);
        CHECK(reach >= 0.8f * tg && reach <= 1.2f * tg, "glide: on pitch %.1f ms after the note, wanted %.0f ms +-20%%",
              (double)reach / 44.1, (double)p->p[10]);
        CHECK(mono, "glide: the pitch turned back on the way");
    }
    { /* after a rest: 64 released at 0.3 s (its release lengthened to 0.3 s, so its tail still sounds, about 40 dB
         down), 69 at 0.5 s: on pitch in its first window (10 to 22 ms) */
        float c;
        int lo_p = (int)(44100.0f / sb_hz(69) * 0.9f), hi_p = (int)(44100.0f / sb_hz(64) * 1.1f), tail = -1;
        uint32_t k;
        sb_pt = *vx_patch(INS_SAW_LEAD);
        sb_pt.rel = 0.3f;
        vx_init();
        vx_assign_patch(V_LEAD, &sb_pt);
        vt_ev(V_LEAD, 64, 100, 0, 13230u);
        vt_ev(V_LEAD, 69, 100, 22050u, 22050u);
        for (k = 0; k < 8u * 4096u; k += 2048u) {
            vt_block(2048, &bad);
            memcpy(vt_long + k, vt_l, 2048 * sizeof(float));
            if (k + 2048u == 20480u) /* 30 ms before 69 */
                tail = vx_active();
        }
        c = 1200.0f * fm_log2f(44100.0f / sb_hz(69) / sb_period(22050u + 441u, 512u, lo_p, hi_p));
        printf("sub: saw lead after a rest (the last note's tail on: %d): %.1f cents from the note 10 ms in\n",
               tail == 1, (double)c);
        CHECK(tail == 1, "after a rest: %d voices sounding before the note, wanted the last note's tail", tail);
        CHECK(fm_fabsf(c) <= 5.0f, "after a rest: %.1f cents off the note 10 ms in, wanted no glide", (double)c);
    }
    { /* the ruling (2026-10-23): a mono lead's note within 25 ms of its last one's end and a 4th or less from it
         glides (the saw lead, still releasing; and with a 5 ms release, gone, its voice not reused); a 5th, or a
         40 ms gap, starts on pitch */
        const vx_patch_t *p = vx_patch(INS_SAW_LEAD);
        float tg = p->p[10] * 0.001f * 44100.0f, lo, reach, c10;
        int mono, k;
        vx_patch_t sh = *p;
        sh.rel = 0.005f;
        for (k = 0; k < 2; k++) {
            sb_glide(k ? &sh : p, 64, 67, 441u, &lo, &reach, &c10, &mono);
            printf("sub: saw lead (release %.0f ms) 64, 10 ms rest, 67: from %.1f cents, on pitch %.1f ms in, %s\n",
                   (double)(k ? sh.rel : p->rel) * 1000.0, (double)lo, (double)reach / 44.1,
                   mono ? "monotonic" : "not monotonic");
            CHECK(lo < -270.0f, "glide after a 10 ms rest (k %d): lowest %.1f cents, wanted from about -300", k,
                  (double)lo);
            CHECK(reach >= 0.8f * tg && reach <= 1.2f * tg,
                  "glide after a 10 ms rest (k %d): on pitch %.1f ms in, wanted %.0f ms +-20%%", k,
                  (double)reach / 44.1, (double)p->p[10]);
            CHECK(mono, "glide after a 10 ms rest (k %d): the pitch turned back", k);
        }
        sb_glide(&sh, 64, 71, 441u, &lo, &reach, &c10, &mono);
        printf("sub: saw lead 64, 10 ms rest, 71 (a 5th): %.1f cents 10 ms in\n", (double)c10);
        CHECK(fm_fabsf(c10) <= 5.0f, "a 5th after a 10 ms rest: %.1f cents 10 ms in, wanted on pitch", (double)c10);
        sb_glide(&sh, 64, 67, 1764u, &lo, &reach, &c10, &mono);
        printf("sub: saw lead 64, 40 ms rest, 67: %.1f cents 10 ms in\n", (double)c10);
        CHECK(fm_fabsf(c10) <= 5.0f, "a 3rd after a 40 ms rest: %.1f cents 10 ms in, wanted on pitch", (double)c10);
    }
    { /* a steal: the saw lead (release 2 s) at 57, released; 69 after a rest, released; 81 then takes 57's voice,
         which fades: 57's tone 5 to 25 ms after 81's start is 30 dB or more under its level before it */
        float a0, a1, c1;
        uint32_t on = 13230u;
        sb_pt = *vx_patch(INS_SAW_LEAD);
        sb_pt.rel = 2.0f;
        vx_init();
        vx_assign_patch(V_LEAD, &sb_pt);
        vt_ev(V_LEAD, 57, 100, 0, 4410u);
        vt_ev(V_LEAD, 69, 100, 6615u, 4410u);
        vt_ev(V_LEAD, 81, 100, on, 22050u);
        sb_run(8u * 4096u, &bad);
        a0 = vt_tone(on - 882u, 882u, sb_hz(57) / 44100.0f);
        a1 = vt_tone(on + 221u, 882u, sb_hz(57) / 44100.0f);
        c1 = vt_tone(on + 221u, 882u, sb_hz(81) / 44100.0f);
        printf("sub: steal: the victim's tone 5-25 ms after the thief %.1f dB of its level before; the thief at %.2f\n",
               20.0 * 0.30103 * (double)fm_log2f(a1 / a0 + 1e-9f), (double)(c1 / a0));
        CHECK(a1 < 0.0316f * a0, "steal: the victim at %.1f dB 5 ms after, wanted gone (-30 dB)",
              20.0 * 0.30103 * (double)fm_log2f(a1 / a0 + 1e-9f));
        CHECK(c1 > 0.1f * a0, "steal: the new note hardly sounds 5-25 ms in (%.3f)", (double)(c1 / a0));
    }
    { /* the half-rate rule on the chords role: a render differs from the full rate's exactly when the patch's highest
         cutoff (key tracked, the envelope at full) is 8 kHz or under, on a 2-pole */
        static const int NOTES[4] = {48, 60, 72, 84}, VELS[2] = {64, 110};
        for (i = 0; i < 3; i++)
            for (j = 0; j < 4; j++)
                for (v = 0; v < 2; v++) {
                    const vx_patch_t *p = vx_patch(SUB_INS[i]);
                    float top = (p->p[3] + p->p[5] * sb_hz(NOTES[j])) *
                                fm_exp2f(p->p[6] * (0.5f + 0.5f * (float)VELS[v] / 127.0f));
                    int want = top <= 8000.0f, half;
                    cm_note(V_CHORD, SUB_INS[i], NOTES[j], VELS[v], 1, cm_x, 8192u);
                    cm_note(V_CHORD, SUB_INS[i], NOTES[j], VELS[v], 0, cm_y, 8192u);
                    half = memcmp(cm_x, cm_y, 8192u * sizeof(float)) != 0;
                    CHECK(half == want, "half rate: ins %d note %d vel %d (top cutoff %.0f Hz): %s, wanted %s",
                          SUB_INS[i], NOTES[j], VELS[v], (double)top, half ? "half" : "full", want ? "half" : "full");
                    if (SUB_INS[i] == INS_ORGAN)
                        CHECK(want, "half rate: ins %d note %d vel %d: top cutoff %.0f Hz, wanted the half rate",
                              SUB_INS[i], NOTES[j], VELS[v], (double)top);
                }
    }
    CHECK(!bad, "a subtractive voice rendered NaN or inf");
    vx_fx_off(0u);
}

/* The ruling (2026-10-23): in rolled songs a mono synth glides. Over 3 songs of each set with a mono patch (the saw
   lead: 13, 18, 19, pinned with roll_set: the first seeds whose roll takes it), the engine's glides (vx_glides) are
   15% or more of the lead's note changes (a note after another at a new pitch), in every set; the bass never
   glides. */
static void test_glide_songs(void)
{
    static const int SETS[3] = {13, 18, 19};
    static song_t s;
    int k;
    for (k = 0; k < 3; k++) {
        const lib_set_t *set = lib_set_by_id((uint8_t)SETS[k]);
        uint32_t seed, ch[2] = {0, 0}, gl[2] = {0, 0}, sm[2] = {0, 0};
        int r, mono[2], got;
        mono[0] = vx_patch(set->lead)->model == M_SUB && vx_patch(set->lead)->p[9] > 0.0f;
        mono[1] = 0; /* (no mono synth on the bass) */
        CHECK(mono[0], "glides: set %d has no mono lead", SETS[k]);
        for (seed = 1u, got = 0; got < 3 && seed < 600u; seed++) {
            song_roll_t ro;
            song_ev_t ev[64];
            int last[2] = {-1, -1};
            float frac = 0.0f;
            roll_song(NULL, seed, &ro);
            if (roll_set(&ro, RF_SET, SETS[k]) != SETS[k])
                continue; /* (a seed whose roll won't take the set) */
            got++;
            ro.v[RF_NOISE] = ro.v[RF_AMBIENCE] = 0;
            vx_init();
            song_start(&s, &ro);
            vx_song(&s.roll);
            while (s.state == SS_PLAYING) {
                int n = song_tick(&s, ev, 64), i;
                float adv;
                uint32_t left;
                for (i = 0; i < n; i++) {
                    int j = ev[i].voice == V_LEAD ? 0 : ev[i].voice == V_BASS ? 1 : -1;
                    if (j >= 0 && ev[i].len) {
                        if (last[j] >= 0 && last[j] != ev[i].note)
                            ch[j]++;
                        last[j] = ev[i].note;
                    }
                    vx_event(&ev[i]);
                }
                adv = song_step_samples(&s) + frac;
                left = (uint32_t)adv;
                frac = adv - (float)left;
                while (left) {
                    uint32_t b = left < 1024u ? left : 1024u;
                    memset(vt_l, 0, b * sizeof(float));
                    memset(vt_r, 0, b * sizeof(float));
                    vx_render(vt_l, vt_r, b);
                    left -= b;
                }
            }
            gl[0] += vx_glides(V_LEAD);
            gl[1] += vx_glides(V_BASS);
        }
        CHECK(got == 3, "glides: %d seeds take set %d, wanted 3", got, SETS[k]);
        for (r = 0; r < 2; r++)
            sm[r] = mono[r] ? ch[r] : 0u;
        printf("glides: set %d (%s): lead %s %u of %u note changes, bass %s %u of %u; %.0f%%\n", SETS[k], set->name,
               mono[0] ? "mono" : "poly", gl[0], ch[0], mono[1] ? "mono" : "poly", gl[1], ch[1],
               100.0 * (double)(gl[0] + gl[1]) / (double)(sm[0] + sm[1] ? sm[0] + sm[1] : 1u));
        for (r = 0; r < 2; r++)
            if (mono[r])
                CHECK(gl[r] * 100u >= 15u * ch[r], "glides: set %d's %s glides %u of %u note changes, wanted 15%%+",
                      SETS[k], r ? "bass" : "lead", gl[r], ch[r]);
            else
                CHECK(gl[r] == 0u, "glides: set %d's poly %s glided %u times", SETS[k], r ? "bass" : "lead", gl[r]);
    }
}

/* The FM patches (new patches on M_EP and M_BASS). The DX electric piano's first 30 ms is brighter than the Rhodes'
   (spectral centroid) and it falls to half its level sooner; the FM bass's slap: its centroid at 10 ms at least 3x
   its centroid at 200 ms. */
static void test_fm_patches(void)
{
    int bad = 0, i;
    float c[2], h[2];
    static const int KE[2] = {INS_DX_EP, INS_RHODES};

    CHECK(vx_patch(INS_DX_EP)->model == M_EP && vx_patch(INS_FM_BASS)->model == M_BASS,
          "FM patches on the wrong models");
    CHECK(!lib_ins_sustained(INS_DX_EP) && !lib_ins_sustained(INS_FM_BASS),
          "the FM patches are struck, not sustained");
    vx_fx_off(VX_OFF_MUD | VX_OFF_DELAY);
    for (i = 0; i < 2; i++) { /* centroid over the first 30 ms (1024 samples is 23 ms, centred at 12 ms), 50% time */
        uint32_t n;
        float pk, e;
        vt_note(V_CHORD, KE[i], 60, 100, 3u * 44100u, VT_LONG, &bad);
        c[i] = sb_centroid(0);
        pk = vt_peak(0, 2205);
        h[i] = 3.0f;
        for (n = 2205; n + 220 < 3u * 44100u; n += 220) {
            e = vt_peak(n, n + 220);
            if (e < 0.5f * pk) { h[i] = (float)n / 44100.0f; break; }
        }
        printf("fm keys: ins %d centroid %.0f Hz, half level at %.2f s\n", KE[i], (double)c[i], (double)h[i]);
    }
    CHECK(c[0] > c[1], "DX EP centroid %.0f Hz over its first 30 ms, Rhodes %.0f: wanted brighter", (double)c[0],
          (double)c[1]);
    CHECK(h[0] < h[1], "DX EP half level at %.2f s, Rhodes %.2f s: wanted faster", (double)h[0], (double)h[1]);
    { /* the slap */
        float a, b;
        vt_note(V_BASS, INS_FM_BASS, 40, 110, 44100u, 5u * 4096u, &bad);
        a = sb_centroid(0u);
        b = sb_centroid(8820u - 512u);
        printf("fm bass: centroid at 10 ms %.0f Hz, at 200 ms %.0f Hz\n", (double)a, (double)b);
        CHECK(a >= 3.0f * b, "FM bass centroid %.0f Hz at 10 ms, %.0f at 200 ms: wanted 3x", (double)a, (double)b);
    }
    CHECK(!bad, "FM patches: NaN or blow-up");
    vx_fx_off(0);
}


/* Task 7c's head-to-head: Rings' reverb and chorus (the default since Sam's pick; ours is fx_rings(0), host only)
   through the plate's impulse, denormal and random-input tests; the reverb tuned to the plate (its -60 dB time within 10%, its tail's energy 0.1-1 s within
   1 dB); the chorus's dry scale. */
static float rp_t60(int rings, double *tail)
{
    static double edc[2u * 44100u];
    uint32_t t, i;
    double e = 0.0;
    int t5 = -1, t35 = -1;
    fx_rings(rings);
    fx_fresh(0, 0, 0, FL_LOFI);
    for (t = 0; t < 2u * 44100u; t += 2205u) {
        memset(fx_l, 0, 2205 * sizeof(float));
        memset(fx_r, 0, 2205 * sizeof(float));
        if (t == 0)
            fx_r[0] = 0.2f;
        fx_process(fx_l, fx_r, 2205, 0.0f);
        memcpy(cm_x + t, fx_l, 2205 * sizeof(float));
        memcpy(cm_y + t, fx_r, 2205 * sizeof(float));
    }
    *tail = 0.0;
    for (i = 2u * 44100u; i-- > 0;) {
        double x = (double)cm_x[i] * (double)cm_x[i] + (double)cm_y[i] * (double)cm_y[i];
        e += x;
        edc[i] = e;
        if (i >= 4410u && i < 44100u)
            *tail += x;
    }
    for (i = 0; i < 2u * 44100u; i++) {
        float db = db_of(edc[i] / edc[0]);
        if (t5 < 0 && db < -5.0f)
            t5 = (int)i;
        if (t35 < 0 && db < -35.0f)
            t35 = (int)i;
    }
    fx_rings(1);
    return (float)(t35 - t5) * (2.0f / 44100.0f);
}

static void test_rings_pair(void)
{
    double tp, tr;
    float a = rp_t60(0, &tp), b = rp_t60(1, &tr), wpk = 0.0f, late = 0.0f;
    int i, k, bad = 0;
    uint32_t t = 0;
    printf("rings: reverb -60 dB time %.2f s (plate %.2f s), tail energy %.2f dB against the plate's\n", (double)b,
           (double)a, (double)db_of(tr / tp));
    CHECK(fm_fabsf(b - a) <= 0.1f * a, "rings: the reverb's -60 dB time %.2f s, the plate's %.2f s", (double)b, (double)a);
    CHECK(fm_fabsf(db_of(tr / tp)) <= 1.0f, "rings: the reverb's tail %.2f dB from the plate's", (double)db_of(tr / tp));
    fx_rings(1);
    fx_fresh(0, 0, 0, FL_LOFI); /* the plate's impulse test: below -60 dB within 4 s */
    for (k = 0; k < 5 * 44100 / 2205; k++) {
        memset(fx_l, 0, 2205 * sizeof(float));
        memset(fx_r, 0, 2205 * sizeof(float));
        if (k == 0)
            fx_r[0] = 1.0f;
        fx_process(fx_l, fx_r, 2205, 0.0f);
        for (i = 0; i < 2205; i++, t++) {
            float x = fm_maxf(fm_fabsf(fx_l[i]), fm_fabsf(fx_r[i]));
            wpk = fm_maxf(wpk, x);
            if (t >= 4u * 44100u)
                late = fm_maxf(late, x);
        }
    }
    CHECK(wpk > 1e-3f && late < wpk * 1e-3f, "rings: reverb peak %.4f, %.2e after 4 s", (double)wpk, (double)late);
    fx_fresh(3, 1, 1, FL_CHILLWAVE); /* 60 s of random input, the chorus on, then 10 s of silence */
    fx_chorus(0.7f);
    fx_seed = 4242u;
    for (k = 0; k < 70 * 44100 / 2205; k++) {
        static float ch[2205];
        for (i = 0; i < 2205; i++) {
            fx_l[i] = k < 60 * 44100 / 2205 ? fx_white() : 0.0f;
            fx_r[i] = k < 60 * 44100 / 2205 ? fx_white() : 0.0f;
            ch[i] = fx_l[i];
        }
        fx_process_buses(fx_l, fx_r, ch, NULL, 2205, 0.5f);
        for (i = 0; i < 2205; i++)
            if (fx_l[i] != fx_l[i] || fx_r[i] != fx_r[i] || fm_fabsf(fx_l[i]) > 4.0f ||
                (fx_l[i] != 0.0f && fm_fabsf(fx_l[i]) < 1.2e-38f))
                bad++;
    }
    printf("rings: reverb peak %.4f, after 4 s %.2e; random input: %d bad samples, %d tiny states\n", (double)wpk,
           (double)late, bad, fx_tiny());
    CHECK(bad == 0 && fx_tiny() == 0, "rings: %d bad samples, %d tiny states after random input and silence", bad,
          fx_tiny());
    fx_chorus(0.6f);
    CHECK(fm_fabsf(fx_chorus_dry() - 0.7f) < 1e-6f, "rings: the chorus's dry at 0.6 is %.3f, wanted Rings' 0.7",
          (double)fx_chorus_dry());
}

/* Task 8's mix parts: the mud high-pass, the pads' chord changes, the drums' compression per kit */
/* n samples of the engine's left channel into out, from where it is */
static void mx_render(float *out, uint32_t n)
{
    uint32_t d = 0;
    while (d < n) {
        uint32_t b = n - d < 4096u ? n - d : 4096u;
        memset(vt_l, 0, b * sizeof(float));
        memset(vt_r, 0, b * sizeof(float));
        vx_render(vt_l, vt_r, b);
        memcpy(out + d, vt_l, b * sizeof(float));
        d += b;
    }
}

/* a chord (rolled 20 ms a note, as the keys play it) from a dry engine, with the mud high-pass on or off: low, voiced
   from C2 (65 Hz; nothing else under 98 Hz), or in the keys' register (E3-D4); the energy of [from, from + CM_N) in
   bands below 80 Hz and above 200 Hz */
static void mx_chord(int ins, int mud, int low, uint32_t from, double *lo, double *hi)
{
    static const int ch[2][4] = {{52, 55, 59, 62}, {36, 43, 48, 52}};
    int i;
    vx_fx_off(mud ? 0u : VX_OFF_MUD);
    vx_init();
    vx_assign(V_CHORD, ins);
    for (i = 0; i < 4; i++)
        vt_ev(V_CHORD, ch[low][i], 100, (uint32_t)i * 882u, 3u * 44100u);
    mx_render(cm_x, from + CM_N);
    cm_spec(cm_x + from, CM_N, cm_pa, 0);
    *lo = cm_band(cm_pa, CM_N, 1.0f, 80.0f);
    *hi = cm_band(cm_pa, CM_N, 200.0f, 20000.0f);
    vx_fx_off(0u);
}

/* the level (dB, 50 ms windows) of buf: the lowest window in [a, b) against the mean of [c, d) */
static float mx_dip(const float *x, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    double ref = 0.0, lo = 1e30;
    uint32_t i, w;
    for (i = c; i < d; i++)
        ref += (double)x[i] * (double)x[i];
    ref /= (double)(d - c);
    for (w = a; w + 2205u <= b; w += 441u) {
        double e = 0.0;
        for (i = w; i < w + 2205u; i++)
            e += (double)x[i] * (double)x[i];
        e /= 2205.0;
        lo = e < lo ? e : lo;
    }
    return db_of(lo / ref);
}

/* Task 8's new states never rest as denormals: a loud passage (the glue's hold rises), then 100 s of a quiet held
   chord (the glue running, its reduction under 1 dB, so the hold decays; the mud filters ringing out after the
   notes end); then every state of the chain (fx_tiny) and the voices' mix (vx_tiny) is exactly 0 or >= 1e-20 */
static void test_mix_denormals(void)
{
    song_roll_t roll;
    int i, b, bad = 0;
    roll_song(NULL, 3u, &roll);
    roll.v[RF_NOISE] = roll.v[RF_AMBIENCE] = 0;
    vx_init();
    vx_configure(&roll);
    for (i = 0; i < 4; i++)
        vt_ev(V_CHORD, 60 + 4 * i, 127, 0, 44100u);
    for (i = 0; i < 8; i++)
        vt_ev(V_KICK, 0, 127, (uint32_t)i * 5512u, 0);
    vt_ev(V_LEAD, 72, 110, 0, 22050u);
    vt_ev(V_CANDY, 84, 110, 0, 22050u);
    for (b = 0; b < 22; b++)
        vt_block(4096, &bad);
    for (i = 0; i < 3; i++)
        vt_ev(V_CHORD, 55 + 4 * i, 4, 0, 90u * 44100u);
    for (b = 0; b < (int)(100u * 44100u / 4096u); b++)
        vt_block(4096, &bad);
    printf("mix: after 100 s quiet, %d chain and %d mix states denormal-bound\n", fx_tiny(), vx_tiny());
    CHECK(fx_tiny() == 0 && vx_tiny() == 0 && !bad, "denormals: chain %d, mix %d tiny states (bad %d)", fx_tiny(),
          vx_tiny(), bad);
}

#define MX_BAR 124518u /* a bar at 85 BPM */
static void test_mix_parts(void)
{
    static const int mud_ins[2] = {INS_RHODES, INS_PAD};
    int k, i;
    /* the mud high-pass: a Rhodes and a pad chord voiced from C2 lose 12 dB or more below 80 Hz; in the keys'
       register, little above 200 Hz */
    for (k = 0; k < 2; k++) {
        double lo0, hi0, lo1, hi1;
        uint32_t from = k ? 30000u : 4410u;
        double x;
        mx_chord(mud_ins[k], 0, 1, from, &lo0, &x);
        mx_chord(mud_ins[k], 1, 1, from, &lo1, &x);
        mx_chord(mud_ins[k], 0, 0, from, &x, &hi0);
        mx_chord(mud_ins[k], 1, 0, from, &x, &hi1);
        printf("mix: %s chord below 80 Hz %.1f dB with the mud high-pass, above 200 Hz %.2f dB\n", k ? "pad" : "rhodes",
               (double)db_of(lo1 / lo0), (double)db_of(hi1 / hi0));
        CHECK(db_of(lo1 / lo0) <= -12.0f, "mud high-pass: %s below 80 Hz only %.1f dB down", k ? "pad" : "rhodes",
              (double)db_of(lo1 / lo0));
        CHECK(db_of(hi1 / hi0) >= -1.5f, "mud high-pass: %s above 200 Hz %.2f dB down", k ? "pad" : "rhodes",
              (double)db_of(hi1 / hi0));
    }

    /* the pads' chord change: a held chord (its notes overlapping the next bar by 6%, as the keys hold them), then
       the next chord; the lowest 50 ms around the change against the old chord's level before it */
    {
        static const int a[4] = {57, 60, 64, 67}, b[4] = {55, 59, 62, 65};
        uint32_t n = 3u * MX_BAR;
        float dip;
        vx_init();
        vx_assign(V_CHORD, INS_PAD);
        for (i = 0; i < 4; i++) {
            vt_ev(V_CHORD, a[i], 70, (uint32_t)i * 882u, MX_BAR * 106u / 100u - (uint32_t)i * 882u);
            vt_ev(V_CHORD, b[i], 70, MX_BAR + (uint32_t)i * 882u, 2u * MX_BAR);
        }
        mx_render(sb_x, n);
        dip = mx_dip(sb_x, MX_BAR, MX_BAR + 44100u, MX_BAR - 30000u, MX_BAR - 4410u);
        printf("mix: a pad's chord change dips %.1f dB\n", (double)dip);
        CHECK(dip >= -3.0f, "pads: the chord change dips %.1f dB, wanted 3 dB at most", (double)dip);

        /* an arpeggio on a pad (a note a 16th, held a little over a 16th) speaks: within 6 dB of the held chord */
        vx_init();
        vx_assign(V_CHORD, INS_PAD);
        for (i = 0; i < 32; i++)
            vt_ev(V_CHORD, a[i & 3], 70, (uint32_t)i * (MX_BAR / 16u), MX_BAR * 19u / 256u);
        mx_render(sb_l, 2u * MX_BAR);
        {
            double e0 = 0.0, e1 = 0.0;
            uint32_t j;
            for (j = MX_BAR / 2u; j < 2u * MX_BAR; j++)
                e1 += (double)sb_l[j] * (double)sb_l[j];
            for (j = MX_BAR / 2u; j < 2u * MX_BAR; j++)
                e0 += (double)sb_x[j] * (double)sb_x[j];
            dip = db_of(e1 / e0);
        }
        printf("mix: a pad's arpeggio sits %.1f dB against its held chord\n", (double)dip);
        CHECK(dip >= -6.0f, "pads: an arpeggio sits %.1f dB under the held chord, wanted 6 dB at most", (double)dip);
    }

    /* the drums' compressor, per kit: 3-6 dB of reduction on a full-velocity kick and snare */
    {
        static const char *const kn[] = {"dusty", "soft", "808", "brushes"};
        int kit, v;
        for (kit = KIT_DUSTY; kit <= KIT_BRUSHES; kit++)
            for (v = V_KICK; v <= V_SNARE; v++) {
                float gr = 0.0f;
                int b;
                vx_init();
                vx_set_kit(kit);
                vt_ev(v, 0, 127, 0, 0);
                for (b = 0; b < 200; b++) {
                    vx_render(vt_l, vt_r, 32);
                    gr = fm_maxf(gr, vx_comp_db());
                }
                printf("mix: %s %s at 127: %.1f dB of compression\n", kn[kit], v == V_KICK ? "kick" : "snare",
                       (double)gr);
                CHECK(gr >= 3.0f && gr <= 6.0f, "compressor: %s %s at 127 gets %.1f dB, wanted 3-6", kn[kit],
                      v == V_KICK ? "kick" : "snare", (double)gr);
            }
    }

    /* the glue: none in a dry engine (vx_init), on once a song configures the chain */
    {
        song_roll_t roll;
        vx_init();
        for (i = 0; i < 4; i++)
            vt_ev(V_CHORD, 60 + 4 * i, 127, 0, 44100u);
        vt_ev(V_KICK, 0, 127, 0, 0);
        mx_render(sb_l, 22050u);
        CHECK(fx_glue_db() == 0.0f, "glue: %.2f dB in a dry engine", (double)fx_glue_db());
        roll_song(NULL, 3u, &roll);
        vx_init();
        vx_configure(&roll);
        for (i = 0; i < 4; i++)
            vt_ev(V_CHORD, 60 + 4 * i, 127, 0, 44100u);
        for (i = 0; i < 4; i++)
            vt_ev(V_KICK, 0, 127, (uint32_t)i * 11025u, 0);
        mx_render(sb_l, 44100u);
        CHECK(fx_glue_db() > 0.5f, "glue: %.2f dB on a loud passage", (double)fx_glue_db());
    }
}

/* ------------------------------------------------------------------- Task 8: the mix, steady levels --- */
/* A whole song through the engine as the app plays it (vx_song, the section moves and the last block's fade each
   step, song_tick, vx_render with the chain), its bed off unless bed: per bar the energy of both channels, the glue
   compressor's mean reduction (dB), the peak. */
#define LV_BARS 80
typedef struct {
    double e[LV_BARS], gr[LV_BARS];
    uint32_t n[LV_BARS];
    float peak;
    song_plan_t plan;
    song_roll_t roll;
    int bars;
} lv_t;
static lv_t lv;
static void lv_song(uint32_t seed, int set, int bed)
{
    static song_t s;
    song_ev_t ev[64];
    float frac = 0.0f;
    memset(&lv, 0, sizeof lv);
    roll_song(NULL, seed, &lv.roll);
    if (set >= 0)
        (void)roll_set(&lv.roll, RF_SET, set);
    if (!bed)
        lv.roll.v[RF_NOISE] = lv.roll.v[RF_AMBIENCE] = 0;
    vx_init();
    song_start(&s, &lv.roll);
    vx_song(&s.roll);
    lv.plan = s.plan;
    lv.bars = s.roll.v[RF_BARS];
    while (s.state == SS_PLAYING) {
        const plan_block_t *last = &s.plan.b[s.plan.n ? s.plan.n - 1 : 0];
        int bar = s.bar, st = s.step, n, i;
        float adv;
        uint32_t left;
        sb_moves(&s.plan, bar, st, 1);
        if (s.plan.n && (last->flags & PB_FADE) && bar >= last->start)
            vx_set_gain(1.0f - (float)((bar - last->start) * 16 + st + 1) / (float)(last->bars * 16));
        n = song_tick(&s, ev, 64);
        for (i = 0; i < n; i++)
            vx_event(&ev[i]);
        adv = song_step_samples(&s) + frac;
        left = (uint32_t)adv;
        frac = adv - (float)left;
        while (left) {
            uint32_t b = left < 1024u ? left : 1024u, k;
            double e = 0.0;
            memset(vt_l, 0, b * sizeof(float));
            memset(vt_r, 0, b * sizeof(float));
            vx_render(vt_l, vt_r, b);
            for (k = 0; k < b; k++) {
                e += (double)vt_l[k] * (double)vt_l[k] + (double)vt_r[k] * (double)vt_r[k];
                lv.peak = fm_maxf(lv.peak, fm_maxf(fm_fabsf(vt_l[k]), fm_fabsf(vt_r[k])));
            }
            if (bar < LV_BARS) {
                lv.e[bar] += e;
                lv.n[bar] += 2u * b;
                lv.gr[bar] += (double)fx_glue_db() * b;
            }
            left -= b;
        }
    }
}

/* the RMS (dB) and the glue's mean reduction over bars [b0, b1) */
static float lv_db(int b0, int b1, float *gr)
{
    double e = 0.0, g = 0.0;
    uint32_t n = 0;
    int b;
    for (b = b0; b < b1 && b < LV_BARS; b++) {
        e += lv.e[b];
        g += lv.gr[b];
        n += lv.n[b];
    }
    if (gr)
        *gr = n ? (float)(2.0 * g / n) : 0.0f;
    return n ? db_of(e / n) : -999.0f;
}

/* a block that plays the full band: keys, kick, snare and bass, a groove or a lead section, no fade */
static int lv_full(const plan_block_t *b)
{
    int want = L_KEYS | L_KICK | L_SNARE | L_BASS;
    return (b->layers & want) == want && (b->section == SEC_GROOVE || b->section == SEC_LEAD) && !(b->flags & PB_FADE);
}
/* a block allowed to be quieter: an intro, an outro, a break, the fade */
static int lv_quiet(const plan_block_t *b)
{
    return b->section == SEC_INTRO || b->section == SEC_OUTRO || b->section == SEC_BREAK || (b->flags & PB_FADE);
}

#define LIB_NSETS_MAX 32 /* test_lead_over_keys' arrays; it CHECKs LIB_NSETS fits */
static int lk_cmp_f(const void *a, const void *b);
static int lv_cmp(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;
    return x < y ? -1 : x > y;
}

/* Step 1: over 50 rolled songs (seeds 1-50, the roll's own set draw), then one song of each set the 50 left out
   (pinned with roll_set: the synth sets' songs are judged too), each song's full-band RMS within 3 dB of the median
   song's; inside a song no block's RMS more than 6 dB from the last full-level block's (intros, outros, breaks and
   fades may be quieter, never louder than the song's full band); the peak under 0 dBFS. */
#define LV_N 50
#define LV_ALL (LV_N + LIB_NSETS_MAX)
static float lv_song_db[LV_ALL], lv_song_gr[LV_ALL], lv_intro_gr[LV_ALL], lv_intro_db[LV_ALL], lv_worst_jump;
static void test_levels_steady(void)
{
    float sorted[LV_ALL], med, peak = 0.0f, worst = 0.0f;
    uint32_t sd[LV_ALL];
    int pin[LV_ALL], seen[LIB_NSETS_MAX + 1], k, n = 0, bad_jump = 0, bad_quiet = 0;
    lv_worst_jump = 0.0f;
    memset(seen, 0, sizeof seen);
    CHECK(LIB_NSETS <= LIB_NSETS_MAX, "levels: %d sets, LIB_NSETS_MAX %d: raise it", LIB_NSETS, LIB_NSETS_MAX);
    for (k = 0; k < LV_N; k++) { /* the draw, then a pinned song for each set it missed */
        song_roll_t r;
        roll_song(NULL, (uint32_t)k + 1u, &r);
        sd[n] = (uint32_t)k + 1u;
        pin[n++] = -1;
        if (r.v[RF_SET] <= LIB_NSETS_MAX)
            seen[r.v[RF_SET]] = 1;
    }
    for (k = 0; k < LIB_NSETS && k < LIB_NSETS_MAX; k++) {
        uint32_t seed;
        if (seen[LIB_SETS[k].id])
            continue;
        for (seed = 51u; seed < 600u; seed++) {
            song_roll_t r;
            roll_song(NULL, seed, &r);
            if (roll_set(&r, RF_SET, LIB_SETS[k].id) == LIB_SETS[k].id)
                break;
        }
        CHECK(seed < 600u, "levels: no seed takes set %s", LIB_SETS[k].name);
        sd[n] = seed;
        pin[n++] = LIB_SETS[k].id;
    }
    for (k = 0; k < n; k++) {
        const lib_set_t *set;
        float full, prev = -999.0f;
        double e = 0.0;
        uint32_t nn = 0;
        int j, b;
        lv_song(sd[k], pin[k], 0);
        set = lib_set_by_id((uint8_t)lv.roll.v[RF_SET]);
        for (j = 0; j < lv.plan.n; j++)
            if (lv_full(&lv.plan.b[j]))
                for (b = lv.plan.b[j].start; b < lv.plan.b[j].start + lv.plan.b[j].bars && b < LV_BARS; b++) {
                    e += lv.e[b];
                    nn += lv.n[b];
                }
        full = nn ? db_of(e / nn) : -999.0f;
        lv_song_db[k] = full;
        {
            float gs = 0.0f, gi = 0.0f;
            double g = 0.0;
            int gn = 0;
            for (j = 0; j < lv.plan.n; j++)
                if (lv_full(&lv.plan.b[j])) {
                    lv_db(lv.plan.b[j].start, lv.plan.b[j].start + lv.plan.b[j].bars, &gs);
                    g += (double)gs * lv.plan.b[j].bars;
                    gn += lv.plan.b[j].bars;
                }
            lv_song_gr[k] = gn ? (float)(g / gn) : 0.0f;
            lv_intro_gr[k] = -1.0f;
            lv_intro_db[k] = -999.0f;
            if (lv.plan.n && lv.plan.b[0].section == SEC_INTRO) {
                lv_intro_db[k] = lv_db(lv.plan.b[0].start, lv.plan.b[0].start + lv.plan.b[0].bars, &gi);
                lv_intro_gr[k] = gi;
            }
        }
        peak = fm_maxf(peak, lv.peak);
        for (j = 0; j < lv.plan.n; j++) {
            const plan_block_t *pb = &lv.plan.b[j];
            float d = lv_db(pb->start, pb->start + pb->bars, NULL);
            if (lv_quiet(pb)) {
                if (d > full + 1.0f) {
                    bad_quiet++;
                    printf("levels: seed %d block %d (%s) %.1f dB, louder than the full band's %.1f\n", (int)sd[k], j,
                           plan_section_name(pb->section), (double)d, (double)full);
                }
                continue;
            }
            if (prev > -999.0f) {
                float jmp = fm_fabsf(d - prev);
                if (jmp > lv_worst_jump)
                    lv_worst_jump = jmp;
                if (jmp > 6.0f) {
                    bad_jump++;
                    printf("levels: seed %d block %d (%s) %.1f dB after %.1f\n", (int)sd[k], j,
                           plan_section_name(pb->section), (double)d, (double)prev);
                }
            }
            prev = d;
        }
        printf("levels: seed %2d set %-17s full %.1f dB, glue %.1f dB; intro %.1f dB, glue %.1f; peak %.3f\n", (int)sd[k],
               set ? set->name : "?", (double)full, (double)lv_song_gr[k], (double)lv_intro_db[k],
               (double)lv_intro_gr[k], (double)lv.peak);
    }
    memcpy(sorted, lv_song_db, (size_t)n * sizeof sorted[0]);
    qsort(sorted, (size_t)n, sizeof sorted[0], lv_cmp);
    med = n & 1 ? sorted[n / 2] : 0.5f * (sorted[n / 2 - 1] + sorted[n / 2]);
    for (k = 0; k < n; k++)
        worst = fm_maxf(worst, fm_fabsf(lv_song_db[k] - med));
    printf("levels: median %.1f dB, spread %.1f..%.1f (worst %.1f off), worst block jump %.1f dB, peak %.3f\n",
           (double)med, (double)sorted[0], (double)sorted[n - 1], (double)worst, (double)lv_worst_jump,
           (double)peak);
    for (k = 0; k < n; k++)
        CHECK(fm_fabsf(lv_song_db[k] - med) <= 3.0f, "levels: seed %d's full band %.1f dB, the median %.1f", (int)sd[k],
              (double)lv_song_db[k], (double)med);
    CHECK(bad_jump == 0, "levels: %d block jumps over 6 dB", bad_jump);
    CHECK(bad_quiet == 0, "levels: %d quiet blocks louder than the full band", bad_quiet);
    CHECK(peak < 1.0f, "levels: peak %.3f", (double)peak);
    /* the bed (vinyl and rain, its loudest pairing, and tape alone) sits 30-40 dB under each song's own full band */
    {
        float bed[2];
        int j;
        bed[0] = fx_bed_rms(1, 1, 10, NULL);
        bed[1] = fx_bed_rms(2, 0, 10, NULL);
        for (j = 0; j < 2; j++) {
            float lo = 999.0f, hi = -999.0f;
            for (k = 0; k < n; k++) {
                float d = 20.0f * fm_log2f(bed[j]) * 0.30103f - lv_song_db[k];
                lo = fm_minf(lo, d);
                hi = fm_maxf(hi, d);
                CHECK(d >= -40.0f && d <= -30.0f, "the bed (%s) %.1f dB under seed %d's music, wanted 30-40",
                      j ? "tape" : "vinyl and rain", (double)-d, (int)sd[k]);
            }
            printf("levels: the bed (%s) %.1f..%.1f dB under the songs' music\n", j ? "tape" : "vinyl and rain",
                   (double)-hi, (double)-lo);
        }
    }
    /* the glue: 1-4 dB on average over a song's full band, under 1 dB in its intro */
    for (k = 0; k < n; k++) {
        CHECK(lv_song_gr[k] >= 1.0f && lv_song_gr[k] <= 4.0f, "glue: seed %d's full band %.2f dB, wanted 1-4", (int)sd[k],
              (double)lv_song_gr[k]);
        CHECK(lv_intro_gr[k] < 1.0f, "glue: seed %d's intro %.2f dB, wanted under 1", (int)sd[k], (double)lv_intro_gr[k]);
    }
}

/* ------------------------------------------------------ the lead leads: its level over the keys, the duck --- */
/* A song as the app plays it (vx_song, the moves, the fade), its bed off, with only one role's bus in the mix
   (vx_solo: every voice still plays, so the keys' duck follows the lead) and the glue off (it would level a soloed
   bus as it never does in the mix, where it moves every bus alike): per bar the energy of both channels, in dB (-200
   for a silent bar), and in lk_on whether a lead note starts in it. */
#define LK_BARS 512
static double lk_db[2][LK_BARS];
static uint8_t lk_on[LK_BARS];
static int lk_skip_lane; /* a V_PAD lane (1-4) whose events lk_song drops (test_pad_level: 4, the solo note), 0 none */
static int lk_keys = INS_NONE, lk_lead = INS_NONE; /* test_samp: the keys' and the lead's instrument in place of the
                                                      set's (INS_NONE: the set's) */
static int lk_song(const song_roll_t *roll, int role, double *db)
{
    static song_t s;
    song_ev_t ev[64];
    double e = 0.0;
    uint32_t cnt = 0;
    float frac = 0.0f;
    int cur = 0;
    vx_bed_off(1);
    vx_fx_off(VX_OFF_GLUE);
    vx_init();
    vx_solo(role);
    memset(lk_on, 0, sizeof lk_on);
    song_start(&s, roll);
    vx_song(&s.roll);
    if (lk_keys != INS_NONE)
        vx_assign(V_CHORD, lk_keys);
    if (lk_lead != INS_NONE)
        vx_assign(V_LEAD, lk_lead);
    while (s.state == SS_PLAYING) {
        const plan_block_t *last = &s.plan.b[s.plan.n ? s.plan.n - 1 : 0];
        int bar = s.bar, st = s.step, n, i;
        float adv;
        uint32_t left;
        sb_moves(&s.plan, bar, st, 1);
        if (s.plan.n && (last->flags & PB_FADE) && bar >= last->start)
            vx_set_gain(1.0f - (float)((bar - last->start) * 16 + st + 1) / (float)(last->bars * 16));
        if (bar != cur && cur < LK_BARS) {
            db[cur] = cnt ? 10.0 * (double)fm_log2f((float)(e / cnt + 1e-20)) * 0.30103 : -200.0;
            e = 0.0;
            cnt = 0;
            cur = bar;
        }
        n = song_tick(&s, ev, 64);
        for (i = 0; i < n; i++) {
            if (lk_skip_lane && ev[i].voice == V_PAD && ev[i].pad == lk_skip_lane)
                continue;
            vx_event(&ev[i]);
            if (ev[i].voice == V_LEAD && bar < LK_BARS)
                lk_on[bar] = 1;
        }
        adv = song_step_samples(&s) + frac;
        left = (uint32_t)adv;
        frac = adv - (float)left;
        while (left) {
            uint32_t b = left < 1024u ? left : 1024u, k;
            memset(vt_l, 0, b * sizeof(float));
            memset(vt_r, 0, b * sizeof(float));
            vx_render(vt_l, vt_r, b);
            for (k = 0; k < b; k++)
                e += (double)vt_l[k] * (double)vt_l[k] + (double)vt_r[k] * (double)vt_r[k];
            cnt += 2u * b;
            left -= b;
        }
    }
    vx_solo(-1);
    vx_fx_off(0u);
    vx_bed_off(0);
    return cur < LK_BARS ? cur : LK_BARS;
}

/* lead minus keys (dB), the mean over the bars where both play (a lead note starts in the bar, the keys sound over
   -60 dB: a bar of only the lead's echo tail is a rest); the bars counted in *nb */
static float lk_diff(const song_roll_t *roll, int *nb)
{
    int b, n = lk_song(roll, V_CHORD, lk_db[0]), m = lk_song(roll, V_LEAD, lk_db[1]), c = 0;
    double sum = 0.0;
    for (b = 0; b < n && b < m; b++)
        if (lk_on[b] && lk_db[1][b] > -45.0 && lk_db[0][b] > -60.0) {
            sum += lk_db[1][b] - lk_db[0][b];
            c++;
        }
    *nb = c;
    return c ? (float)(sum / c) : 0.0f;
}

/* The lead leads (Sam: "the accompaniment is way too loud. I can barely hear the lead above it"): in every set (so
   every lead instrument over every keys instrument, the synths' too), six songs each, while the lead plays it sits
   over the keys: each set's mean 4-6.5 dB and its median 4-6 (the spec: 4-6 dB in every set), the median over all
   songs 4-6, and no song under 2 dB. Each song's keys and lead are its set's (no set left out: LIB_NSETS_MAX). */
#define LK_PER 6
static void test_lead_over_keys(void)
{
    float d[LIB_NSETS_MAX * LK_PER], sorted[LIB_NSETS_MAX * LK_PER], med;
    int k, n = 0;
    CHECK(LIB_NSETS <= LIB_NSETS_MAX, "lead over keys: %d sets, LIB_NSETS_MAX %d: raise it", LIB_NSETS, LIB_NSETS_MAX);
    for (k = 0; k < LIB_NSETS && k < LIB_NSETS_MAX; k++) {
        const lib_set_t *t = &LIB_SETS[k];
        uint32_t seed;
        int got = 0, n0 = n;
        float lo = 99.0f, hi = -99.0f, mean = 0.0f, smed, ss[LK_PER];
        for (seed = 1u; got < LK_PER && seed < 600u; seed++) {
            song_roll_t r;
            int nb;
            float x;
            roll_song(NULL, seed, &r);
            if (roll_set(&r, RF_SET, t->id) != t->id)
                continue;
            x = lk_diff(&r, &nb);
            if (nb < 4)
                continue;
            CHECK(vx_config()->chords == t->chords && vx_config()->lead == t->lead,
                  "lead over keys: %s seed %u plays keys %d lead %d, its set's %d %d", t->name, (unsigned)seed,
                  vx_config()->chords, vx_config()->lead, t->chords, t->lead);
            d[n++] = x;
            got++;
            mean += x;
            lo = fm_minf(lo, x);
            hi = fm_maxf(hi, x);
            CHECK(x >= 6.0f, "lead over keys: %s seed %u's lead %.1f dB over its keys, wanted 6 at least", t->name,
                  (unsigned)seed, (double)x);
        }
        CHECK(got == LK_PER, "lead over keys: only %d songs found for %s", got, t->name);
        if (!got)
            continue;
        mean /= (float)got;
        memcpy(ss, d + n0, (size_t)got * sizeof ss[0]);
        qsort(ss, (size_t)got, sizeof ss[0], lk_cmp_f);
        smed = got & 1 ? ss[got / 2] : 0.5f * (ss[got / 2 - 1] + ss[got / 2]);
        printf("lead over keys: %-21s mean %+.1f median %+.1f (%+.1f..%+.1f) dB\n", t->name, (double)mean,
               (double)smed, (double)lo, (double)hi);
        CHECK(mean >= 8.0f && mean <= 10.5f, "lead over keys: %s's mean %.1f dB, wanted 8-10.5 (Sam: the comping quieter)", t->name, (double)mean);
        CHECK(smed >= 8.0f && smed <= 10.0f, "lead over keys: %s's median %.1f dB, wanted 8-10", t->name, (double)smed);
    }
    memcpy(sorted, d, (size_t)n * sizeof sorted[0]);
    qsort(sorted, (size_t)n, sizeof sorted[0], lk_cmp_f);
    med = n ? (n & 1 ? sorted[n / 2] : 0.5f * (sorted[n / 2 - 1] + sorted[n / 2])) : 0.0f;
    printf("lead over keys: median %+.1f dB over %d songs (%+.1f..%+.1f)\n", (double)med, n, (double)sorted[0],
           (double)sorted[n - 1]);
    CHECK(med >= 8.0f && med <= 10.0f, "lead over keys: the median %.1f dB, wanted 8-10", (double)med);
}

/* The pad layer's chord part sits under the keys (pads design, Mix: about 6 dB): in every set with percussive keys
   (the chord part plays over those only), six songs each as test_lead_over_keys picks them, the pad bus soloed with
   its solo note left out (lane 4's events dropped) against the keys' bus, over the bars where both sound (over -60
   dB): each set's mean and median 7-11 dB under, and no song under 5 dB or over 13. */
static void test_pad_level(void)
{
    float d[LIB_NSETS_MAX * LK_PER], sorted[LIB_NSETS_MAX * LK_PER], med;
    int k, n = 0, sets = 0;
    for (k = 0; k < LIB_NSETS && k < LIB_NSETS_MAX; k++) {
        const lib_set_t *t = &LIB_SETS[k];
        uint32_t seed;
        int got = 0, n0 = n;
        float lo = 99.0f, hi = -99.0f, mean = 0.0f, smed, ss[LK_PER];
        if (lib_ins_sustained(t->chords))
            continue;
        sets++;
        for (seed = 1u; got < LK_PER && seed < 600u; seed++) {
            song_roll_t r;
            int b, nk, np, c = 0;
            double sum = 0.0;
            float x;
            roll_song(NULL, seed, &r);
            if (roll_set(&r, RF_SET, t->id) != t->id)
                continue;
            nk = lk_song(&r, V_CHORD, lk_db[0]);
            lk_skip_lane = 4;
            np = lk_song(&r, V_PAD, lk_db[1]);
            lk_skip_lane = 0;
            for (b = 0; b < nk && b < np; b++)
                if (lk_db[0][b] > -60.0 && lk_db[1][b] > -60.0) {
                    sum += lk_db[0][b] - lk_db[1][b];
                    c++;
                }
            if (c < 8)
                continue;
            x = (float)(sum / c);
            d[n++] = x;
            got++;
            mean += x;
            lo = fm_minf(lo, x);
            hi = fm_maxf(hi, x);
            CHECK(x >= 5.0f && x <= 13.0f, "pad level: %s seed %u's chord part %.1f dB under its keys, wanted 5-13",
                  t->name, (unsigned)seed, (double)x);
        }
        CHECK(got == LK_PER, "pad level: only %d songs found for %s", got, t->name);
        if (!got)
            continue;
        mean /= (float)got;
        memcpy(ss, d + n0, (size_t)got * sizeof ss[0]);
        qsort(ss, (size_t)got, sizeof ss[0], lk_cmp_f);
        smed = got & 1 ? ss[got / 2] : 0.5f * (ss[got / 2 - 1] + ss[got / 2]);
        printf("pad level: %-21s chord part under the keys: mean %.1f median %.1f (%.1f..%.1f) dB\n", t->name,
               (double)mean, (double)smed, (double)lo, (double)hi);
        CHECK(mean >= 7.0f && mean <= 11.0f, "pad level: %s's mean %.1f dB under, wanted 7-11", t->name, (double)mean);
        CHECK(smed >= 7.0f && smed <= 11.0f, "pad level: %s's median %.1f dB under, wanted 7-11", t->name, (double)smed);
    }
    CHECK(sets >= 10, "pad level: only %d sets with percussive keys", sets);
    memcpy(sorted, d, (size_t)n * sizeof sorted[0]);
    qsort(sorted, (size_t)n, sizeof sorted[0], lk_cmp_f);
    med = n ? (n & 1 ? sorted[n / 2] : 0.5f * (sorted[n / 2 - 1] + sorted[n / 2])) : 0.0f;
    printf("pad level: median %.1f dB under over %d songs (%.1f..%.1f)\n", (double)med, n, n ? (double)sorted[0] : 0.0,
           n ? (double)sorted[n - 1] : 0.0);
    {   /* the solo voices at the songs' velocity (58; Task 1 matched them at 100): their spread, reported (pads
           revision 2: the levels are Task 4's) */
        static const int ins[4] = {INS_VOX_PAD, INS_FLUTE, INS_ORGAN, INS_STRINGS};
        static const int note[4] = {76, 76, 76, 88};
        static const int vel[2] = {100, 58};
        float db[2][4], lo[2] = {1e9f, 1e9f}, hi[2] = {-1e9f, -1e9f};
        int i, v, j, bad = 0;
        for (v = 0; v < 2; v++)
            for (i = 0; i < 4; i++) {
                song_ev_t e;
                double en = 0.0;
                vx_init();
                vx_assign(V_PAD, INS_STRINGS);
                vx_assign_pad_solo(ins[i]);
                vx_solo(V_PAD);
                memset(&e, 0, sizeof e);
                e.voice = V_PAD;
                e.note = (uint8_t)note[i];
                e.vel = (uint8_t)vel[v];
                e.len = 132300u;
                e.pad = 4;
                vx_event(&e);
                for (j = 0; j < 32; j++) {
                    uint32_t q;
                    vt_block(4096, &bad);
                    if (j >= 11)
                        for (q = 0; q < 4096; q++)
                            en += (double)vt_l[q] * (double)vt_l[q];
                }
                db[v][i] = 10.0f * 0.30103f * fm_log2f((float)(en / (21.0 * 4096.0)) + 1e-30f);
                lo[v] = fm_minf(lo[v], db[v][i]);
                hi[v] = fm_maxf(hi[v], db[v][i]);
            }
        vx_solo(-1);
        printf("pad level: the solo voices (choir, flute, organ, strings) at velocity 100: %.1f %.1f %.1f %.1f dBFS "
               "(spread %.1f); at 58: %.1f %.1f %.1f %.1f (spread %.1f); 100 to 58: %.1f %.1f %.1f %.1f dB\n",
               (double)db[0][0], (double)db[0][1], (double)db[0][2], (double)db[0][3], (double)(hi[0] - lo[0]),
               (double)db[1][0], (double)db[1][1], (double)db[1][2], (double)db[1][3], (double)(hi[1] - lo[1]),
               (double)(db[1][0] - db[0][0]), (double)(db[1][1] - db[0][1]), (double)(db[1][2] - db[0][2]),
               (double)(db[1][3] - db[0][3]));
        CHECK(hi[1] - lo[1] <= 2.0f, "pad level: the solo voices at velocity 58 span %.1f dB", (double)(hi[1] - lo[1]));
        CHECK(!bad, "pad level: the solo voices: NaN or inf");
    }
}

/* The keys duck under the lead: a held pad chord (keys only in the mix, vx_solo) against the same chord with a lead
   note from 1 s to 2 s: 2-4 dB down while the lead sounds; not at once (under 1.5 dB 10 ms into the note) but by
   100 ms at least 1.5 dB; easing back (still 1 dB down 100 ms after the note's end), and back within 0.3 dB a
   second and a half after it; at rest, no duck at all. */
static double lk_win(const float *x, uint32_t a, uint32_t b)
{
    double e = 0.0;
    uint32_t i;
    for (i = a; i < b; i++)
        e += (double)x[i] * (double)x[i];
    return e;
}
static void test_keys_duck(void)
{
    static const int ch[4] = {57, 60, 64, 67};
    float at10 = 0.0f, at100 = 0.0f, rel100 = 0.0f, rest = 0.0f, dip, back;
    int k, i;
    uint32_t t;
    for (k = 0; k < 2; k++) {
        float *out = k ? sb_x : sb_l;
        vx_init();
        vx_solo(V_CHORD);
        vx_assign(V_CHORD, INS_PAD);
        vx_assign(V_LEAD, INS_FLUTE);
        for (i = 0; i < 4; i++)
            vt_ev(V_CHORD, ch[i], 90, 0, 4u * 44100u);
        if (k)
            vt_ev(V_LEAD, 76, 100, 44100u, 44100u);
        for (t = 0; t < 4u * 44100u + 22050u; t += 256u) {
            memset(vt_l, 0, 256 * sizeof(float));
            memset(vt_r, 0, 256 * sizeof(float));
            vx_render(vt_l, vt_r, 256);
            memcpy(out + t, vt_l, 256 * sizeof(float));
            /* (the duck after each 256-sample block: the block ending at or past 10 ms, 100 ms) */
            if (!k)
                rest = fm_minf(rest, vx_duck_db());
            else if (t < 44100u + 441u && t + 256u >= 44100u + 441u)
                at10 = vx_duck_db();
            else if (t < 44100u + 4410u && t + 256u >= 44100u + 4410u)
                at100 = vx_duck_db();
            else if (t < 2u * 44100u + 4410u && t + 256u >= 2u * 44100u + 4410u)
                rel100 = vx_duck_db();
        }
    }
    vx_solo(-1);
    dip = db_of(lk_win(sb_x, 66150u, 88200u) / lk_win(sb_l, 66150u, 88200u));
    back = db_of(lk_win(sb_x, 154350u, 176400u) / lk_win(sb_l, 154350u, 176400u));
    printf("duck: keys %.1f dB under the lead, %.1f dB 1.5 s after; at 10 ms %.1f, 100 ms %.1f, 100 ms after %.1f; "
           "at rest %.1f\n", (double)dip, (double)back, (double)at10, (double)at100, (double)rel100, (double)rest);
    CHECK(dip <= -2.0f && dip >= -4.0f, "duck: the keys %.1f dB under the lead, wanted 2-4", (double)-dip);
    CHECK(fm_fabsf(back) <= 0.3f, "duck: the keys %.1f dB off 1.5 s after the lead", (double)back);
    CHECK(at10 > -1.5f && at100 <= -1.5f, "duck: %.1f dB at 10 ms, %.1f at 100 ms (wanted a 20-30 ms attack)",
          (double)at10, (double)at100);
    CHECK(rel100 <= -1.0f, "duck: %.1f dB 100 ms after the note (wanted a slow release)", (double)rel100);
    CHECK(rest == 0.0f, "duck: %.1f dB with no lead", (double)rest);
}

/* The duck on the mix paths the songs take (test_keys_duck hears it through vx_solo, which forces the filtered
   path): the section filters off (the unfiltered mix, as with every bus open), the keys at half rate (the pad's
   default) and at full rate (VX_OFF_HALF). The keys alone can't be soloed there, so they come by subtraction: the
   chain here is linear (no drums, glue or bed; the soft clip idle at these levels), so the mix of keys and lead minus
   the lead alone is the ducked keys; against the keys alone, 2-4 dB down while the lead plays, back within 0.3 dB
   1.5 s after it, and the same (0.3 dB) before it. A vibes lead: no noise draw to shift with the keys. */
static void test_keys_duck_mix(void)
{
    static const int ch[4] = {57, 60, 64, 67};
    int pass, k, i;
    uint32_t t, N = 4u * 44100u + 22050u;
    for (pass = 0; pass < 2; pass++) {
        unsigned off = VX_OFF_FILTERS | VX_OFF_GLUE | (pass ? VX_OFF_HALF : 0u);
        float pre, dip, back;
        vx_bed_off(1);
        vx_fx_off(off);
        for (k = 0; k < 3; k++) { /* 0: keys alone (sb_l), 1: keys and lead (sb_x), 2: the lead alone (sb_r) */
            float *out = k == 0 ? sb_l : k == 1 ? sb_x : sb_r;
            vx_init();
            vx_assign(V_CHORD, INS_PAD);
            vx_assign(V_LEAD, INS_VIBES);
            if (k < 2)
                for (i = 0; i < 4; i++)
                    vt_ev(V_CHORD, ch[i], 90, 0, 4u * 44100u);
            if (k)
                vt_ev(V_LEAD, 76, 100, 44100u, 44100u);
            for (t = 0; t < N; t += 256u) {
                memset(vt_l, 0, 256 * sizeof(float));
                memset(vt_r, 0, 256 * sizeof(float));
                vx_render(vt_l, vt_r, 256);
                memcpy(out + t, vt_l, 256 * sizeof(float));
            }
        }
        for (t = 0; t < N; t++)
            sb_x[t] -= sb_r[t];
        pre = db_of(lk_win(sb_x, 22050u, 44100u) / lk_win(sb_l, 22050u, 44100u));
        dip = db_of(lk_win(sb_x, 66150u, 88200u) / lk_win(sb_l, 66150u, 88200u));
        back = db_of(lk_win(sb_x, 154350u, 176400u) / lk_win(sb_l, 154350u, 176400u));
        printf("duck (unfiltered mix, %s rate): keys %.2f dB before the lead, %.2f under it, %.2f 1.5 s after\n",
               pass ? "full" : "half", (double)pre, (double)dip, (double)back);
        CHECK(fm_fabsf(pre) <= 0.3f, "duck (%s rate): the keys %.2f dB off before the lead", pass ? "full" : "half",
              (double)pre);
        CHECK(dip <= -2.0f && dip >= -4.0f, "duck (%s rate, unfiltered): the keys %.1f dB under the lead, wanted 2-4",
              pass ? "full" : "half", (double)-dip);
        CHECK(fm_fabsf(back) <= 0.3f, "duck (%s rate): the keys %.2f dB off 1.5 s after the lead",
              pass ? "full" : "half", (double)back);
    }
    vx_fx_off(0u);
    vx_bed_off(0);
}

static int lk_cmp_f(const void *a, const void *b) { return lv_cmp(a, b); }

/* The sustained lead's phrase plan (sustained-lead design): the motif's shape and the 8-bar question and answer. */
static int pp_bar(uint32_t seed, int block, int pass, int bar, int pbars, int last, phrase_note_t *o)
{
    return phrase_bar(seed, block, pass, bar, pbars, last, o, 8);
}

static int pp_same(const phrase_note_t *a, int na, const phrase_note_t *b, int nb)
{
    int i;
    if (na != nb) return 0;
    for (i = 0; i < na; i++)
        if (a[i].at != b[i].at || a[i].len != b[i].len || a[i].deg != b[i].deg || a[i].role != b[i].role ||
            a[i].orn != b[i].orn || a[i].on != b[i].on)
            return 0;
    return 1;
}

/* b is a with exactly one nudge: one note moved 1-2 steps (a step, or the last interval altered), the contour
   from some note on inverted, or one note delayed an 8th (2 16ths). Compares onsets and degrees. */
static int pp_one_nudge(const phrase_note_t *a, int na, const phrase_note_t *b, int nb)
{
    int i, h, dat = 0, ddeg = 0, wat = -1, wdeg = -1;
    if (na != nb || na == 0) return 0;
    for (i = 0; i < na; i++) {
        if (a[i].at != b[i].at) { dat++; wat = i; }
        if (a[i].deg != b[i].deg) { ddeg++; wdeg = i; }
    }
    if (dat == 1 && ddeg == 0) return b[wat].at == a[wat].at + 2;
    if (dat != 0 || ddeg == 0) return 0;
    if (ddeg == 1 && b[wdeg].deg - a[wdeg].deg >= -2 && b[wdeg].deg - a[wdeg].deg <= 2) return 1;
    for (h = 1; h < na; h++) {
        int ok = 1;
        for (i = 0; i < h; i++) if (a[i].deg != b[i].deg) ok = 0;
        for (i = h; i < na; i++) if (b[i].deg != 2 * a[h - 1].deg - a[i].deg) ok = 0;
        if (ok) return 1;
    }
    return 0;
}

static int pp_mod7(int d) { return ((d % 7) + 7) % 7; }

static void test_phrase_plan(void)
{
    static const uint8_t vocab[5] = { 2, 3, 4, 6, 8 };
    int s, offb1 = 0, leapy = 0, backs = 0, badm = 0, nseeds = 500;
    int bad_rest = 0, bad_nudge = 0, bad_q = 0, bad_a = 0, bad_dens = 0, bad_b1 = 0, bad_mono = 0, bad_short = 0;
    int bad_pure = 0, bad_orn = 0, ties = 0, pleaps = 0, pbacks = 0, onsets = 0, bars = 0, arc_up = 0, settle = 0, revaried = 0, plans = 0, maxb = 0;
    for (s = 0; s < nseeds; s++) {
        uint32_t seed = (uint32_t)s * 2654435761u + 12345u;
        motif_t2 m;
        int i, leaps = 0, end;
        phrase_motif(seed, &m);
        /* the motif: 3-6 onsets over 1-2 bars, long-note lengths, monophonic, one leap at most and a step back */
        if (m.n < 3 || m.n > 6 || m.bars < 1 || m.bars > 2) badm++;
        for (i = 0; i < m.n && i < 8; i++) {
            int k, inv = 1;
            for (k = 0; k < 5; k++) if (m.len[i] == vocab[k]) inv = 0;
            end = m.at[i] + m.len[i];
            if (inv || end > m.bars * 8 || (i + 1 < m.n && end > m.at[i + 1])) badm++;
            if (i > 0 && (m.step[i] >= 3 || m.step[i] <= -3)) {
                leaps++;
                if (i + 1 < m.n && (m.step[i] > 0 ? m.step[i + 1] == -1 : m.step[i + 1] == 1)) backs++;
            }
        }
        if (leaps > 1) badm++;
        if (leaps) leapy++;
        if (m.at[0] != 0) offb1++;
        { /* the 8-bar plan, for two blocks and two passes */
            int block, pass;
            for (block = 0; block < 2; block++)
                for (pass = 0; pass < 2; pass++) {
                    phrase_note_t b[8][8], o[8], v[8], fl[64];
                    int n[8], ft[64], nf = 0, no, k, j, h, qdeg = 0, adeg = 0, hi01 = -99, hi23 = -99;
                    for (k = 0; k < 8; k++) n[k] = pp_bar(seed, block, pass, k, 8, 0, b[k]);
                    plans++;
                    if (!pp_same(b[4], n[4], b[0], n[0]) || !pp_same(b[5], n[5], b[1], n[1])) bad_nudge++;
                    for (h = 0; h < 2; h++) {   /* bars 3 and 7's varied notes: bars 1 and 5 with one nudge */
                        int nv = 0;
                        for (j = 0; j < n[2 + 4 * h]; j++)
                            if (b[2 + 4 * h][j].role == PR_VARIED) v[nv++] = b[2 + 4 * h][j];
                        if (!pp_one_nudge(b[4 * h], n[4 * h], v, nv)) bad_nudge++;
                    }
                    for (k = 0; k < 8; k++) {
                        onsets += n[k];
                        bars++;
                        if (n[k] > 5) bad_dens++;
                        if (n[k] > maxb) maxb = n[k];
                        if (n[k] && b[k][0].at == 0) bad_b1++;   /* beat 1 struck */
                        for (j = 0; j < n[k]; j++) {
                            const phrase_note_t *e = &b[k][j];
                            if (!e->on || e->at > 15 || e->len == 0) bad_mono++;
                            if (e->orn && (e->at == 0 || e->at == 8 || (e->role != PR_MOTIF && e->role != PR_VARIED)))
                                bad_orn++;
                            if (k < 2 && e->deg > hi01) hi01 = e->deg;
                            if ((k == 2 || k == 3) && e->deg > hi23) hi23 = e->deg;
                            ft[nf] = k * 16 + e->at;
                            fl[nf++] = *e;
                        }
                    }
                    for (j = 0; j + 1 < nf; j++) if (ft[j] + fl[j].len > ft[j + 1]) bad_mono++;
                    /* the question and the arrival: sounding from beat 1 of bars 4 and 8 (an onset there or a tie
                       in from bars 3 and 7) to beat 3, then rest; approached by step */
                    for (h = 0; h < 2; h++) {
                        int e = -1, ok;
                        for (j = 0; j < nf; j++) if (ft[j] < 64 * h + 64) e = j;
                        ok = e > 0 && ft[e] >= 64 * h + 32 && ft[e] <= 64 * h + 48 &&
                             ft[e] + fl[e].len == 64 * h + 56 &&
                             (fl[e].deg - fl[e - 1].deg == 1 || fl[e].deg - fl[e - 1].deg == -1);
                        if (e < 0) { bad_q++; continue; }
                        if (h == 0) { qdeg = fl[e].deg; if (!ok || fl[e].role != PR_QUESTION ||
                                      (pp_mod7(fl[e].deg) != 2 && pp_mod7(fl[e].deg) != 4)) bad_q++; }
                        else { adeg = fl[e].deg; if (!ok || fl[e].role != PR_ARRIVAL || pp_mod7(fl[e].deg) != 0)
                               bad_a++; }
                        if (ft[e] + fl[e].len > 64 * h + 56) bad_rest++;
                        if (ft[e] < 64 * h + 48) ties++;
                    }
                    /* leaps (not right after a rest) answered by a step back, over the whole phrase */
                    for (j = 1; j + 1 < nf; j++) {
                        int d = fl[j].deg - fl[j - 1].deg, nd = fl[j + 1].deg - fl[j].deg;
                        if ((d < 3 && d > -3) || ft[j - 1] + fl[j - 1].len < ft[j]) continue;
                        pleaps++;
                        if ((d > 0 && nd == -1) || (d < 0 && nd == 1)) pbacks++;
                    }
                    if (hi23 >= hi01) arc_up++;
                    if (adeg < qdeg) settle++;
                    /* the motif re-varied each pass */
                    if (pass == 1) {
                        no = pp_bar(seed, block, 0, 2, 8, 0, o);
                        if (!pp_same(o, no, b[2], n[2])) revaried++;
                    }
                    /* a short block: the question, or the answer when it ends the song's lead */
                    for (k = 0; k < 4; k++) {
                        no = pp_bar(seed, block, pass, k, 4, 0, o);
                        if (!pp_same(o, no, b[k], n[k])) bad_short++;
                        no = pp_bar(seed, block, pass, k, 4, 1, o);
                        if (!pp_same(o, no, b[k + 4], n[k + 4])) bad_short++;
                    }
                    if (pp_bar(seed, block, pass, 4, 4, 0, o) != 0 || pp_bar(seed, block, pass, 8, 8, 0, o) != 0)
                        bad_short++;
                    /* pure: backwards, and after another seed's plan */
                    for (k = 7; k >= 0; k--) {
                        pp_bar(seed + 1u, block + 1, pass, 7 - k, 8, 1, o);
                        no = pp_bar(seed, block, pass, k, 8, 0, o);
                        if (!pp_same(o, no, b[k], n[k])) bad_pure++;
                    }
                }
        }
    }
    printf("phrase: off beat 1 %d/%d, leap %d (step back %d), onsets/bar %.2f (max %d), arc up %d/%d, "
           "settles %d/%d, re-varied %d/%d, tied-in targets %d/%d, phrase leaps %d (step back %d)\n", offb1, nseeds,
           leapy, backs, (double)onsets / bars, maxb, arc_up, plans, settle, plans, revaried, plans / 2, ties,
           2 * plans, pleaps, pbacks);
    CHECK(badm == 0, "motif shape broken %d times", badm);
    CHECK(offb1 * 100 >= 30 * nseeds && offb1 * 100 <= 50 * nseeds, "motif off beat 1 in %d of %d seeds (want 30-50%%)",
          offb1, nseeds);
    CHECK(leapy > 0 && backs * 100 >= 70 * leapy, "leaps followed by a step back %d of %d (want >= 70%%)", backs, leapy);
    CHECK(bad_nudge == 0, "bars 5-6 or the nudged bars 3 and 7 broken %d times", bad_nudge);
    CHECK(bad_q == 0, "bar 4's question broken %d times", bad_q);
    CHECK(bad_a == 0, "bar 8's arrival broken %d times", bad_a);
    CHECK(bad_rest == 0, "a question or arrival without half a bar's breath %d times", bad_rest);
    CHECK(bad_dens == 0 && onsets * 2 <= bars * 7, "density: %d bars over 5 onsets, mean %.2f (want <= 3.5)", bad_dens,
          (double)onsets / bars);
    /* Sam: beat 1 is the norm, varied (not avoided): struck in about 65% of bars */
    CHECK(bad_b1 * 100 >= 55 * bars && bad_b1 * 100 <= 80 * bars, "beat 1 struck in %d of %d bars (want 55-80%%)",
          bad_b1, bars);
    CHECK(bad_mono == 0, "overlapping or malformed notes %d times", bad_mono);
    CHECK(bad_orn == 0, "an ornament on beat 1 or 3, or on a cadence note, %d times", bad_orn);
    CHECK(pleaps > 0 && pbacks * 100 >= 70 * pleaps, "phrase leaps followed by a step back %d of %d (want >= 70%%)",
          pbacks, pleaps);
    CHECK(arc_up * 100 >= 70 * plans, "bars 3-4 reach bars 1-2's height in %d of %d plans (want >= 70%%)", arc_up, plans);
    CHECK(settle * 100 >= 90 * plans, "the arrival under the question in %d of %d plans (want >= 90%%)", settle,
          plans);
    CHECK(revaried * 100 >= 50 * (plans / 2), "pass 2 re-varied bar 3 in %d of %d", revaried, plans / 2);
    CHECK(bad_short == 0, "short blocks broken %d times", bad_short);
    CHECK(bad_pure == 0, "phrase_bar not pure %d times", bad_pure);
}

/* ---- the sustained lead sings phrases (sustained-lead design, Task 2) ----
   Songs pinned to the sustained-lead sets, played through song_tick; each bar's plan (s.lead.on/len) recorded as
   notes. Phrase position (the design's call 1): a run of contiguous lead blocks (a request that splits a block, or a
   4-bar lead block after another, resumes the phrase); 8-bar phrases from the run's start; a 4-bar remainder plays
   half a phrase, the answer when the run is the song's last lead run. Rules beat phrasing (the design's call 4): a
   question takes another chord tone than the root where its 3rd and 5th rub the chord; an arrival takes
   the 3rd over home or where the root rubs a maj7; a tie the rules refuse lands on 1, so a cadence and the next
   phrase may both strike beat 1 across the breath (at most 1 cadence in 10). */
static const int SL_SETS[] = {1, 3, 4, 7, 9, 11, 13, 18, 19};
#define SL_NSETS ((int)(sizeof SL_SETS / sizeof SL_SETS[0]))
#define SL_MAXN 640
typedef struct { int start, end, note, vel; } sl_note_t;
typedef struct {
    int n, bars, lead[80], pcs[80][5], np[80], home[80], on0[80];
    int run[80], pass[80], bip[80], pbars[80], last[80], ktop[80];
    sl_note_t nt[SL_MAXN];
} sl_song_t;
static sl_song_t sl_a, sl_b;

static void sl_pos(const song_plan_t *p, int bar, int *start, int *pass, int *bip, int *pbars, int *last)
{
    int i, k = -1, s, e, j;
    *start = -1;
    for (i = 0; i < p->n; i++)
        if (bar >= p->b[i].start && bar < p->b[i].start + p->b[i].bars) k = i;
    if (k < 0 || !lead_plays(&p->b[k])) return;
    for (i = k; i > 0 && lead_plays(&p->b[i - 1]); i--)
        ;
    s = p->b[i].start;
    for (i = k; i + 1 < p->n && lead_plays(&p->b[i + 1]); i++)
        ;
    e = p->b[i].start + p->b[i].bars;
    *last = 1;
    for (j = i + 1; j < p->n; j++)
        if (lead_plays(&p->b[j])) *last = 0;
    *start = s;
    *pass = (bar - s) / 8;
    *bip = (bar - s) % 8;
    *pbars = e - (s + *pass * 8) >= 8 ? 8 : 4;
}

/* a key request issued at sl_req_bar (step 0), landing on the next 4-bar line; the lead's state wiped at sl_wipe_bar
   (step 0) to what a hybrid lead leaves (no anchor, its last skeleton note): -1 none */
static int sl_req_bar = -1, sl_req_key, sl_wipe_bar = -1;

static void sl_play(const song_roll_t *r, sl_song_t *o)
{
    static song_t s;
    song_ev_t ev[64];
    const lib_prog_t *pr = lib_prog_by_id((uint8_t)r->v[RF_PROG]);
    int loop = pr && pr->n * pr->bars_per_chord ? pr->n * pr->bars_per_chord : 4, i, cur[128];
    memset(o, 0, sizeof *o);
    for (i = 0; i < 128; i++) cur[i] = -1;
    song_start(&s, r);
    o->bars = s.plan.bars;
    while (s.state == SS_PLAYING) {
        int bar = s.bar, st = s.step, hp[5];
        const plan_block_t *b = plan_at(&s.plan, bar);
        int n;
        if (st == 0 && bar == sl_req_bar) song_request(&s, RF_KEY, sl_req_key);
        if (st == 0 && bar == sl_wipe_bar) {
            memset(&s.lead, 0, sizeof s.lead);
            s.lead.last = 50;
        }
        n = song_tick(&s, ev, 64);
        for (i = 0; i < n && bar < 80; i++)
            if (ev[i].voice == V_CHORD && ev[i].note > o->ktop[bar]) o->ktop[bar] = ev[i].note;
        if (st != 0 || bar >= 80) continue;
        (void)n;
        o->lead[bar] = lead_plays(b);
        o->np[bar] = harmony_chord_at(&s.roll, bar, bar / loop + 1, 0, o->pcs[bar]);
        harmony_chord_at(&s.roll, bar - bar % loop, bar / loop + 1, 0, hp);
        o->home[bar] = hp[0] % 12 == o->pcs[bar][0] % 12;
        sl_pos(&s.plan, bar, &o->run[bar], &o->pass[bar], &o->bip[bar], &o->pbars[bar], &o->last[bar]);
        o->on0[bar] = s.lead.on[0] > 0;
        for (i = 0; i < 16; i++)
            if (s.lead.on[i] > 0 && o->n < SL_MAXN) {
                sl_note_t *x = &o->nt[o->n++];
                x->start = bar * 16 + i;
                x->end = x->start + s.lead.len[i];
                x->note = s.lead.on[i];
                x->vel = s.lead.vel[i];
            }
    }
}

static int sl_first(const sl_song_t *o, int t)   /* the first note starting at or after t */
{
    int i;
    for (i = 0; i < o->n; i++)
        if (o->nt[i].start >= t) return i;
    return o->n;
}

static int sl_iv(const sl_song_t *o, int bar, int note)   /* the note's interval over bar's chord root */
{
    return ((note - o->pcs[bar][0]) % 12 + 12) % 12;
}

static int sl_has_maj7(const sl_song_t *o, int bar)
{
    int j;
    for (j = 1; j < o->np[bar]; j++)
        if (((o->pcs[bar][j] - o->pcs[bar][0]) % 12 + 12) % 12 == 11) return 1;
    return 0;
}

/* the chord's 3rd and 5th both rub another chord tone a semitone away (or the 5th is left out): the question takes
   another tone than the root */
static int sl_no_q(const sl_song_t *o, int bar)
{
    int j, i, free = 0;
    for (j = 1; j < o->np[bar]; j++) {
        int iv = ((o->pcs[bar][j] - o->pcs[bar][0]) % 12 + 12) % 12, rub = 0;
        if (!(iv == 3 || iv == 4 || (iv >= 6 && iv <= 8))) continue;
        for (i = 0; i < o->np[bar]; i++) {
            int d = ((o->pcs[bar][i] - o->pcs[bar][j]) % 12 + 12) % 12;
            rub |= d == 1 || d == 11;
        }
        free |= !rub;
    }
    return !free;
}

static int sl_sgn(int x) { return x > 0 ? 1 : x < 0 ? -1 : 0; }

/* Roll a song pinned to set; 0 if the set isn't legal in the seed's mood. */
static int sl_roll(uint32_t seed, int set, int motion, song_roll_t *r)
{
    roll_song(NULL, seed, r);
    if (roll_set(r, RF_SET, set) != set) return 0;
    if (motion >= 0) roll_set(r, RF_MOTION, motion);
    return 1;
}

static void test_sustained_phrases(void)
{
    long phrases = 0, reversed = 0, rest_ok = 0, rhythm_bad = 0, contour_same = 0, q_bad = 0, a_bad = 0, breath_bad = 0;
    long halves = 0, half_bad = 0, onsets = 0, leadbars = 0, starts = 0, off1 = 0, leaps = 0, backs = 0, b1_bad = 0;
    long lens_same = 0, a_third = 0, arrivals = 0, questions = 0, songs = 0, varied = 0, steps = 0;
    long wide = 0;
    int k, got;
    uint32_t seed;
    for (k = 0; k < SL_NSETS; k++)
        for (seed = 1u, got = 0; got < 14 && seed < 900u; seed++) {
            song_roll_t r;
            sl_song_t *o = &sl_a;
            int bar, i;
            if (!sl_roll(seed, SL_SETS[k], -1, &r)) continue;
            got++;
            sl_play(&r, o);
            {   /* the line's variety (4+ distinct notes), and its leaps past a 5th within a gesture */
                int seen[128] = {0}, distinct = 0;
                for (i = 0; i < o->n; i++) {
                    if (!seen[o->nt[i].note]++) distinct++;
                    if (i && o->nt[i].start - o->nt[i - 1].end <= 2) {
                        int d = o->nt[i].note - o->nt[i - 1].note;
                        steps++;
                        wide += d > 7 || d < -7;
                    }
                }
                if (o->n) { songs++; varied += distinct >= 4; }
            }
            for (bar = 0; bar < o->bars && bar < 80; bar++) {
                int P = bar * 16, j, end, nx, lb;
                if (!o->lead[bar]) continue;
                leadbars++;
                for (j = 0; j < o->n; j++) onsets += o->nt[j].start >= P && o->nt[j].start < P + 16;
                b1_bad += o->on0[bar];                   /* beat 1 struck */
                if (o->bip[bar] != 0) continue;
                /* a phrase (or half a phrase) starts here */
                starts++;
                j = sl_first(o, P);
                off1 += j < o->n && o->nt[j].start != P;
                for (i = j; i + 2 < o->n && o->nt[i + 2].start < P + o->pbars[bar] * 16; i++) {
                    int d1 = o->nt[i + 1].note - o->nt[i].note, d2 = o->nt[i + 2].note - o->nt[i + 1].note;
                    if (o->nt[i + 1].start - o->nt[i].end > 2 || o->nt[i + 2].start - o->nt[i + 1].end > 2 ||
                        (d1 < 5 && d1 > -5))
                        continue;                        /* a leap within a gesture (no rest after it) */
                    leaps++;
                    backs += sl_sgn(d2) == -sl_sgn(d1) && d2 >= -2 && d2 <= 2;
                }
                if (o->pbars[bar] == 8) {
                    int a0 = j, a1 = sl_first(o, P + 32), b0 = sl_first(o, P + 64), b1 = sl_first(o, P + 96), same = 1;
                    phrases++;
                    if (a1 - a0 != b1 - b0) rhythm_bad++;
                    else {
                        int lsame = 1;
                        for (i = 0; i < a1 - a0; i++) {
                            if (o->nt[a0 + i].start - P != o->nt[b0 + i].start - P - 64) {
                                rhythm_bad++;
                                same = 0;
                                break;
                            }
                            lsame &= o->nt[a0 + i].end - o->nt[a0 + i].start == o->nt[b0 + i].end - o->nt[b0 + i].start;
                            if (i && sl_sgn(o->nt[a0 + i].note - o->nt[a0 + i - 1].note) !=
                                         sl_sgn(o->nt[b0 + i].note - o->nt[b0 + i - 1].note))
                                same = 0, reversed += sl_sgn(o->nt[a0 + i].note - o->nt[a0 + i - 1].note) ==
                                    -sl_sgn(o->nt[b0 + i].note - o->nt[b0 + i - 1].note) && o->nt[a0 + i].note !=
                                    o->nt[a0 + i - 1].note;
                        }
                        lens_same += lsame;
                        contour_same += same;
                    }
                }
                /* the question (bar 4) and the arrival (bar 8), or the half phrase's end */
                for (lb = 3; lb < o->pbars[bar]; lb += 4) {
                    int qb = bar + lb, ans = lb == 7 || (o->pbars[bar] == 4 && o->last[bar]), iv;
                    j = sl_first(o, (qb + 1) * 16) - 1;
                    if (o->pbars[bar] == 4) halves++;
                    if (j < 0 || o->nt[j].start < P) { if (ans) a_bad++; else q_bad++; continue; }
                    end = o->nt[j].end;
                    iv = sl_iv(o, qb, o->nt[j].note);
                    nx = j + 1 < o->n ? o->nt[j + 1].start : 1 << 20;
                    if (nx - end < 8 || end > (qb + 1) * 16 - 8) { breath_bad++; if (o->pbars[bar] == 4) half_bad++; }
                    else rest_ok++;
                    if (ans) {
                        arrivals++;
                        a_third += iv == 3 || iv == 4;
                        if (!(iv == 0 || ((iv == 3 || iv == 4) && (o->home[qb] || sl_has_maj7(o, qb)))) ||
                            end - o->nt[j].start < 8) {
                            a_bad++;
                            if (o->pbars[bar] == 4) half_bad++;
                        }
                    } else {
                        questions++;
                        if (!(iv == 3 || iv == 4 || (iv >= 6 && iv <= 8) || (iv && sl_no_q(o, qb))) ||
                            end - o->nt[j].start < 8) {
                            q_bad++;
                            if (o->pbars[bar] == 4) half_bad++;
                        }
                    }
                }
            }
        }
    printf("sustained: %ld phrases, restated rhythm broken %ld, contour kept %ld (%.1f%%), lengths kept %ld; questions "
           "%ld (%ld broken), arrivals %ld (%ld broken, %ld on the 3rd), %ld breaths kept, %ld broken; %ld half phrases "
           "(%ld broken)\n", phrases, rhythm_bad, contour_same, phrases ? 100.0 * (double)contour_same / (double)phrases
           : 0.0, lens_same, questions, q_bad, arrivals, a_bad, a_third, rest_ok, breath_bad, halves, half_bad);
    printf("sustained: %.2f onsets a lead bar; phrases off beat 1 %ld of %ld; leaps (a 4th+) answered by a step back "
           "%ld of %ld; beat 1 struck in %ld lead bars\n", leadbars ? (double)onsets / (double)leadbars : 0.0, off1,
           starts, backs, leaps, b1_bad);
    printf("sustained: %ld of %ld songs sing 4+ distinct notes; %ld of %ld steps within a gesture leap past a 5th\n",
           varied, songs, wide, steps);
    CHECK(songs > 100 && varied * 10 >= songs * 7, "the line sings 4+ notes in %ld of %ld songs", varied, songs);
    CHECK(wide * 1000 <= steps * 3, "%ld of %ld sung steps leap past a 5th", wide, steps);
    CHECK(phrases > 100 && rhythm_bad == 0, "restatements: rhythm broken in %ld of %ld", rhythm_bad, phrases);
    /* beat 1 struck more often (Sam) snaps more restated notes to chord tones: a step may become a repeat where the
       step would rub, but the line never turns the other way */
    CHECK(contour_same * 100 >= phrases * 85, "restatements: contour kept in %ld of %ld (want >= 85%%)", contour_same,
          phrases);
    CHECK(reversed == 0, "restatements: %ld steps turned the other way", reversed);
    CHECK(questions > 100 && q_bad == 0, "questions: %ld of %ld not a held 3rd or 5th", q_bad, questions);
    CHECK(arrivals > 100 && a_bad == 0, "arrivals: %ld of %ld not a held stable tone", a_bad, arrivals);
    CHECK(breath_bad == 0, "%ld questions or arrivals without half a bar's breath after", breath_bad);
    CHECK(half_bad == 0, "%ld of %ld half phrases broken", half_bad, halves);
    CHECK(leadbars > 500 && onsets * 2 <= leadbars * 7, "%ld onsets in %ld lead bars (want a mean <= 3.5)", onsets,
          leadbars);
    CHECK(off1 * 100 >= starts * 25 && off1 * 100 <= starts * 55, "phrases off beat 1 in %ld of %ld (want 25-55%%)",
          off1, starts);
    CHECK(leaps > 0 && backs * 10 >= leaps * 7, "leaps answered by a step back %ld of %ld (want >= 70%%)", backs, leaps);
    CHECK(b1_bad * 100 >= leadbars * 55 && b1_bad * 100 <= leadbars * 85, "beat 1 struck in %ld of %ld lead bars "
          "(want 55-85%%)", b1_bad, leadbars);
}

/* A full plan (24 blocks, the most a plan holds): the lead reads each block's next neighbor, up to the zero block
   after the last (song_plan_t's b[24]); every bar plans, and the run's 6 phrases each sing. */
static void test_lead_full_plan(void)
{
    static song_plan_t p;
    static lead_state_t ls;
    song_roll_t r;
    int i, bar, onsets = 0, phrases = 0, inph = 0, keys[4] = {48, 52, 55, 60};
    CHECK(sl_roll(2u, 7, 70, &r), "full plan: seed 2 takes set 7");
    memset(&p, 0, sizeof p);
    memset(&ls, 0, sizeof ls);
    p.n = 24;
    p.bars = 48;
    for (i = 0; i < 24; i++) {
        p.b[i].start = (uint8_t)(2 * i);
        p.b[i].bars = 2;
        p.b[i].section = SEC_LEAD;
        p.b[i].layers = L_KEYS | L_LEAD;
    }
    CHECK(p.b[24].bars == 0 && sizeof p.b / sizeof p.b[0] == 25, "full plan: no zero block after the 24th");
    for (bar = 0; bar < 48; bar++) {
        int pcs[5], np = harmony_chord_at(&r, bar, 1, 0, pcs), n;
        n = lead_plan_bar(&ls, &r, &p.b[bar / 2], bar, keys, keys, pcs, np, bar + 1 < 48);
        onsets += n;
        inph += n;
        if (bar % 8 == 7) { phrases += inph > 0; inph = 0; }
    }
    printf("full plan: %d lead onsets over 48 bars of 24 blocks\n", onsets);
    CHECK(onsets > 48 && phrases == 6, "full plan: %d onsets, %d phrases", onsets, phrases);
}

/* Motion by rank (the design's call 5): the same seed at motion 30 and 70; where the phrase is the same (the same
   run, pass and position: the plan may move lead blocks with motion), the low-motion onsets are a subset of the
   high-motion ones, and per phrase fewer onsets, the sounding time within 10% (Sam: fewer notes, held longer). */
static int sl_same_phrase(const sl_song_t *a, const sl_song_t *b, int bar)
{
    return a->lead[bar] && b->lead[bar] && a->run[bar] == b->run[bar] && a->pass[bar] == b->pass[bar] &&
           a->pbars[bar] == b->pbars[bar] && a->last[bar] == b->last[bar];
}

static void test_sustained_motion(void)
{
    long bars = 0, not_sub = 0, ph = 0, fewer = 0, more = 0, sound_bad = 0, on_lo = 0, on_hi = 0;
    double snd_lo = 0.0, snd_hi = 0.0;
    int k, got;
    uint32_t seed;
    for (k = 0; k < SL_NSETS; k++)
        for (seed = 1u, got = 0; got < 10 && seed < 900u; seed++) {
            song_roll_t ra, rb;
            int bar;
            if (!sl_roll(seed, SL_SETS[k], 30, &ra) || !sl_roll(seed, SL_SETS[k], 70, &rb)) continue;
            got++;
            sl_play(&ra, &sl_a);
            sl_play(&rb, &sl_b);
            for (bar = 0; bar < sl_a.bars && bar < 80; bar++) {
                int i, j, ok = 1, P = bar * 16, nb, b2, cl = 0, ch = 0, sl = 0, sh = 0;
                if (!sl_same_phrase(&sl_a, &sl_b, bar)) continue;
                bars++;
                for (i = 0; i < sl_a.n; i++) {
                    int found = 0;
                    if (sl_a.nt[i].start < P || sl_a.nt[i].start >= P + 16) continue;
                    for (j = 0; j < sl_b.n; j++) found |= sl_b.nt[j].start == sl_a.nt[i].start;
                    if (!found) ok = 0;
                }
                not_sub += !ok;
                if (sl_a.bip[bar] != 0) continue;
                nb = sl_a.pbars[bar];
                for (b2 = bar; b2 < bar + nb; b2++)
                    if (b2 >= 80 || !sl_same_phrase(&sl_a, &sl_b, b2)) break;
                if (b2 < bar + nb) continue;
                for (i = 0; i < sl_a.n; i++)
                    if (sl_a.nt[i].start >= P && sl_a.nt[i].start < P + nb * 16) {
                        cl++;
                        sl += sl_a.nt[i].end - sl_a.nt[i].start;
                    }
                for (i = 0; i < sl_b.n; i++)
                    if (sl_b.nt[i].start >= P && sl_b.nt[i].start < P + nb * 16) {
                        ch++;
                        sh += sl_b.nt[i].end - sl_b.nt[i].start;
                    }
                ph++;
                fewer += cl < ch;
                more += cl > ch;
                on_lo += cl;
                on_hi += ch;
                snd_lo += sl;
                snd_hi += sh;
                if (sl * 10 < sh * 9 || sl * 10 > sh * 11) sound_bad++;
            }
        }
    printf("sustained motion: %ld bars compared, %ld not a subset; %ld phrases, low motion fewer onsets in %ld, more in "
           "%ld (%ld against %ld); sounding %.0f against %.0f 16ths, %ld phrases off by more than 10%%\n", bars,
           not_sub, ph, fewer, more, on_lo, on_hi, snd_lo, snd_hi, sound_bad);
    CHECK(bars > 300 && not_sub == 0, "low-motion onsets not a subset of high-motion in %ld of %ld bars", not_sub, bars);
    CHECK(ph > 50 && more == 0 && on_lo < on_hi, "low motion: more onsets in %ld of %ld phrases (%ld against %ld)",
          more, ph, on_lo, on_hi);
    CHECK(sound_bad == 0, "low motion's sounding time off by more than 10%% in %ld of %ld phrases", sound_bad, ph);
}

/* the mean pitch of o's notes starting in bars [b0, b1); 0 if none */
static double sl_mean(const sl_song_t *o, int b0, int b1)
{
    int i, k = 0;
    double sum = 0.0;
    for (i = 0; i < o->n; i++)
        if (o->nt[i].start >= b0 * 16 && o->nt[i].start < b1 * 16) { sum += o->nt[i].note; k++; }
    return k ? sum / k : 0.0;
}

/* the first 8-bar phrase's start on a 4-bar line (bip 0, 8 bars), -1 none */
static int sl_phrase8(const sl_song_t *o)
{
    int bar;
    for (bar = 0; bar + 8 < o->bars && bar + 8 < 80; bar++)
        if (o->lead[bar] && o->bip[bar] == 0 && o->pbars[bar] == 8 && bar % 4 == 0) return bar;
    return -1;
}

/* Final review fixes. A key request landing mid-phrase (the answer, bip 4) resumes the phrase (call 1): the answer
   moves with the key (the wrapped interval, -6..5, as the motif does), never by an octave where the key crosses C
   (B to C, C to B, D down to B). And a lead that starts singing mid-phrase with no anchor yet (a set change from a
   percussive lead) places one: its answer sits where the unbroken phrase's does. */
static void test_sustained_key_request(void)
{
    static const int from[3] = {11, 0, 2}, to[3] = {0, 11, 11}, dk[3] = {1, -1, -3};
    long n[3] = {0, 0, 0}, far[3] = {0, 0, 0}, revoiced[3] = {0, 0, 0}, nw = 0, wfar = 0;
    double err[3] = {0, 0, 0}, worst[3] = {0, 0, 0}, werr = 0.0, wworst = 0.0;
    int k, c, got;
    uint32_t seed;
    for (c = 0; c < 3; c++)
        for (k = 0; k < SL_NSETS; k++)
            for (seed = 1u, got = 0; got < 8 && seed < 900u; seed++) {
                song_roll_t r;
                int S, i;
                double e;
                if (!sl_roll(seed, SL_SETS[k], -1, &r)) continue;
                roll_set(&r, RF_KEY, from[c]);
                sl_req_bar = sl_wipe_bar = -1;
                sl_play(&r, &sl_a);
                S = sl_phrase8(&sl_a);
                if (S < 0) continue;
                sl_req_bar = S + 1;                      /* lands on S + 4: the answer */
                sl_req_key = to[c];
                sl_play(&r, &sl_b);
                sl_req_bar = -1;
                if (!sl_b.lead[S + 4] || sl_b.run[S + 4] != S || sl_b.bip[S + 4] != 4 || sl_b.pbars[S + 4] != 8 ||
                    sl_mean(&sl_a, S + 4, S + 8) == 0.0 || sl_mean(&sl_b, S + 4, S + 8) == 0.0)
                    continue;
                for (i = S + 4; i < S + 8; i++)          /* the keys moved with the key: the lead's floor did too */
                    if (sl_b.ktop[i] - sl_a.ktop[i] - dk[c] > 2 || sl_b.ktop[i] - sl_a.ktop[i] - dk[c] < -2) break;
                if (i < S + 8) { revoiced[c]++; continue; }
                got++;
                n[c]++;
                e = sl_mean(&sl_b, S + 4, S + 8) - sl_mean(&sl_a, S + 4, S + 8) - dk[c];
                if (e < 0) e = -e;
                err[c] += e;
                if (e > worst[c]) worst[c] = e;
                far[c] += e > 3.0;
                if (c == 0) {                            /* the lead's state wiped at the answer: it re-anchors */
                    sl_wipe_bar = S + 4;
                    sl_play(&r, &sl_b);
                    sl_wipe_bar = -1;
                    if (!sl_b.lead[S + 4] || sl_mean(&sl_b, S + 4, S + 8) == 0.0) continue;
                    e = sl_mean(&sl_b, S + 4, S + 8) - sl_mean(&sl_a, S + 4, S + 8);
                    if (e < 0) e = -e;
                    nw++;
                    werr += e;
                    if (e > wworst) wworst = e;
                    wfar += e > 3.0;
                }
            }
    for (c = 0; c < 3; c++) {
        printf("key request at the answer, %d to %d: %ld songs, the answer off the key's move by %.2f semitones on "
               "average (worst %.1f), %ld by more than 3 (%ld more where the keys revoiced in another octave)\n",
               from[c], to[c], n[c], n[c] ? err[c] / (double)n[c] : 0.0, worst[c], far[c], revoiced[c]);
        CHECK(n[c] > 30 && err[c] <= 1.5 * (double)n[c] && far[c] * 10 <= n[c],
              "key request %d to %d: the answer moved %.2f semitones off the key's move on average, %ld of %ld by 3+",
              from[c], to[c], n[c] ? err[c] / (double)n[c] : 0.0, far[c], n[c]);
    }
    printf("lead wiped at the answer: %ld songs, the answer off by %.2f semitones on average (worst %.1f), %ld by more "
           "than 3\n", nw, nw ? werr / (double)nw : 0.0, wworst, wfar);
    CHECK(nw > 30 && werr <= 3.0 * (double)nw, "wiped lead: the answer off by %.2f semitones on average",
          nw ? werr / (double)nw : 0.0);
}

/* Final review fix: a cadence planned across the bar line takes the next bar's chord and its home. In i-iv-V-i (prog
   23) the arrival lands on the home i (bar 8) from a note struck over the V (bar 7): it aims a step from where the
   arrival lands, which over home may be the i's 3rd, so an arrival on the 3rd is stepped into (2 semitones or less)
   at least half the time (a third of the time when the step aimed as if bar 7's V were the arrival's chord). */
static void test_sustained_tied_home(void)
{
    long arr = 0, thirds = 0, far = 0, tfar = 0, songs = 0;
    int k;
    uint32_t seed;
    for (k = 0; k < SL_NSETS; k++)
        for (seed = 1u; seed < 3000u; seed++) {
            song_roll_t r;
            int bar, i;
            if (!sl_roll(seed, SL_SETS[k], -1, &r) || roll_set(&r, RF_PROG, 23) != 23) continue;
            songs++;
            sl_play(&r, &sl_a);
            for (bar = 1; bar < sl_a.bars && bar < 80; bar++) {
                if (!sl_a.lead[bar] || sl_a.bip[bar] != 7 || !sl_a.home[bar] || sl_a.home[bar - 1]) continue;
                for (i = 1; i < sl_a.n; i++)
                    if (sl_a.nt[i].start == bar * 16 && sl_a.nt[i - 1].start >= bar * 16 - 4 &&
                        sl_a.nt[i - 1].end >= bar * 16 - 2) {   /* the arrival on 1, stepped into from bar 7 */
                        int iv = sl_iv(&sl_a, bar, sl_a.nt[i].note), d = sl_a.nt[i].note - sl_a.nt[i - 1].note;
                        arr++;
                        thirds += iv == 3 || iv == 4;
                        far += d > 2 || d < -2;
                        tfar += (iv == 3 || iv == 4) && (d > 2 || d < -2);
                    }
            }
        }
    printf("tied home: %ld prog-23 songs, %ld arrivals on the home i stepped into from the V, %ld on its 3rd, %ld "
           "after more than a step (%ld on the 3rd)\n", songs, arr, thirds, far, tfar);
    CHECK(thirds > 100 && tfar * 2 <= thirds, "tied home: %ld of %ld arrivals on the home 3rd not stepped into",
          tfar, thirds);
}

/* ---- the pad layer (pads design, 2026-10-24): a mid-range chord over percussive keys and a high solo note ----
   Rolled songs played through song_tick; every pitched note recorded with its sample span and, on V_PAD, its lane
   (1-3 the chord part, 4 the solo note). */
#define PD_BARS 76
#define PD_MAXE 12000
typedef struct { uint32_t start, end; int bar; uint8_t voice, note, lane; } pd_ev_t;
typedef struct {
    int bars, ne, first[PD_BARS + 1];                    /* first: each bar's first event */
    uint32_t at[PD_BARS * 16 + 1];                       /* each step's start sample */
    int layers[PD_BARS], sec[PD_BARS], blk[PD_BARS], key[PD_BARS], mode[PD_BARS], np[PD_BARS];
    int part[PD_BARS], sus[PD_BARS], voice[PD_BARS];     /* pads_section_part's, sustained keys, the solo's voice */
    int pcs[PD_BARS][5];
    int keys[PD_BARS][4];                                /* the bar's full keys voicing (harmony_voicing_at) */
    uint32_t hash;                                       /* the pad events */
    pd_ev_t ev[PD_MAXE];
} pd_rec_t;
static pd_rec_t pd_a, pd_b;
static int pd_req_bar = -1, pd_req_key;                  /* a key request at this bar's step 3 (-1: none) */

static void pd_play(song_t *s, pd_rec_t *rec, song_t *other)
{
    song_ev_t ev[64], oev[64];
    uint32_t pos = 0;
    float frac = 0.0f;
    int t, i;
    rec->ne = 0;
    rec->hash = 2166136261u;
    for (t = 0; s->state == SS_PLAYING && t < PD_BARS * 16; t++) {
        int bar = s->bar, step = s->step, n;
        float adv;
        if (pd_req_bar == bar && step == 3) song_request(s, RF_KEY, pd_req_key);
        if (other && other->state == SS_PLAYING) song_tick(other, oev, 64);
        n = song_tick(s, ev, 64);
        if (step == 0) {                                 /* the roll and plan as they play this bar (a request lands on 0) */
            const plan_block_t *b = plan_at(&s->plan, bar);
            const lib_prog_t *pr = lib_prog_by_id((uint8_t)s->roll.v[RF_PROG]);
            int loop = pr ? pr->n * pr->bars_per_chord : 4;
            rec->first[bar] = rec->ne;
            rec->layers[bar] = b ? b->layers : 0;
            rec->sec[bar] = b ? b->section : -1;
            rec->blk[bar] = b ? (int)(b - s->plan.b) : -1;
            {
                const lib_set_t *set = lib_set_by_id((uint8_t)s->roll.v[RF_SET]);
                rec->sus[bar] = !set || lib_ins_sustained(set->chords);
            }
            rec->part[bar] = b ? pads_section_part(&s->roll, rec->blk[bar], b->section, rec->sus[bar]) : 0;
            rec->voice[bar] = pads_solo_voice(&s->roll);
            rec->key[bar] = s->roll.v[RF_KEY];
            rec->mode[bar] = s->roll.v[RF_MODE];
            rec->np[bar] = harmony_chord_at(&s->roll, bar, bar / loop + 1, 0, rec->pcs[bar]);
            {
                harmony_voicing_t vs = s->voicing;
                harmony_voicing_at(&s->roll, bar, bar / loop + 1, &vs, rec->keys[bar]);
            }
        }
        rec->at[t] = pos;
        for (i = 0; i < n; i++) {
            pd_ev_t *e;
            if (ev[i].voice > V_PAD || rec->ne >= PD_MAXE) continue;
            e = &rec->ev[rec->ne++];
            e->start = pos + ev[i].delay;
            e->end = e->start + ev[i].len;
            e->bar = bar;
            e->voice = ev[i].voice;
            e->note = ev[i].note;
            e->lane = ev[i].voice == V_PAD ? ev[i].pad : 0;
            if (ev[i].voice == V_PAD) {
                uint32_t h = rec->hash;
                h = (h ^ (uint32_t)t) * 16777619u;
                h = (h ^ (uint32_t)(ev[i].note | ev[i].pad << 8 | ev[i].vel << 16)) * 16777619u;
                rec->hash = (h ^ ev[i].len) * 16777619u;
            }
        }
        adv = song_step_samples(s) + frac;
        frac = adv - (float)(uint32_t)adv;
        pos += (uint32_t)adv;
    }
    rec->bars = t / 16;
    rec->at[t] = pos;
    rec->first[rec->bars] = rec->ne;
}

/* the notes of lane lo..hi sounding at sample t (their count; out ascending by pitch) */
static int pd_sounding(const pd_rec_t *rec, int bar, uint32_t t, int lo, int hi, int *out)
{
    int k, n = 0, from = bar >= 4 ? rec->first[bar - 4] : 0, to = bar + 1 < rec->bars ? rec->first[bar + 1] : rec->ne;
    int lane[4] = {0, 0, 0, 0};
    for (k = from; k < to; k++) {
        const pd_ev_t *e = &rec->ev[k];
        if (e->voice == V_PAD && e->lane >= lo && e->lane <= hi && e->start <= t && e->end > t) lane[e->lane - 1] = e->note;
    }
    for (k = lo; k <= hi; k++)
        if (lane[k - 1]) {                               /* ascending (lanes keep common tones: not in pitch order) */
            int j = n++;
            for (; j > 0 && out[j - 1] > lane[k - 1]; j--) out[j] = out[j - 1];
            out[j] = lane[k - 1];
        }
    return n;
}

/* a semitone rub between two sounding notes: the upper a minor 2nd (or 9th) above the lower; over the bass, a major
   7th above it too unless the bass is the chord's root (clash_test's bass rule) */
static int pd_rub(int a, int b) { int d = a > b ? a - b : b - a; return d % 12 == 1; }
static int pd_rub_bass(int pad, int bass, int root)
{
    int d = ((pad - bass) % 12 + 12) % 12;
    return d == 1 || (d == 11 && bass % 12 != root);
}

/* rubs of the pad events of lanes lo..hi against overlapping keys, bass and (with_lead) lead notes */
static long pd_rubs(const pd_rec_t *rec, int lo, int hi, int with_lead, long *checked)
{
    long bad = 0;
    int k, j;
    for (k = 0; k < rec->ne; k++) {
        const pd_ev_t *e = &rec->ev[k];
        int from, to;
        if (e->voice != V_PAD || e->lane < lo || e->lane > hi) continue;
        from = e->bar >= 2 ? rec->first[e->bar - 2] : 0;
        for (to = e->bar; to < rec->bars && rec->at[to * 16] < e->end; to++) {}
        to = to < rec->bars ? rec->first[to] : rec->ne;
        for (j = from; j < to; j++) {
            const pd_ev_t *x = &rec->ev[j];
            int r;
            if (x->start >= e->end || x->end <= e->start) continue;
            if (x->voice == V_CHORD || (with_lead && x->voice == V_LEAD)) r = pd_rub(e->note, x->note);
            else if (x->voice == V_BASS) r = pd_rub_bass(e->note, x->note, rec->pcs[x->bar][0]);
            else continue;
            (*checked)++;
            if (r) {
                if (bad < 5)
                    printf("  pad rub: lane %d note %d bar %d against %s %d (bar %d)\n", e->lane, e->note, e->bar,
                           x->voice == V_CHORD ? "keys" : x->voice == V_BASS ? "bass" : "lead", x->note, x->bar);
                bad++;
            }
        }
    }
    return bad;
}

static int pd_bpc(const song_roll_t *r)
{
    const lib_prog_t *pr = lib_prog_by_id((uint8_t)r->v[RF_PROG]);
    return pr && pr->bars_per_chord ? pr->bars_per_chord : 1;
}

/* Present in every keys bar of a chord section (pads_section_part 1) over percussive keys, absent over sustained keys; 2 notes (the budget's cut (c), pads
   design call 1), never 3 or fewer than 2, the 3rd and the 7th in >= 80% of the chords that have both; onsets only at chord changes; the top voice moves 2 semitones or less in >= 80% of changes; MIDI
   55-72; no note a semitone from a sounding keys, bass or lead note */
static void test_pad_chord(void)
{
    static song_t s;
    song_roll_t prev, r;
    uint32_t seed = 9191;
    long chords = 0, three = 0, two = 0, few = 0, missing = 0, over_sus = 0, off_change = 0, range = 0, changes = 0,
         smooth = 0, rubs = 0, checked = 0, songs_perc = 0, common = 0, lane_moved = 0, restruck = 0, guide = 0,
         both = 0, moves = 0, vsmooth = 0, lchanges = 0, released = 0, smear = 0, swell = 0, glides = 0, wide = 0;
    int k, bar, i;
    for (k = 0; k < 500; k++) {
        const lib_set_t *set;
        int sus, bpc, top_prev = 0, pv[4], npv = 0;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        roll_song(k ? &prev : 0, seed, &r);
        prev = r;
        set = lib_set_by_id((uint8_t)r.v[RF_SET]);
        sus = set && lib_ins_sustained(set->chords);
        bpc = pd_bpc(&r);
        songs_perc += !sus;
        song_start(&s, &r);
        pd_play(&s, &pd_a, NULL);
        for (i = 0; i < pd_a.ne; i++) {
            const pd_ev_t *e = &pd_a.ev[i];
            if (e->voice != V_PAD || e->lane < 1 || e->lane > 3) continue;
            if (sus) over_sus++;
            if (e->start != pd_a.at[e->bar * 16] || e->bar % bpc) off_change++;
            if (e->note < 55 || e->note > 72) range++;
        }
        for (bar = 0; bar < pd_a.bars; bar++) {
            int notes[4], n;
            if (sus || !(pd_a.layers[bar] & L_KEYS) || pd_a.part[bar] != 1) { top_prev = npv = 0; continue; }
            n = pd_sounding(&pd_a, bar, pd_a.at[bar * 16], 1, 3, notes);
            if (n == 0) { missing++; top_prev = npv = 0; continue; }
            if (bar % bpc) continue;
            if (bar > 0 && (pd_a.layers[bar - 1] & L_KEYS)) {   /* a common pitch rings on, on its lane */
                int l, j, was[4], now[4];
                for (l = 1; l <= 3; l++) {
                    if (!pd_sounding(&pd_a, bar, pd_a.at[bar * 16] - 1, l, l, was)) continue;
                    for (j = 1; j <= 3; j++)
                        if (pd_sounding(&pd_a, bar, pd_a.at[bar * 16], j, j, now) && now[0] == was[0]) break;
                    if (j > 3) continue;
                    common++;
                    lane_moved += j != l;
                    if (bar % 4)                         /* (a 4-bar line cuts every note: the same pitch again on
                                                            the same lane extends it) */
                        for (i = pd_a.first[bar]; i < (bar + 1 < pd_a.bars ? pd_a.first[bar + 1] : pd_a.ne); i++)
                            restruck += pd_a.ev[i].voice == V_PAD && pd_a.ev[i].lane == l && pd_a.ev[i].note == was[0];
                }
            }
            if (bar > 0 && (pd_a.layers[bar - 1] & L_KEYS)) {   /* by lane: a step glides on its own lane */
                int l, was[4] = {0, 0, 0, 0}, on[4] = {0, 0, 0, 0}, held[4] = {0, 0, 0, 0}, x[4];
                uint32_t t0 = pd_a.at[bar * 16];
                for (l = 1; l <= 3; l++) {
                    if (pd_sounding(&pd_a, bar, t0 - 1, l, l, x)) was[l] = x[0];
                    if (pd_sounding(&pd_a, bar, t0, l, l, x)) held[l] = x[0];
                }
                for (i = pd_a.first[bar]; i < (bar + 1 < pd_a.bars ? pd_a.first[bar + 1] : pd_a.ne); i++)
                    if (pd_a.ev[i].voice == V_PAD && pd_a.ev[i].lane >= 1 && pd_a.ev[i].lane <= 3 &&
                        pd_a.ev[i].start == t0 && pd_a.ev[i].note != was[pd_a.ev[i].lane])
                        on[pd_a.ev[i].lane] = pd_a.ev[i].note;   /* a new pitch on the lane (not an extension) */
                lchanges++;
                for (l = 1; l <= 3; l++) {
                    int m, near = 0, rub = 0;
                    if (was[l] && on[l]) {               /* a glide: the lane held, a new pitch on it */
                        glides++;
                        wide += on[l] - was[l] > 5 || was[l] - on[l] > 5;
                    }
                    if (!was[l] || held[l]) continue;
                    released++;                          /* the lane stops here, its tail under the new chord */
                    for (m = 1; m <= 3; m++) {
                        if (m == l || !on[m]) continue;
                        near |= on[m] - was[l] <= 4 && was[l] - on[m] <= 4;
                        rub |= pads_rub(on[m], was[l]);
                    }
                    swell += near;
                    smear += rub;
                }
            }
            chords++;
            {
                int h3 = 0, h7 = 0, g3 = 0, g7 = 0, j;
                for (j = 1; j < pd_a.np[bar]; j++) {
                    int iv = ((pd_a.pcs[bar][j] - pd_a.pcs[bar][0]) % 12 + 12) % 12;
                    h3 |= iv == 3 || iv == 4;
                    h7 |= iv == 10 || iv == 11;
                }
                for (j = 0; j < n; j++) {
                    int iv = ((notes[j] - pd_a.pcs[bar][0]) % 12 + 12) % 12;
                    g3 |= iv == 3 || iv == 4;
                    g7 |= iv == 10 || iv == 11;
                }
                guide += h3 && h7;
                both += h3 && h7 && g3 && g7;
            }
            three += n == 3;
            two += n == 2;
            few += n < 2;
            if (top_prev) {
                int d = notes[n - 1] - top_prev;
                changes++;
                smooth += d <= 2 && d >= -2;
            }
            if (npv) {                                   /* each voice: its move from the nearest note before */
                int j, q;
                for (j = 0; j < n; j++) {
                    int best = 99;
                    for (q = 0; q < npv; q++) {
                        int d = notes[j] - pv[q];
                        if (d < 0) d = -d;
                        if (d < best) best = d;
                    }
                    moves++;
                    vsmooth += best <= 2;
                }
            }
            memcpy(pv, notes, sizeof pv);
            npv = n;
            top_prev = notes[n - 1];
        }
        rubs += pd_rubs(&pd_a, 1, 3, 1, &checked);
    }
    printf("pad chord: %ld songs over percussive keys, %ld chords: 3 notes %ld (%.1f%%), 2 notes %ld, fewer %ld; top "
           "voice a step or less in %ld of %ld changes (%.1f%%); %ld rubs in %ld pairs checked\n", songs_perc, chords,
           three, chords ? 100.0 * (double)three / (double)chords : 0.0, two, few, smooth, changes,
           changes ? 100.0 * (double)smooth / (double)changes : 0.0, rubs, checked);
    CHECK(songs_perc > 100 && chords > 5000, "pad chord: only %ld songs, %ld chords", songs_perc, chords);
    CHECK(missing == 0, "pad chord: missing in %ld keys bars over percussive keys", missing);
    CHECK(over_sus == 0, "pad chord: %ld notes over sustained keys", over_sus);
    printf("pad chord: the 3rd and the 7th in %ld of %ld chords that have both; each voice a step or less in %ld of "
           "%ld moves (%.1f%%)\n", both, guide, vsmooth, moves, moves ? 100.0 * (double)vsmooth / (double)moves : 0.0);
    CHECK(three == 0 && few == 0, "pad chord: %ld chords of 3 notes, %ld under 2 (of %ld)", three, few, chords);
    CHECK(both * 100 >= guide * 80 && guide > 1000, "pad chord: the 3rd and 7th in only %ld of %ld", both, guide);
    CHECK(off_change == 0, "pad chord: %ld onsets off a chord change", off_change);
    CHECK(smooth * 100 >= changes * 80, "pad chord: top voice smooth in %ld of %ld changes", smooth, changes);
    CHECK(range == 0, "pad chord: %ld notes outside 55-72", range);
    CHECK(rubs == 0 && checked > 10000, "pad chord: %ld rubs with keys, bass or lead (%ld checked)", rubs, checked);
    printf("pad chord: %ld common pitches across chord changes, %ld on another lane, %ld struck again\n", common,
           lane_moved, restruck);
    printf("pad chord: %ld changes by lane: %ld lanes released (%ld a semitone from a new note), %ld with a new note "
           "4 semitones or less from it on another lane; %ld glides, %ld over 5 semitones\n", lchanges, released, smear,
           swell, glides, wide);
    CHECK(swell == 0 && smear == 0 && wide == 0 && glides > 5000, "pad chord: %ld step moves released and swelled on "
          "another lane, %ld released lanes a semitone from a new note, %ld glides over 5 semitones (of %ld)", swell,
          smear, wide, glides);
    CHECK(common > 5000 && lane_moved == 0 && restruck == 0, "pad chord: of %ld common pitches %ld changed lanes, %ld "
          "struck again", common, lane_moved, restruck);
}

static const int PD_DEG[4] = {0, 1, 2, 6};             /* the solo's scale degrees: 1, 2, 3, 7 */

static int pd_degree(int key, int mode, int note)       /* index into PD_DEG, -1 if another degree */
{
    uint16_t pool = th_mode_pool(mode);
    int off[7], i, n = 0, pc = ((note - key) % 12 + 12) % 12;
    for (i = 0; i < 12 && n < 7; i++)
        if ((pool >> i) & 1) off[n++] = i;
    for (i = 0; i < 4; i++)
        if (PD_DEG[i] < n && off[PD_DEG[i]] == pc) return i;
    return -1;
}

/* the solo note's lowest MIDI note by its voice: the strings an octave up (84-96), the others 72-84 */
static int pd_solo_lo(int voice) { return voice == INS_STRINGS ? 84 : 72; }

/* the solo note held over `bar` is legal there: its degree, range, at least 3 above the chord part's top, no avoid
   note, no rub with the keys' voicing or the bass */
static int pd_solo_legal(const pd_rec_t *rec, int bar, int note)
{
    int chord[4], n, k, to = bar + 1 < rec->bars ? rec->first[bar + 1] : rec->ne, lo = pd_solo_lo(rec->voice[bar]);
    if (pd_degree(rec->key[bar], rec->mode[bar], note) < 0 || note < lo || note > lo + 12) return 0;
    n = pd_sounding(rec, bar, rec->at[bar * 16], 1, 3, chord);
    if (n && note < chord[n - 1] + 3) return 0;
    if (th_is_avoid(rec->key[bar], rec->mode[bar], rec->pcs[bar], rec->np[bar], note)) return 0;
    for (k = 0; k < 4; k++)                              /* the bar's full voicing, as pads_solo judges it (a busy bar may strike less) */
        if (rec->keys[bar][k] && pd_rub(note, rec->keys[bar][k])) return 0;
    for (k = rec->first[bar]; k < to; k++) {
        const pd_ev_t *x = &rec->ev[k];
        if (x->voice == V_CHORD && pd_rub(note, x->note)) return 0;
        if (x->voice == V_BASS && pd_rub_bass(note, x->note, rec->pcs[bar][0])) return 0;
    }
    return 1;
}

/* Over 1500 songs (about 3 in 5 with a solo note, a quarter of their sections over struck keys): at most one onset a
   bar, on step 0, sounding to the bar's end (or tied); scale degrees 1, 2, 3, 7 only, the tonic in >= 35%; a note
   legal over the next chord is kept, otherwise it moves 2 semitones or less in >= 90%; MIDI 72-84 (the strings
   84-96), 3 or more above the chord part's top; no rubs with the keys or bass. A key request mid-section: no solo
   note sounds outside the new key after it lands. */
static void test_pad_solo(void)
{
    static song_t s;
    song_roll_t prev, r;
    uint32_t seed = 4747;
    long notes = 0, tonic = 0, bars = 0, btonic = 0, deg_bad = 0, twice = 0, off_step = 0, short_ = 0, range = 0,
         low = 0, kept = 0, keep_bad = 0, moved = 0, near = 0, rubs = 0, checked = 0, avoid = 0;
    int k, bar, i, found = 0;
    for (k = 0; k < 1500; k++) {
        int bpc, onsets[PD_BARS];
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        roll_song(k ? &prev : 0, seed, &r);
        prev = r;
        bpc = pd_bpc(&r);
        song_start(&s, &r);
        pd_play(&s, &pd_a, NULL);
        memset(onsets, 0, sizeof onsets);
        for (i = 0; i < pd_a.ne; i++) {
            const pd_ev_t *e = &pd_a.ev[i];
            int d, b2;
            if (e->voice != V_PAD || e->lane != 4) continue;
            notes++;
            onsets[e->bar]++;
            if (e->start != pd_a.at[e->bar * 16]) off_step++;
            for (b2 = e->bar + 1; b2 <= pd_a.bars && pd_a.at[b2 * 16] < e->end; b2++) {}
            if (b2 > pd_a.bars || pd_a.at[b2 * 16] != e->end) short_++;   /* ends on a bar line */
            d = pd_degree(pd_a.key[e->bar], pd_a.mode[e->bar], e->note);
            deg_bad += d < 0;
            tonic += d == 0;
            range += e->note < pd_solo_lo(pd_a.voice[e->bar]) || e->note > pd_solo_lo(pd_a.voice[e->bar]) + 12;
            avoid += th_is_avoid(pd_a.key[e->bar], pd_a.mode[e->bar], pd_a.pcs[e->bar], pd_a.np[e->bar], e->note);
        }
        for (bar = 0; bar < pd_a.bars; bar++) {
            int so[4], ch[4], n, nc, was[4];
            twice += onsets[bar] > 1;
            n = pd_sounding(&pd_a, bar, pd_a.at[bar * 16], 4, 4, so);
            if (!n) continue;
            bars++;
            btonic += pd_degree(pd_a.key[bar], pd_a.mode[bar], so[0]) == 0;
            nc = pd_sounding(&pd_a, bar, pd_a.at[bar * 16], 1, 3, ch);
            if (nc && so[0] < ch[nc - 1] + 3) low++;
            if (bar == 0 || bar % bpc || pd_a.key[bar] != pd_a.key[bar - 1]) continue;
            if (!pd_sounding(&pd_a, bar - 1, pd_a.at[bar * 16 - 1], 4, 4, was)) continue;
            if (pd_solo_legal(&pd_a, bar, was[0])) {    /* a chord change: keep a legal note */
                kept++;
                keep_bad += so[0] != was[0];
            } else {
                moved++;
                near += so[0] - was[0] <= 2 && so[0] - was[0] >= -2;
            }
        }
        rubs += pd_rubs(&pd_a, 4, 4, 0, &checked);
    }
    printf("pad solo: %ld notes, the tonic %.1f%% (of %ld bars sounding, %.1f%%); kept over %ld changes (%ld not), "
           "moved at %ld (%ld by a step or less, %.1f%%); %ld rubs in %ld pairs checked\n", notes,
           notes ? 100.0 * (double)tonic / (double)notes : 0.0, bars, bars ? 100.0 * (double)btonic / (double)bars : 0.0,
           kept, keep_bad, moved, near, moved ? 100.0 * (double)near / (double)moved : 0.0, rubs, checked);
    CHECK(notes > 3000, "pad solo: only %ld notes", notes);
    CHECK(twice == 0 && off_step == 0, "pad solo: %ld bars with 2+ onsets, %ld off step 0", twice, off_step);
    CHECK(short_ == 0, "pad solo: %ld notes not held to a bar line", short_);
    CHECK(deg_bad == 0 && tonic * 100 >= notes * 35, "pad solo: %ld notes off degrees 1, 2, 3, 7; the tonic in %ld of "
          "%ld", deg_bad, tonic, notes);
    CHECK(avoid == 0, "pad solo: %ld avoid notes", avoid);
    CHECK(keep_bad == 0 && kept > 500, "pad solo: a legal note left at %ld of %ld chord changes", keep_bad, kept);
    CHECK(near * 100 >= moved * 90, "pad solo: moved a step or less at %ld of %ld changes", near, moved);
    CHECK(range == 0 && low == 0, "pad solo: %ld notes outside their voice's range, %ld bars under the chord part's top + 3", range,
          low);
    CHECK(rubs == 0 && checked > 10000, "pad solo: %ld rubs with keys or bass (%ld checked)", rubs, checked);
    /* a key request landing at bar 16 in a section where the solo plays */
    for (seed = 1; seed < 2000 && found < 20; seed++) {
        int nk, bad = 0, after = 0;
        roll_song(NULL, seed, &r);
        song_start(&s, &r);
        if (r.v[RF_BARS] < 24) continue;
        pd_req_bar = -1;
        pd_play(&s, &pd_a, NULL);
        if (pd_a.part[15] != 2 || pd_a.part[16] != 2 || pd_a.blk[15] != pd_a.blk[16] || !(pd_a.layers[16] & L_KEYS)) continue;
        nk = (r.v[RF_KEY] + 1 + (int)(seed % 11u)) % 12;
        pd_req_bar = 14;
        pd_req_key = nk;
        song_start(&s, &r);
        pd_play(&s, &pd_b, NULL);
        pd_req_bar = -1;
        if (pd_b.key[16] != nk) continue;
        found++;
        for (i = 0; i < pd_b.ne; i++) {
            const pd_ev_t *e = &pd_b.ev[i];
            if (e->voice != V_PAD || e->lane != 4 || e->end <= pd_b.at[16 * 16]) continue;
            after += e->lane == 4;
            if (e->lane == 4) bad += pd_degree(nk, pd_b.mode[16], e->note) < 0;
        }
        CHECK(after > 0 && bad == 0, "pad key request, seed %u: %d of %d solo notes after it outside the new key", seed,
              bad, after);
    }
    CHECK(found >= 20, "pad key request: only %d songs", found);
}

/* Pads revision 2: the section's part (pads_section_part) and the song's solo voice (pads_solo_voice), pure functions
   of the roll and the plan. 40% (+-5) of songs have no solo note (every section then the chord part over struck keys,
   nothing over sustained keys). In the songs with one: over struck keys a section is the chord part or the solo note
   (the solo within 10 points of intro and break 35%, groove 25%, lead 15%, outro 30%); over sustained keys the solo
   note or nothing (the first design's shares within 10: groove 60%, intro, break, outro 50%, lead 30%). The voice:
   choir, flute, organ or strings, each 25% (+-7) of the solo songs where all four are allowed; never the flute with a
   flute lead, never the organ over organ keys. */
static void test_pad_sections(void)
{
    static const int want[2][5] = {{35, 25, 15, 35, 30}, {50, 60, 30, 50, 50}};  /* struck, sustained; by SEC_* */
    static const int VO[4] = {INS_VOX_PAD, INS_FLUTE, INS_ORGAN, INS_STRINGS};
    song_roll_t prev, r;
    uint32_t seed = 3131;
    long blocks[2][5] = {{0}}, solo[2][5] = {{0}}, bad_part = 0, songs = 0, none = 0, all4 = 0, by[4] = {0, 0, 0, 0},
         odd = 0, fl_lead = 0, org_keys = 0, excl = 0;
    int k, i, c, v;
    for (k = 0; k < 2000; k++) {
        song_plan_t pl;
        const lib_set_t *set;
        int voice;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        roll_song(k ? &prev : 0, seed, &r);
        prev = r;
        voice = pads_solo_voice(&r);
        set = lib_set_by_id((uint8_t)r.v[RF_SET]);
        CHECK(voice == pads_solo_voice(&r), "pad sections: pads_solo_voice is not pure");
        if (k < 500) {                                   /* the shares over 500 songs */
            songs++;
            none += voice == INS_NONE;
        }
        if (set && set->lead == INS_FLUTE) { fl_lead++; excl += voice == INS_FLUTE; }
        if (set && set->chords == INS_ORGAN) { org_keys++; excl += voice == INS_ORGAN; }
        for (v = 0; v < 4 && VO[v] != voice; v++) {}
        odd += voice != INS_NONE && v == 4;
        if (voice != INS_NONE && k < 500 && set && set->lead != INS_FLUTE && set->chords != INS_ORGAN) {
            all4++;
            by[v]++;
        }
        if (k >= 500) continue;
        plan_build(&r, &pl);
        for (i = 0; i < pl.n; i++) {
            int sec = pl.b[i].section, sus;
            for (sus = 0; sus < 2; sus++) {
                int p = pads_section_part(&r, i, sec, sus);
                CHECK(p == pads_section_part(&r, i, sec, sus), "pads_section_part is not pure");
                if (voice == INS_NONE) {                 /* no solo note: the chord part, or nothing */
                    bad_part += p != (sus ? 0 : 1);
                    continue;
                }
                bad_part += sus ? p != 0 && p != 2 : p != 1 && p != 2;
                blocks[sus][sec]++;
                solo[sus][sec] += p == 2;
            }
        }
    }
    printf("pad sections: %ld of %ld songs with no solo note (%.1f%%, want 40%%)\n", none, songs,
           100.0 * (double)none / (double)songs);
    CHECK(none * 100 >= songs * 35 && none * 100 <= songs * 45, "pad sections: %ld of %ld songs with no solo note",
          none, songs);
    CHECK(bad_part == 0, "pad sections: %ld sections with a part outside chord-or-solo (struck), solo-or-none "
          "(sustained), chord (struck, no solo) or none (sustained, no solo)", bad_part);
    for (k = 0; k < 2; k++)
        for (c = 0; c < 5; c++) {
            long pct = blocks[k][c] ? solo[k][c] * 100 / blocks[k][c] : 0;
            printf("pad sections: over %s keys, %-6s the solo in %ld of %ld blocks (%ld%%, want %d%%)\n",
                   k ? "sustained" : "struck", plan_section_name(c), solo[k][c], blocks[k][c], pct, want[k][c]);
            CHECK(blocks[k][c] > 60 && pct >= want[k][c] - 10 && pct <= want[k][c] + 10,
                  "pad sections: over %s keys, %s solo in %ld%%", k ? "sustained" : "struck", plan_section_name(c), pct);
        }
    for (v = 0; v < 4; v++) {
        printf("pad sections: voice %d in %ld of %ld solo songs with all four allowed (%.1f%%)\n", VO[v], by[v], all4,
               all4 ? 100.0 * (double)by[v] / (double)all4 : 0.0);
        CHECK(all4 > 150 && by[v] * 100 >= all4 * 18 && by[v] * 100 <= all4 * 32,
              "pad sections: voice %d in %ld of %ld solo songs", VO[v], by[v], all4);
    }
    printf("pad sections: %ld songs with a flute lead, %ld over organ keys: %ld excluded voices drawn\n", fl_lead,
           org_keys, excl);
    CHECK(odd == 0, "pad sections: %ld songs with a solo voice not choir, flute, organ or strings", odd);
    CHECK(excl == 0 && fl_lead > 20 && org_keys > 20, "pad sections: %ld excluded voices drawn (%ld flute leads, %ld "
          "organ keys)", excl, fl_lead, org_keys);
}

/* Either the chord part or the solo note, never both (pads revision 2), as the songs play over 500 songs: no bar has
   both chord-part (lanes 1-3) and solo (lane 4) events, and no pad note sounds while the other part's does; a song
   with no solo voice never plays lane 4; each keys bar of a chord section over struck keys has the chord part, of a
   solo section the solo note (95%); over struck keys, in the songs with a solo, 25% (+-7) of the keys sections are the
   solo's; the solo's notes in its voice's range (the strings 84-96, the others 72-84). Handovers (Review Focus 1): at a
   section line where one part hands over to the other, the outgoing part has ended on the line and the incoming one
   sounds from it. The engine (a few handover songs rendered through vx_song): on step 2 of every bar no lane is held
   that the bar's part does not play (no stuck note), and lane 4 sounds on the song's solo voice. */
static int pd_lane_events(const pd_rec_t *rec, int bar, int lo, int hi)
{
    int k, n = 0, to = bar + 1 < rec->bars ? rec->first[bar + 1] : rec->ne;
    for (k = rec->first[bar]; k < to; k++)
        n += rec->ev[k].voice == V_PAD && rec->ev[k].lane >= lo && rec->ev[k].lane <= hi;
    return n;
}
static long pd_engine_stuck(const song_roll_t *roll, long *checked, long *wrong)
{
    static song_t s;
    static float l[1024], rr[1024];
    song_ev_t ev[64];
    float frac = 0.0f;
    long bad = 0;
    int voice = pads_solo_voice(roll);
    vx_bed_off(1);
    vx_init();
    song_start(&s, roll);
    vx_song(&s.roll);
    while (s.state == SS_PLAYING) {
        int bar = s.bar, st = s.step, n = song_tick(&s, ev, 64), i;
        float adv;
        uint32_t left;
        if (st == 2) {
            const plan_block_t *b = plan_at(&s.plan, bar);
            const lib_set_t *set = lib_set_by_id((uint8_t)s.roll.v[RF_SET]);
            int part = b && (b->layers & L_KEYS) ? pads_section_part(&s.roll, (int)(b - s.plan.b), b->section,
                                                                      !set || lib_ins_sustained(set->chords)) : 0;
            int lane, step, held;
            for (lane = 1; lane <= 4; lane++) {
                vx_pad_lane(lane, &step, &held);
                (*checked)++;
                if (held && part != (lane < 4 ? 1 : 2)) {
                    if (bad < 5)
                        printf("  pad stuck: seed %u bar %d lane %d held in a part-%d section\n", (unsigned)roll->seed,
                               bar, lane, part);
                    bad++;
                }
            }
            if (vx_pad_lane(4, &step, &held) > 0.0f && vx_pad_lane_patch(4) != vx_patch(voice))
                (*wrong)++;
        }
        for (i = 0; i < n; i++)
            vx_event(&ev[i]);
        adv = song_step_samples(&s) + frac;
        left = (uint32_t)adv;
        frac = adv - (float)left;
        while (left) {
            uint32_t b = left < 1024u ? left : 1024u;
            memset(l, 0, b * sizeof(float));
            memset(rr, 0, b * sizeof(float));
            vx_render(l, rr, b);
            left -= b;
        }
    }
    vx_release_all();
    vx_bed_off(0);
    return bad;
}
static void test_pad_either(void)
{
    static song_t s;
    song_roll_t prev, r;
    uint32_t seed = 6262;
    long both = 0, overlap = 0, none_notes = 0, cbars = 0, cmiss = 0, sbars = 0, smiss = 0, ksec = 0, ksolo = 0,
         range = 0, solo_notes = 0, hand = 0, hand_bad = 0, stuck = 0, checked = 0, wrong = 0;
    int k, bar, i, rendered = 0;
    for (k = 0; k < 500; k++) {
        int handover = 0, voice;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        roll_song(k ? &prev : 0, seed, &r);
        prev = r;
        voice = pads_solo_voice(&r);
        song_start(&s, &r);
        if (voice != INS_NONE)                           /* the keys sections over struck keys */
            for (i = 0; i < s.plan.n; i++) {
                const lib_set_t *set = lib_set_by_id((uint8_t)r.v[RF_SET]);
                if (!(s.plan.b[i].layers & L_KEYS) || !set || lib_ins_sustained(set->chords)) continue;
                ksec++;
                ksolo += pads_section_part(&r, i, s.plan.b[i].section, 0) == 2;
            }
        pd_play(&s, &pd_a, NULL);
        for (i = 0; i < pd_a.ne; i++) {
            const pd_ev_t *e = &pd_a.ev[i];
            int other[4], lo = pd_solo_lo(pd_a.voice[e->bar]);
            if (e->voice != V_PAD) continue;
            if (pd_sounding(&pd_a, e->bar, e->start, e->lane == 4 ? 1 : 4, e->lane == 4 ? 3 : 4, other)) overlap++;
            if (e->lane != 4) continue;
            solo_notes++;
            none_notes += pd_a.voice[e->bar] == INS_NONE;
            range += e->note < lo || e->note > lo + 12;
        }
        for (bar = 0; bar < pd_a.bars; bar++) {
            int x[4], keys = (pd_a.layers[bar] & L_KEYS) != 0;
            both += pd_lane_events(&pd_a, bar, 1, 3) && pd_lane_events(&pd_a, bar, 4, 4);
            if (keys && pd_a.part[bar] == 1 && !pd_a.sus[bar]) {
                cbars++;
                cmiss += !pd_sounding(&pd_a, bar, pd_a.at[bar * 16], 1, 3, x);
            }
            if (keys && pd_a.part[bar] == 2) {
                sbars++;
                smiss += !pd_sounding(&pd_a, bar, pd_a.at[bar * 16], 4, 4, x);
            }
            if (bar > 0 && keys && (pd_a.layers[bar - 1] & L_KEYS) && pd_a.part[bar] + pd_a.part[bar - 1] == 3) {
                int in = pd_a.part[bar] == 1, a = pd_sounding(&pd_a, bar, pd_a.at[bar * 16] - 1, in ? 4 : 1, in ? 4 : 3, x);
                int out = pd_sounding(&pd_a, bar, pd_a.at[bar * 16], in ? 4 : 1, in ? 4 : 3, x);
                int now = pd_sounding(&pd_a, bar, pd_a.at[bar * 16], in ? 1 : 4, in ? 3 : 4, x);
                hand++;
                handover |= a != 0;                      /* the outgoing part sounded to the line */
                hand_bad += out != 0 || now == 0;
            }
        }
        if (handover && rendered < 6) {                  /* the engine: no lane left held */
            rendered++;
            stuck += pd_engine_stuck(&r, &checked, &wrong);
        }
    }
    printf("pad either: %ld bars with both parts' events, %ld pad notes over the other part; %ld lane-4 notes in songs "
           "with no solo voice\n", both, overlap, none_notes);
    printf("pad either: chord part in %ld of %ld chord-section keys bars, solo in %ld of %ld solo-section keys bars; "
           "the solo in %ld of %ld keys sections over struck keys (%.1f%%, want 25%%)\n", cbars - cmiss, cbars,
           sbars - smiss, sbars, ksolo, ksec, ksec ? 100.0 * (double)ksolo / (double)ksec : 0.0);
    printf("pad either: %ld handovers, %ld not clean; the engine: %ld held lanes outside their part (%ld checked, %d "
           "songs), %ld bars with lane 4 off the song's voice\n", hand, hand_bad, stuck, checked, rendered, wrong);
    CHECK(both == 0 && overlap == 0, "pad either: %ld bars with both parts, %ld notes over the other part", both,
          overlap);
    CHECK(none_notes == 0, "pad either: %ld solo notes in songs with no solo voice", none_notes);
    CHECK(cmiss == 0 && cbars > 5000, "pad either: no chord part in %ld of %ld chord-section keys bars", cmiss, cbars);
    CHECK(smiss * 100 <= sbars * 5 && sbars > 1000, "pad either: no solo in %ld of %ld solo-section keys bars", smiss,
          sbars);
    CHECK(ksec > 500 && ksolo * 100 >= ksec * 18 && ksolo * 100 <= ksec * 32, "pad either: the solo in %ld of %ld keys "
          "sections over struck keys", ksolo, ksec);
    CHECK(range == 0 && solo_notes > 1000, "pad either: %ld of %ld solo notes outside their voice's range", range,
          solo_notes);
    CHECK(hand > 100 && hand_bad * 100 <= hand * 5, "pad either: %ld of %ld handovers not clean", hand_bad, hand);
    CHECK(rendered == 6 && stuck == 0 && wrong == 0, "pad either: %ld stuck lanes, %ld bars off the solo voice (%d "
          "songs rendered)", stuck, wrong, rendered);
}

/* the same seed gives the same pad events, alone, after another song, or with another ticking in between */
static void test_pad_determinism(void)
{
    static song_t s, o;
    song_roll_t r, ro;
    uint32_t seed;
    long pads = 0;
    int i;
    for (seed = 11; seed < 31; seed++) {
        uint32_t h;
        roll_song(NULL, seed, &r);
        roll_song(&r, seed + 900u, &ro);
        song_start(&s, &r);
        pd_play(&s, &pd_a, NULL);
        h = pd_a.hash;
        for (i = 0; i < pd_a.ne; i++) pads += pd_a.ev[i].voice == V_PAD;
        song_start(&o, &ro);
        pd_play(&o, &pd_b, NULL);                        /* another song first */
        song_start(&s, &r);
        pd_play(&s, &pd_b, NULL);
        CHECK(pd_b.hash == h, "pad determinism, seed %u: after another song %08x != %08x", seed, pd_b.hash, h);
        roll_song(&r, seed + 900u, &ro);
        song_start(&o, &ro);
        song_start(&s, &r);
        pd_play(&s, &pd_b, &o);                          /* another ticking in between */
        CHECK(pd_b.hash == h, "pad determinism, seed %u: with another in between %08x != %08x", seed, pd_b.hash, h);
    }
    CHECK(pads > 100, "pad determinism: only %ld pad events", pads);
}

/* The A/B switch (host renders, VNL_PADS=double): over percussive keys with held comping, the chord part doubles the
   keys' voicing (the same pitches, struck with them) instead of its own voice-led notes: every chord-part note one the
   keys sound, and the default (the switch off) does not do so throughout. */
static int pdbl_play(const song_roll_t *r, int *bad, int *notes)
{
    static song_t s;
    song_ev_t ev[64];
    int cur[4] = {0}, nc = 0, strikes = 0;
    *bad = *notes = 0;
    song_start(&s, r);
    while (s.state == SS_PLAYING) {
        int n = song_tick(&s, ev, 64), i, j, fresh = 1;
        for (i = 0; i < n; i++)
            if (ev[i].voice == V_CHORD) {
                if (fresh) { nc = 0; strikes++; fresh = 0; }
                if (nc < 4) cur[nc++] = ev[i].note;
            }
        for (i = 0; i < n; i++)
            if (ev[i].voice == V_PAD && ev[i].pad >= 1 && ev[i].pad <= 3) {
                int in = 0;
                for (j = 0; j < nc; j++) in |= cur[j] == ev[i].note;
                (*notes)++;
                *bad += !in;
            }
    }
    return strikes;
}
static void test_pad_double(void)
{
    static const uint32_t SEED[3] = {1001u, 1007u, 1008u};
    static const int SET[3] = {1, 4, 19};
    int k;
    for (k = 0; k < 3; k++) {
        song_roll_t r;
        int bad, notes, bad0, notes0, strikes;
        roll_song(NULL, SEED[k], &r);
        CHECK(roll_set(&r, RF_SET, SET[k]) == SET[k] && harmony_style(&r) == CS_HELD,
              "pad double: seed %u on set %d is not a held-comping song", (unsigned)SEED[k], SET[k]);
        pdbl_play(&r, &bad0, &notes0);
        song_set_pads_double(1);
        strikes = pdbl_play(&r, &bad, &notes);
        song_set_pads_double(0);
        printf("pad double: seed %u set %d: %d keys strikes, %d chord-part notes, %d not the keys' (default: %d of %d)\n",
               (unsigned)SEED[k], SET[k], strikes, notes, bad, bad0, notes0);
        CHECK(notes >= strikes && bad == 0, "pad double: seed %u: %d of %d chord-part notes not the keys' (%d strikes)",
              (unsigned)SEED[k], bad, notes, strikes);
        CHECK(bad0 > 0, "pad double: seed %u's default chord part already doubles the keys", (unsigned)SEED[k]);
    }
}

/* The strum engine (CS_STRUM, the guitars; Task 4): per strum (a step's chord events) at most 6 notes, each string's
   onset 5-15 ms after the last (quick) or 30-80 ms (slow: the ballad, half the time); up strokes (high to low) only on
   the odd 8ths; chucks (pad 1: the sampler's muted note) only in the chuck pattern; every note a chord tone in E2-E5,
   none a semitone over another, over the root or over a bass note it sounds with (a major 7th only over the root);
   the top note moves 5 semitones or less in 80% of chord changes; with the pad A/B switch on (VNL_PADS=double) the
   chord part doubles the guitar's notes, as over held comping; one shape a chord through the song (its notes 6 at
   most). Rendered, the chords' pool never steals a held note
   (8 voices for a sampled instrument), and over 500 seeds a ballad is slow in 45-55% of songs, the rest never. */
static long st_strums, st_ups, st_moves, st_small, st_chucks, st_pad_notes, st_bad[9], st_pad0, st_bad0;
static int st_count0; /* st_play counts the default chord part (st_pad0, st_bad0) of a song played doubled too */
static void st_play(const song_roll_t *r, int pat, int slow, int dbl)
{
    static song_t s;
    song_ev_t ev[64];
    const lib_prog_t *pr = lib_prog_by_id((uint8_t)r->v[RF_PROG]);
    int loop = pr && pr->n * pr->bars_per_chord ? pr->n * pr->bars_per_chord : 4;
    double lo = (slow ? 30.0 : 5.0) * 44.1 - 1.0, hi = (slow ? 80.0 : 15.0) * 44.1 + 1.0;
    int prev_top = 0, prev_pcs[5] = {0}, prev_np = 0, bar_top = 0, cur = -1, pcs[5] = {0}, np = 0;
    int gn[64], ngn = 0, pn[16], npn = 0, i, j, nsig = 0;
    uint32_t sig[32], csig = 0;
    uint64_t used[32];                                   /* each chord's notes through the song (bit: note - 40) */
    song_set_pads_double(dbl);
    song_start(&s, r);
    while (s.state == SS_PLAYING) {
        int bar = s.bar, step = s.step, n = song_tick(&s, ev, 64), k = 0, nb = 0, bass[8], c[64], d[64], pad[64];
        if (bar != cur) {
            if (bar_top) {
                if (prev_top && (np != prev_np || memcmp(pcs, prev_pcs, sizeof pcs))) {
                    st_moves++;
                    st_small += abs(bar_top - prev_top) <= 5;
                }
                prev_top = bar_top;
                prev_np = np;
                memcpy(prev_pcs, pcs, sizeof pcs);
            }
            for (i = 0; i < npn; i++) {
                for (j = 0; j < ngn && gn[j] != pn[i]; j++) {}
                if (dbl) { st_pad_notes++; st_bad[8] += j == ngn; }
                else if (st_count0) { st_pad0++; st_bad0 += j == ngn; }
            }
            cur = bar;
            bar_top = ngn = npn = 0;
            memset(pcs, 0, sizeof pcs);
            np = harmony_chord_at(&s.roll, bar, bar / loop + 1, 0, pcs);
            for (i = 0, csig = (uint32_t)s.roll.v[RF_KEY] + 1u; i < np; i++) csig = csig * 31u + (uint32_t)pcs[i] + 1u;
        }
        for (i = 0; i < n; i++) {
            if (ev[i].voice == V_CHORD) { c[k] = ev[i].note; d[k] = (int)ev[i].delay; pad[k++] = ev[i].pad; }
            if (ev[i].voice == V_BASS && nb < 8) bass[nb++] = ev[i].note;
            if (ev[i].voice == V_PAD && ev[i].pad >= 1 && ev[i].pad <= 3 && npn < 16) pn[npn++] = ev[i].note;
        }
        if (!k) continue;
        st_strums++;
        st_bad[0] += k > 6;
        for (i = 1; i < k; i++)
            st_bad[1] += (double)(d[i] - d[i - 1]) < lo || (double)(d[i] - d[i - 1]) > hi;
        if (k >= 2 && c[0] > c[k - 1]) {
            st_ups++;
            st_bad[2] += (step & 3) != 2;
        }
        for (i = 0; i < k; i++) {
            int in = 0;
            st_chucks += pad[i] == 1;
            st_bad[3] += pad[i] == 1 && pat != SP_CHUCK;
            for (j = 0; j < np; j++) in |= (c[i] - pcs[j] + 120) % 12 == 0;
            st_bad[4] += !in;
            st_bad[5] += c[i] < 40 || c[i] > 76;
            st_bad[6] += (c[i] - pcs[0] + 120) % 12 == 1;
            for (j = 0; j < k; j++) st_bad[6] += j != i && c[i] > c[j] && pads_rub(c[i], c[j]);
            for (j = 0; j < nb; j++) {
                int iv = (c[i] - bass[j] + 120) % 12;
                st_bad[7] += iv == 1 || (iv == 11 && bass[j] % 12 != pcs[0]);
            }
            if (c[i] > bar_top) bar_top = c[i];
            for (j = 0; j < nsig && sig[j] != csig; j++) {}
            if (j == nsig && nsig < 32) { sig[nsig] = csig; used[nsig++] = 0; }
            if (j < nsig && c[i] >= 40 && c[i] <= 76) used[j] |= (uint64_t)1 << (c[i] - 40);
            if (ngn < 64) gn[ngn++] = c[i];
        }
    }
    for (j = 0; j < nsig; j++) {                          /* one guitar shape a chord: 6 notes at most */
        int m = 0;
        for (i = 0; i < 37; i++) m += (int)((used[j] >> i) & 1u);
        st_bad[0] += m > 6;
    }
    song_set_pads_double(0);
}

/* the first 24 bars rendered: the chords' pool's steals of a held note, and the most voices sounding at once */
static uint32_t st_render(const song_roll_t *r, int *peak)
{
    static song_t s;
    song_ev_t ev[64];
    float frac = 0.0f;
    vx_init();
    song_start(&s, r);
    vx_song(&s.roll);
    *peak = 0;
    while (s.state == SS_PLAYING && s.bar < 24) {
        int n = song_tick(&s, ev, 64), i;
        float adv = song_step_samples(&s) + frac;
        uint32_t left = (uint32_t)adv;
        frac = adv - (float)left;
        for (i = 0; i < n; i++) vx_event(&ev[i]);
        while (left) {
            uint32_t b = left < 256u ? left : 256u;
            memset(vt_l, 0, b * sizeof(float));
            memset(vt_r, 0, b * sizeof(float));
            vx_render(vt_l, vt_r, b);
            if (vx_active() > *peak) *peak = vx_active();
            left -= b;
        }
    }
    return vx_steals(V_CHORD);
}

static void test_strum(void)
{
    static const int SETS[3] = {2, 9, 17};
    long pcnt[SP_N] = {0}, ballads = 0, slow_ballads = 0, bad_slow = 0;
    int got[SP_N + 1] = {0}, rendered[SP_N + 1] = {0}, seed, k, dbl_songs = 0;
    for (seed = 1; seed <= 500; seed++) {
        song_roll_t r;
        int slow, p;
        roll_song(NULL, (uint32_t)seed, &r);
        p = strum_pattern(&r, &slow);
        CHECK(p >= 0 && p < SP_N, "strum: seed %d pattern %d", seed, p);
        if (p < 0 || p >= SP_N) continue;
        pcnt[p]++;
        if (p == SP_BALLAD) { ballads++; slow_ballads += slow; }
        else bad_slow += slow;
    }
    printf("strum: patterns over 500 seeds ddu %ld chuck %ld bossa %ld ballad %ld (%ld slow)\n", pcnt[0], pcnt[1],
           pcnt[2], pcnt[3], slow_ballads);
    for (k = 0; k < SP_N; k++) CHECK(pcnt[k] >= 75 && pcnt[k] <= 175, "strum: pattern %d in %ld of 500", k, pcnt[k]);
    CHECK(ballads && slow_ballads * 100 >= ballads * 45 && slow_ballads * 100 <= ballads * 55,
          "strum: %ld of %ld ballads slow, wanted 45-55%%", slow_ballads, ballads);
    CHECK(bad_slow == 0, "strum: %ld songs of another pattern slow", bad_slow);
    memset(st_bad, 0, sizeof st_bad);
    st_strums = st_ups = st_moves = st_small = st_chucks = st_pad_notes = st_pad0 = st_bad0 = 0;
    for (seed = 1; seed < 4000; seed++) {          /* three songs of each pattern (the ballad quick and slow) */
        song_roll_t r;
        int slow, p, kind, set = SETS[seed % 3];
        roll_song(NULL, (uint32_t)seed, &r);
        if (roll_set(&r, RF_SET, set) != set || harmony_style(&r) != CS_STRUM) continue;
        p = strum_pattern(&r, &slow);
        kind = p == SP_BALLAD && slow ? SP_N : p;
        if (got[kind] >= 3) continue;
        got[kind]++;
        st_count0 = set == 2 && dbl_songs < 4;
        st_play(&r, p, slow, 0);
        if (st_count0) { st_play(&r, p, slow, 1); dbl_songs++; }
        if (!rendered[kind] && (kind == SP_CHUCK || kind == SP_N || kind == SP_DDU)) {
            int peak;
            uint32_t st = st_render(&r, &peak);
            rendered[kind] = 1;
            printf("strum: seed %d pattern %d%s: %u held notes stolen on the chords' pool, %d voices at most\n", seed,
                   p, slow ? " (slow)" : "", (unsigned)st, peak);
            CHECK(st == 0, "strum: seed %d: %u held chord notes stolen", seed, (unsigned)st);
        }
    }
    for (k = 0; k <= SP_N; k++) CHECK(got[k] == 3, "strum: only %d songs of kind %d", got[k], k);
    printf("strum: %ld strums (%ld up, %ld chuck notes); top note within 5 semitones in %ld of %ld chord changes; "
           "pads double: %ld chord-part notes, %ld not the guitar's (default: %ld of %ld)\n", st_strums, st_ups,
           st_chucks, st_small, st_moves, st_pad_notes, st_bad[8], st_bad0, st_pad0);
    CHECK(st_strums > 1000 && st_ups > 100 && st_chucks > 100, "strum: %ld strums, %ld up, %ld chuck notes", st_strums,
          st_ups, st_chucks);
    CHECK(st_bad[0] == 0, "strum: %ld strums of more than 6 notes (or chords of more than one shape)", st_bad[0]);
    CHECK(st_bad[1] == 0, "strum: %ld string onsets outside the spread's window", st_bad[1]);
    CHECK(st_bad[2] == 0, "strum: %ld up strokes off the odd 8ths", st_bad[2]);
    CHECK(st_bad[3] == 0, "strum: %ld chuck notes outside the chuck pattern", st_bad[3]);
    CHECK(st_bad[4] == 0, "strum: %ld notes not chord tones", st_bad[4]);
    CHECK(st_bad[5] == 0, "strum: %ld notes outside E2-E5", st_bad[5]);
    CHECK(st_bad[6] == 0, "strum: %ld semitone rubs inside a strum (or over the root)", st_bad[6]);
    CHECK(st_bad[7] == 0, "strum: %ld semitone rubs over the bass", st_bad[7]);
    CHECK(st_moves > 50 && st_small * 100 >= st_moves * 80, "strum: the top moves 5 or less in %ld of %ld changes",
          st_small, st_moves);
    CHECK(dbl_songs == 4 && st_pad_notes > 50 && st_bad[8] == 0,
          "strum: pads double: %ld of %ld chord-part notes not the guitar's (%d songs)", st_bad[8], st_pad_notes,
          dbl_songs);
    CHECK(st_bad0 > 0, "strum: the default chord part already doubles the guitar (%ld notes)", st_pad0);
}

/* The packed sample tables (samples_data.c, from tools/samples/pack.py): every bank's zones and every drum hit are
   well formed, the byte count adds up, and the whole set fits its flash share. */
static void check_zone(const smp_zone_t *z, const char *what, int id, int k)
{
    CHECK(z->data != NULL && z->len > 0, "%s %d zone %d: empty", what, id, k);
    CHECK(z->loop_start + z->loop_len <= z->len, "%s %d zone %d: loop %u+%u past %u", what, id, k,
          (unsigned)z->loop_start, (unsigned)z->loop_len, (unsigned)z->len);
    CHECK(z->rate == 16000 || z->rate == 22050 || z->rate == 32000, "%s %d zone %d: rate %u", what, id, k,
          (unsigned)z->rate);
    CHECK(z->gain > 0.0f, "%s %d zone %d: gain %g", what, id, k, (double)z->gain);
    CHECK(z->idx0 <= 88, "%s %d zone %d: idx0 %d", what, id, k, z->idx0);
}

static void test_samples_table(void)
{
    uint32_t sum = 0;
    int id, k;

    for (id = 0; id < SB_N; id++) {
        const smp_bank_t *b = smp_bank(id);
        CHECK(b != NULL && b->n > 0, "bank %d missing", id);
        if (!b)
            continue;
        for (k = 0; k < b->n; k++) {
            check_zone(&b->z[k], "bank", id, k);
            CHECK(k == 0 || b->z[k].root > b->z[k - 1].root, "bank %d zone %d: root %d after %d", id, k,
                  b->z[k].root, k ? b->z[k - 1].root : 0);
            sum += (b->z[k].len + 1) / 2;
        }
    }
    for (id = 0; id < SH_N; id++) {
        const smp_zone_t *h = smp_hit(id);
        CHECK(h != NULL, "hit %d missing", id);
        if (!h)
            continue;
        check_zone(h, "hit", id, 0);
        sum += (h->len + 1) / 2;
    }
    CHECK(smp_bank(-1) == NULL && smp_bank(SB_N) == NULL, "bank out of range not NULL");
    CHECK(smp_hit(-1) == NULL && smp_hit(SH_N) == NULL, "hit out of range not NULL");
    CHECK(sum == smp_bytes(), "zone bytes %u, smp_bytes %u", (unsigned)sum, (unsigned)smp_bytes());
    CHECK(smp_bytes() <= 200 * 1024, "samples %u B over 200 KB", (unsigned)smp_bytes());
}

/* ------------------------------------------------------------------------------ the sampler voice (M_SAMP) --- */
static const int SP_INS[SB_N] = {INS_KNIGHT, INS_NYLON, INS_EMILY}; /* each bank's patch */
#define SP_LEN (4u * 44100u)
static float sp_a[SP_LEN], sp_b[SP_LEN];

/* n samples of a standalone voice (dv 1 or 2) on patch p into x; 0 if any was NaN or inf, or the decoder read past its
   zone's end */
static int sp_run(smp_voice_t *v, float *x, uint32_t n)
{
    uint32_t i;
    int ok = 1;
    for (i = 0; i < n; i++) {
        float y = samp_sample(v);
        const smp_zone_t *z = v->d.z;
        if (y != y || fm_fabsf(y) > 1e30f)
            ok = 0;
        if (z && v->d.ni > (z->loop_len ? z->loop_start + z->loop_len : z->len))
            ok = 0;
        x[i] = y;
    }
    return ok;
}

/* the pitch of x[from .. from+w) against p0 samples a period, in cents (positive: sharp): the normalized
   autocorrelation's peak near a lag of m periods (m * p0 about 1000 samples or more), parabolically interpolated */
static float sp_cents(const float *x, uint32_t from, uint32_t w, float p0)
{
    int m = (int)(1000.0f / p0) + 1, lo = (int)((float)m * p0 * 0.99f), hi = (int)((float)m * p0 * 1.01f) + 1;
    int lag, best = lo;
    float r[600], a, b, c, d, l;
    if (hi - lo >= 600)
        hi = lo + 599;
    for (lag = lo; lag <= hi; lag++) {
        double s = 0.0, e0 = 0.0, e1 = 0.0;
        uint32_t i;
        for (i = from; i < from + w; i++) {
            s += (double)x[i] * (double)x[i + (uint32_t)lag];
            e0 += (double)x[i] * (double)x[i];
            e1 += (double)x[i + (uint32_t)lag] * (double)x[i + (uint32_t)lag];
        }
        r[lag - lo] = e0 > 0.0 && e1 > 0.0 ? (float)s * fm_rsqrtf((float)e0) * fm_rsqrtf((float)e1) : 0.0f;
        if (r[lag - lo] > r[best - lo])
            best = lag;
    }
    l = (float)best;
    if (best > lo && best < hi) {
        a = r[best - lo - 1];
        b = r[best - lo];
        c = r[best - lo + 1];
        d = a - 2.0f * b + c;
        l += d != 0.0f ? 0.5f * (a - c) / d : 0.0f;
    }
    return 1200.0f * fm_log2f((float)m * p0 / l);
}

static float sp_rms(const float *x, uint32_t n)
{
    double e = 0.0;
    uint32_t i;
    for (i = 0; i < n; i++)
        e += (double)x[i] * (double)x[i];
    return n ? fm_sqrtf((float)(e / n)) : 0.0f;
}

/* a role's mean level (dB) over the bars it sounds in, the song's keys or lead replaced (lk_song) */
static float sp_lk_mean(const song_roll_t *r, int role, int keys, int lead)
{
    int b, n, c = 0;
    double sum = 0.0;
    lk_keys = keys;
    lk_lead = lead;
    n = lk_song(r, role, lk_db[0]);
    lk_keys = lk_lead = INS_NONE;
    for (b = 0; b < n; b++)
        if (lk_db[0][b] > -60.0) {
            sum += lk_db[0][b];
            c++;
        }
    return c ? (float)(sum / c) : -200.0f;
}

/* The sampler (M_SAMP; plan Task 2, Review Focus 1-3 and 5): the data decodes (each zone and hit: a real level, never
   at the clamp; hits one-shot); notes at the zones' centers and boundaries render within 3 cents, at the full rate and
   at half; notes past the range play the end zones, never reading past them; a held note's loop has no click; a
   release fades smoothly to -60 dB; a stolen voice fades in under 6 ms, no click; velocity 1 and 127 stay finite and
   denormal-free; a seed renders the same alone and after a song on the Knight. */
static void test_samp(void)
{
    static const int NOTES[7] = {40, 52, 59, 64, 71, 76, 83};
    smp_voice_t v, w;
    int bk, k, bad = 0;
    float worst = 0.0f;

    /* the data: decoded RMS (as -1..1) at least 0.01, no sample at the clamp, hits one-shot */
    for (bk = 0; bk < SB_N + SH_N; bk++) {
        const smp_bank_t *b = bk < SB_N ? smp_bank(bk) : NULL;
        int nz = b ? b->n : 1;
        for (k = 0; k < nz; k++) {
            const smp_zone_t *z = b ? &b->z[k] : smp_hit(bk - SB_N);
            smp_dec_t d;
            double e = 0.0;
            uint32_t i, clip = 0;
            smp_dec_init(&d, z);
            for (i = 0; i < z->len; i++) {
                int y = smp_dec_next(&d);
                e += (double)y * (double)y;
                clip += y >= 32767 || y <= -32768;
            }
            e = e / z->len / (32768.0 * 32768.0);
            CHECK(e >= 1e-4 && clip == 0, "samples: %s %d zone %d decodes to RMS %.4f, %u at the clamp",
                  b ? "bank" : "hit", b ? bk : bk - SB_N, k, (double)fm_sqrtf((float)e), (unsigned)clip);
            if (!b)
                CHECK(z->loop_len == 0, "samples: hit %d loops", bk - SB_N);
        }
    }

    for (bk = 0; bk < SB_N; bk++) {
        const vx_patch_t *p = vx_patch(SP_INS[bk]);
        const smp_bank_t *b = smp_bank(bk);
        int dv;
        CHECK(p && p->model == M_SAMP && (int)p->p[0] == bk, "samp: ins %d isn't bank %d's sampler patch", SP_INS[bk],
              bk);
        if (!p || p->model != M_SAMP)
            continue;
        /* pitch: 200 ms after the attack (from 20 ms into the loop: a plucked string's attack runs sharp, the nylon's
           up to 16 cents on one zone), full rate and half */
        for (dv = 1; dv <= 2; dv++)
            for (k = 0; k < 7; k++) {
                float sr = 44100.0f / (float)dv, hz = 440.0f * fm_exp2f(((float)NOTES[k] - 69.0f) / 12.0f), c;
                uint32_t at;
                v.dv = (float)dv;
                samp_start(&v, p, (float)NOTES[k], 0.8f);
                at = (uint32_t)((float)v.d.z->loop_start / v.step + 0.02f * sr);
                bad |= !sp_run(&v, sp_a, at + (uint32_t)(0.25f * sr) + 2000u);
                c = sp_cents(sp_a, at, (uint32_t)(0.2f * sr), sr / hz);
                worst = fm_maxf(worst, fm_fabsf(c));
                CHECK(fm_fabsf(c) <= 3.0f, "samp: bank %d note %d (dv %d) %+.2f cents off", bk, NOTES[k], dv,
                      (double)c);
            }
        /* out of range: the end zones, the decoder in bounds, finite; the low note in tune */
        {
            float hz = 440.0f * fm_exp2f((20.0f - 69.0f) / 12.0f), c;
            v.dv = 1.0f;
            samp_start(&v, p, 20.0f, 0.8f);
            CHECK(v.d.z == &b->z[0], "samp: bank %d note 20 not on its lowest zone", bk);
            CHECK(sp_run(&v, sp_a, SP_LEN), "samp: bank %d note 20 read past its zone or went NaN", bk);
            c = sp_cents(sp_a, (uint32_t)((float)v.d.z->loop_start / v.step) + 882u, 22050u, 44100.0f / hz);
            CHECK(fm_fabsf(c) <= 3.0f, "samp: bank %d note 20 %+.2f cents off", bk, (double)c);
            v.dv = 1.0f;
            samp_start(&v, p, 110.0f, 0.8f);
            CHECK(v.d.z == &b->z[b->n - 1], "samp: bank %d note 110 not on its highest zone", bk);
            CHECK(sp_run(&v, sp_a, SP_LEN), "samp: bank %d note 110 read past its zone or went NaN", bk);
        }
        /* release: twins, one released at 0.5 s: the ratio of their 64-sample RMS falls monotonically, at most 6 dB a
           block, under -60 dB; the released one ends (0, off) */
        {
            float prev = 0.0f, r = 1.0f, step = 0.0f;
            uint32_t at = 22050u, blk, nb = (uint32_t)((p->rel + 0.2f) * 44100.0f) / 64u;
            int mono = 1;
            v.dv = w.dv = 1.0f;
            samp_start(&v, p, 60.0f, 0.8f);
            samp_start(&w, p, 60.0f, 0.8f);
            sp_run(&v, sp_a, at);
            sp_run(&w, sp_b, at);
            samp_release(&v);
            for (blk = 0; blk < nb && r > 1e-3f; blk++) {
                float ra, rb, dbs;
                sp_run(&v, sp_a, 64u);
                sp_run(&w, sp_b, 64u);
                ra = sp_rms(sp_a, 64u);
                rb = sp_rms(sp_b, 64u);
                r = rb > 0.0f ? ra / rb : 0.0f;
                if (blk && r > prev * 1.0001f)
                    mono = 0;
                if (blk && r > 0.0f) {
                    dbs = 20.0f * 0.30103f * fm_log2f(prev / r);
                    step = fm_maxf(step, dbs);
                }
                prev = r;
            }
            sp_run(&v, sp_a, 44100u);
            printf("samp: bank %d release: %u blocks to %.1f dB, steepest %.2f dB a block, %s\n", bk, (unsigned)blk,
                   r > 0.0f ? (double)(20.0f * 0.30103f * fm_log2f(r)) : -999.0, (double)step,
                   mono ? "monotonic" : "NOT monotonic");
            CHECK(mono && r <= 1e-3f && step <= 6.0f, "samp: bank %d release: monotonic %d, %.4f at the end, steepest "
                  "%.2f dB a block", bk, mono, (double)r, (double)step);
            CHECK(!v.on && sp_a[44099] == 0.0f, "samp: bank %d released voice still on", bk);
        }
        /* the loop: a held note in the mix, 3 s, no step over 0.05 after the attack */
        for (k = 0; k < 2; k++) {
            float pv = 0.0f, d = 0.0f;
            int j;
            vx_init();
            vx_assign(V_CHORD, SP_INS[bk]);
            vt_ev(V_CHORD, k ? 71 : 52, 100, 0, 200000);
            vt_maxdelta(4096, 4096, &pv);
            for (j = 0; j < 32; j++)
                d = fm_maxf(d, vt_maxdelta(4096, 0, &pv));
            printf("samp: bank %d note %d held 3 s: max step %.4f\n", bk, k ? 71 : 52, (double)d);
            CHECK(d <= 0.05f, "samp: bank %d note %d held: a step of %.4f (a loop click?)", bk, k ? 71 : 52, (double)d);
        }
        /* velocity 1 and 127 */
        for (k = 0; k < 2; k++) {
            int j;
            vx_init();
            vx_assign(V_CHORD, SP_INS[bk]);
            for (j = 0; j < 4; j++)
                vt_ev(V_CHORD, 36 + 14 * j, k ? 127 : 1, 0, 30000);
            for (j = 0; j < 30; j++) {
                vt_block(4096, &bad);
                if (j == 5 || j == 29)
                    CHECK(vx_tiny() == 0, "samp: bank %d velocity %d: denormal-bound state", bk, k ? 127 : 1);
            }
        }
    }
    printf("samp: worst pitch %.2f cents\n", (double)worst);
    CHECK(!bad, "samp: NaN or inf");

    /* a steal: eight held Knight notes (a sampled instrument has the chords' 8 voices), a ninth (velocity 1) takes
       the oldest. The victim's part is the difference of
       two renders that differ only in its velocity (127, 63; the chorus, the delay and the glue off: no delayed
       copies, linear). In the 6-16 ms after the steal it is under a tenth of the 10 ms before (the plate's ring of
       it stays); no step over 0.05 */
    {
        static float s1[6 * 4096], s2[6 * 4096];
        float pre, post, late, pv = 0.0f, d;
        int pass, j;
        vx_fx_off(VX_OFF_CHORUS | VX_OFF_DELAY);
        fx_glue_off(1);
        for (pass = 0; pass < 2; pass++) {
            float *o = pass ? s2 : s1;
            vx_init();
            vx_assign(V_CHORD, INS_KNIGHT);
            vt_ev(V_CHORD, 48, pass ? 63 : 127, 0, 400000);
            for (j = 1; j < 8; j++)
                vt_ev(V_CHORD, 48 + 3 * j, 100, 0, 400000);
            for (j = 0; j < 6; j++) {
                if (j == 2)
                    vt_ev(V_CHORD, 74, 1, 0, 400000);
                d = vt_maxdelta(4096, 0, &pv);
                if (j == 2 && !pass)
                    CHECK(d <= 0.05f, "samp: the steal steps %.4f", (double)d);
                memcpy(o + 4096 * j, vt_l, 4096 * sizeof(float));
            }
        }
        for (j = 0; j < 6 * 4096; j++)
            s1[j] -= s2[j];
        pre = sp_rms(s1 + 8192 - 441, 441);
        post = sp_rms(s1 + 8192 + 265, 441);
        late = sp_rms(s1 + 8192 + 5000, 441);
        printf("samp: steal: the victim %.2e before, %.2e 6-16 ms after (%.1f dB), %.2e 120 ms after (the plate)\n",
               (double)pre, (double)post, (double)(20.0f * 0.30103f * fm_log2f(post / pre)), (double)late);
        CHECK(post <= 0.1f * pre, "samp: a stolen voice still at %.1f dB 6 ms after the steal",
              (double)(20.0f * 0.30103f * fm_log2f(post / pre)));
        vx_fx_off(0u);
        fx_glue_off(0);
    }

    /* levels: in four Rhodes dusty songs, each sampled instrument on the keys and on the lead within 1.5 dB of the
       Rhodes and the flute (bars' mean energy; the nylon sits about 0.6 dB under, its sets' lead over keys) */
    {
        uint32_t seed;
        int got = 0;
        double dk[SB_N] = {0}, dl[SB_N] = {0};
        for (seed = 1u; got < 4 && seed < 600u; seed++) {
            song_roll_t r;
            float ref, refl;
            roll_song(NULL, seed, &r);
            if (roll_set(&r, RF_SET, 1) != 1)
                continue;
            got++;
            ref = sp_lk_mean(&r, V_CHORD, INS_NONE, INS_NONE);
            refl = sp_lk_mean(&r, V_LEAD, INS_NONE, INS_NONE);
            for (k = 0; k < SB_N; k++) {
                dk[k] += (double)(sp_lk_mean(&r, V_CHORD, SP_INS[k], INS_NONE) - ref);
                dl[k] += (double)(sp_lk_mean(&r, V_LEAD, INS_NONE, SP_INS[k]) - refl);
            }
        }
        for (k = 0; k < SB_N; k++) {
            dk[k] /= got;
            dl[k] /= got;
            printf("samp: ins %d on the keys %+.2f dB from the Rhodes, on the lead %+.2f dB from the flute\n",
                   SP_INS[k], dk[k], dl[k]);
            CHECK(dk[k] >= -1.5 && dk[k] <= 1.5 && dl[k] >= -1.5 && dl[k] <= 1.5,
                  "samp: ins %d's level: keys %+.2f dB from the Rhodes, lead %+.2f from the flute", SP_INS[k], dk[k],
                  dl[k]);
        }
    }

    /* carryover: seed 2 (Bossa nylon) the same alone and after a song with the Knight on the keys */
    {
        song_roll_t ra, rb;
        uint32_t j;
        float tail, d0 = 0.0f, d1 = 0.0f;
        roll_song(NULL, 24u, &ra);
        roll_song(NULL, 2u, &rb);
        (void)roll_set(&rb, RF_SET, 2);
        vx_init();
        vx_song(&rb);
        cv_play(&rb, CV_SECS, cv_ref_l, cv_ref_r);
        tail = cv_song_a(&ra, INS_KNIGHT, 1u);
        cv_song_a(&ra, INS_KNIGHT, 0u);
        cv_begin(&rb);
        cv_play(&rb, CV_SECS, cv_l, cv_r);
        for (j = 0; j < CV_SECS * 44100u; j++) {
            float d = fm_maxf(fm_fabsf(cv_l[j] - cv_ref_l[j]), fm_fabsf(cv_r[j] - cv_ref_r[j]));
            if (j < 44100u)
                d0 = fm_maxf(d0, d);
            else if (j >= 12u * 44100u)
                d1 = fm_maxf(d1, d);
        }
        printf("samp: seed 2 after a Knight song: first second %.2e (tail %.2e), from 12 s %.2e\n", (double)d0,
               (double)tail, (double)d1);
        CHECK(d1 <= 1e-6f, "samp: seed 2 after a Knight song differs from 12 s by %.3e", (double)d1);
        CHECK(d0 <= 2.0f * tail + 1e-6f, "samp: seed 2 after a Knight song: first second off by %.3e", (double)d0);
    }
}

int main(int argc, char **argv)
{
    const char *only = argc > 1 ? argv[1] : NULL;
#define RUN(t) do { if (!only || strstr(#t, only)) t(); } while (0)
    RUN(test_rng);
    RUN(test_voice_leading);
    RUN(test_no_chord_clusters);
    RUN(test_bass_and_roots);
    RUN(test_keys_bass_rhythms_differ);
    RUN(test_keys_kick_differ);
    RUN(test_lead_by_class);
    RUN(test_lead_where);
    RUN(test_lead_push_legal);
    RUN(test_comping_makes_room);
    RUN(test_half_comping);
    RUN(test_register_floor);
    RUN(test_pad_places);
    RUN(test_hybrid_follows_motion);
    RUN(test_never_both_silent);
    RUN(test_pushes);
    RUN(test_loop_voicings);
    RUN(test_dominant_extensions);
    RUN(test_no_substitution);
    RUN(test_theory);
    RUN(test_library);
    RUN(test_roll);
    RUN(test_synth_sets);
    RUN(test_plan);
    RUN(test_groove);
    RUN(test_harmony);
    RUN(test_melody);
    RUN(test_voices);
    RUN(test_voices_fix);
    RUN(test_vx_patches);
    RUN(test_pad_voice);
    RUN(test_role_buses);
    RUN(test_pad_rates);
    RUN(test_pad_lanes);
    RUN(test_pad_bus_filter);
    RUN(test_pad_solo_voice);
    RUN(test_pad_solo_patch);
    RUN(test_pad_chorus);
    RUN(test_vx_keys);
    RUN(test_vx_plucks_basses);
    RUN(test_vx_leads);
    RUN(test_chip);
    RUN(test_drum_kits);
    RUN(test_fx_chain);
    RUN(test_fx_engine);
    RUN(test_song_carryover);
    RUN(test_mood_reconfigures);
    RUN(test_moves);
    RUN(test_dynamics);
    RUN(test_wet);
    RUN(test_cost_moves);
    RUN(test_sub);
    RUN(test_glide_songs);
    RUN(test_fm_patches);
    RUN(test_rings_pair);
    RUN(test_mix_parts);
    RUN(test_mix_denormals);
    RUN(test_levels_steady);
    RUN(test_keys_duck);
    RUN(test_keys_duck_mix);
    RUN(test_lead_over_keys);
    RUN(test_pad_level);
    RUN(test_phrase_plan);
    RUN(test_sustained_phrases);
    RUN(test_sustained_motion);
    RUN(test_sustained_key_request);
    RUN(test_sustained_tied_home);
    RUN(test_lead_full_plan);
    RUN(test_pad_chord);
    RUN(test_pad_solo);
    RUN(test_pad_sections);
    RUN(test_pad_either);
    RUN(test_pad_determinism);
    RUN(test_pad_double);
    RUN(test_strum);
    RUN(test_samples_table);
    RUN(test_samp);
    RUN(test_comp_styles);
    if (fails) {
        printf("%d check(s) failed\n", fails);
        return 1;
    }
    printf("song: all checks passed\n");
    return 0;
}

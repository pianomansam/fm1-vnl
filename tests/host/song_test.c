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

static int upright_set(void)
{
    int i;
    for (i = 0; i < LIB_NSETS; i++)
        if (LIB_SETS[i].bass == INS_UPRIGHT) return LIB_SETS[i].id;
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
    int styles[4] = {0, 0, 0, 0}, chords_seen = 0;
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
                        int up = harmony_pad_place(&r) == PL_BELOW ? 0 : 12;
                        if (ev[i].note < 48 + up || ev[i].note > 72 + up) bad_range++;
                    }
                    if (ev[i].vel < 25 || ev[i].vel > 110) bad_vel++;
                    if (ev[i].len == 0) bad_range++;
                }
                if (harmony_style(&r) != CS_ARP && ne > 0) {
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
    CHECK(styles[CS_HELD] > 0 && styles[CS_STABS] > 0 && styles[CS_OFFBEATS] > 0 && styles[CS_ARP] > 0,
          "harmony: styles drawn %d/%d/%d/%d", styles[0], styles[1], styles[2], styles[3]);
    {   /* (Sam, after hearing comping-styles.musicxml) mostly held: 55% held, 20% stabs, 15% offbeats, 10% arpeggio */
        int w[4] = {0, 0, 0, 0}, want[4] = {55, 20, 15, 10}, k2;
        song_roll_t q;
        roll_song(NULL, 1u, &q);
        for (k2 = 0; k2 < 10000; k2++) {
            q.seed = (uint32_t)k2 * 2654435761u + 1u;
            w[harmony_style(&q)]++;
        }
        for (k2 = 0; k2 < 4; k2++)
            CHECK(w[k2] >= want[k2] * 100 - 200 && w[k2] <= want[k2] * 100 + 200,
                  "harmony: style %d drawn %d of 10000, want %d%% (+-2)", k2, w[k2], want[k2]);
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

/* The plucked strings (M_PLUCK: nylon, guitar lick, upright) and the basses (M_BASS: sub, electric bass). A pluck is
   in tune by its period (within 10 cents) at 40, 55 and 70; nylon falls to -40 dB within 3 s and upright within
   1.5 s, held; the Karplus-Strong line is sized for note 28. The sub's 2x and 3x partials are each 12 dB under the
   fundamental; the electric bass's attack is brighter and settles within 100 ms. */
static void test_vx_plucks_basses(void)
{
    static const int PLUCKS[3] = {INS_NYLON, INS_GUITAR_LICK, INS_UPRIGHT};
    static const int NOTES[3] = {40, 55, 70};
    int i, j, bad = 0;

    CHECK(vx_patch(INS_NYLON)->model == M_PLUCK && vx_patch(INS_GUITAR_LICK)->model == M_PLUCK &&
              vx_patch(INS_UPRIGHT)->model == M_PLUCK && vx_patch(INS_SUB)->model == M_BASS &&
              vx_patch(INS_ELEC_BASS)->model == M_BASS,
          "plucks and basses on the wrong models");
    CHECK(VX_KS_N <= 44100 / 41 && (float)VX_KS_N >= 44100.0f / 41.2034f + 1.0f,
          "the Karplus-Strong line is %d samples: wanted note 28's period (+1 for the interpolation) to 44100/41",
          VX_KS_N);
    for (i = 0; i < 3; i++)
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
    for (i = 0; i < 3; i += 2)
        for (j = 0; j < 3; j++) { /* held 5 s: the string's own decay */
            float t = PLUCKS[i] == INS_NYLON ? 3.0f : 1.5f, pk, at;
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

static void test_vx_patches(void)
{
    int ins, s, bad = 0;

    CHECK(vx_patch(INS_NONE) == NULL, "INS_NONE has a patch");
    for (ins = 0; ins < INS_NONE; ins++) {
        const vx_patch_t *p = vx_patch(ins);
        int role, k, silent_at = -1;
        float peak = 0.0f, tail = 0.0f;
        uint32_t off_blocks;
        CHECK(p != NULL && p->level > 0.0f && p->model <= M_BASS, "ins %d: no patch, level 0 or bad model", ins);
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
                  c->kit == t->kit && c->flavor == t->flavor,
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
/* The lead plays like its instrument (lead design): a sustained lead (flute, sax, muted trumpet) never runs an
   arpeggio; a percussive one sings in most bars where it may play. Also where the lead may play, and pushes legal
   over both chords. One pass over 300 chained songs feeds all three tests. */
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
        int sust, run = 0, runpcs = 0, last_at = -100, cur = -1, led = 0, may = 0;
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
                    if (ct && at - last_at <= 2) { run++; runpcs |= 1 << (note % 12); }
                    else { run = ct; runpcs = ct ? 1 << (note % 12) : 0; }
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
    CHECK(lr_runs == 0, "sustained leads ran %ld arpeggios", lr_runs);
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
    long strikes[4][2] = {{0}}, bars[4][2] = {{0}}, bad_same = 0, bad_subset = 0;
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
                if (sust_keys || cs == CS_HELD) {
                    if (n0 != n1 || memcmp(e0, e1, sizeof(song_ev_t) * (size_t)n0)) bad_same++;
                } else if (n1 && (n1 != n0 || memcmp(e0, e1, sizeof(song_ev_t) * (size_t)n0))) {
                    bad_subset++;
                }
            }
        }
    }
    for (cs = CS_STABS; cs <= CS_ARP; cs++) {
        double q = bars[cs][0] ? (double)strikes[cs][0] / (double)bars[cs][0] : 0.0;
        double bz = bars[cs][1] ? (double)strikes[cs][1] / (double)bars[cs][1] : 0.0;
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
                if (ev[i].voice == V_CHORD && pl != PL_BELOW && ev[i].note < 60) shifted_low++;
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

/* Lead design: the sung line (a sustained lead's skeleton) wanders: 4+ distinct skeleton notes in at least 70% of
   sustained-lead songs (75% measured; the spike's 19%, song 1 only alternating C5 and Db5), and a leap past a 5th
   in at most 0.3% of skeleton steps (only where a 4th over percussive keys' top leaves no closer note). */
static void test_line_wanders(void)
{
    static song_t s;
    uint32_t seed = 7171;
    song_roll_t prev, r;
    song_ev_t ev[64];
    long songs = 0, wide = 0, leaps = 0, steps = 0;
    int k;
    for (k = 0; k < 300; k++) {
        const lib_set_t *set;
        const lib_prog_t *pr;
        int seen[128] = {0}, distinct = 0, last = 0, bpc;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        roll_song(k ? &prev : 0, seed, &r);
        prev = r;
        set = lib_set_by_id((uint8_t)r.v[RF_SET]);
        if (!set || !lib_ins_sustained(set->lead)) continue;
        pr = lib_prog_by_id((uint8_t)r.v[RF_PROG]);
        bpc = pr && pr->bars_per_chord ? pr->bars_per_chord : 1;
        song_start(&s, &r);
        while (s.state == SS_PLAYING) {
            int bar = s.bar, st = s.step;
            const plan_block_t *b = plan_at(&s.plan, bar);
            song_tick(&s, ev, 64);
            if (st != 0 || bar % bpc || !lead_plays(b) || s.lead.last <= 0) continue;
            if (!seen[s.lead.last]++) distinct++;
            if (last) {
                steps++;
                leaps += (s.lead.last > last ? s.lead.last - last : last - s.lead.last) > 7;
            }
            last = s.lead.last;
        }
        if (last) {
            songs++;
            wide += distinct >= 4;
        }
    }
    printf("line: %ld of %ld sustained-lead songs reach 4+ skeleton notes; %ld of %ld steps "
                                   "leap past a 5th\n", wide, songs, leaps, steps);
    CHECK(songs > 30 && wide * 100 >= songs * 70, "the line reaches 4+ notes in %ld of %ld songs", wide, songs);
    CHECK(leaps * 1000 <= steps * 3, "%ld of %ld skeleton steps leap past a 5th", leaps, steps);
}

/* Sam: the hybrid lead follows the motion lever: in songs with motion <= 35 it plays at most 3/4 the onsets a lead
   bar of songs with motion >= 65, and its skeleton landings sound as often at low motion as at high. */
static void test_hybrid_follows_motion(void)
{
    static song_t s;
    uint32_t seed = 8181;
    song_roll_t prev, r;
    song_ev_t ev[64];
    long on[2] = {0}, bars[2] = {0}, changes[2] = {0}, landed[2] = {0};
    int k, i;
    for (k = 0; k < 600; k++) {
        const lib_set_t *set;
        const lib_prog_t *pr;
        int m, hi, cur = -1, cnt = 0, bpc, lead_bar = 0;
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
                if (lead_bar) { on[hi] += cnt; bars[hi]++; }
                cur = bar;
                cnt = 0;
                lead_bar = lead_plays(b);
            }
            if (st == 0 && lead_bar && bar % bpc == 0) {
                changes[hi]++;
                landed[hi] += s.lead.acc != 0 || s.lead.active > (s.lead.onsets > 0);
            }
            for (i = 0; i < n; i++) cnt += ev[i].voice == V_LEAD;
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
   instead; and no push rings into a bar where a request lands (the next chord isn't known yet). */
static void test_pushes(void)
{
    static song_t s;
    uint32_t seed = 1212;
    song_roll_t prev, r;
    song_ev_t ev[64];
    long pushes = 0, short_push = 0, rejected = 0, late = 0, req_bars = 0, into_req = 0;
    int k, i;
    for (k = 0; k < 300; k++) {
        const lib_set_t *set;
        const lib_prog_t *pr;
        int bpc, hybrid, pushed_in = 0, tied_now = 0, req_at = -1, cur = -1;
        uint32_t lead_end = 0;
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        roll_song(k ? &prev : 0, seed, &r);
        prev = r;
        set = lib_set_by_id((uint8_t)r.v[RF_SET]);
        hybrid = set && !lib_ins_sustained(set->lead);
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
                if (bar == req_at) { req_bars++; into_req += lead_end > now; }
                cur = bar;
                pushed_in = tied_now;
            }
            n = song_tick(&s, ev, 64);
            if (st == 0) tied_now = lead_plays(b) && s.lead.tied;
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
    CHECK(dense > 5 && moved == 0, "%ld of %ld stab songs over a busy kick moved the stab", moved, dense);
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
#define SB_N (8u * 44100u)
static float sb_l[SB_N], sb_r[SB_N], sb_x[SB_N];

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
        ok &= a[r] == 0.6f && b[r] == 0.0f;
        plan_moves(&p, 9, 9, a, b);
        ok &= a[r] == 0.8f && b[r] == 0.0f;
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
        for (seed = 1; seed < 3000 && !found; seed++) {
            song_roll_t roll;
            song_plan_t q;
            int i;
            roll_song(NULL, seed, &roll);
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
                if (i && !(rat[i] > rat[i - 1]))
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
            for (b = 0; b < (int)(SB_N / 2205u); b++) {
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
            for (i = 3 * 44100; i < (int)SB_N; i++)
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
    static const int CH[6] = {INS_RHODES, INS_WURLI, INS_FELT_PIANO, INS_NYLON, INS_PAD, INS_VOX_PAD};
    static const int NOTES[4] = {48, 60, 72, 84}, VELS[2] = {64, 110};
    float worst_band = 0.0f, worst_alias = -999.0f;
    int c, j, v, b;

    /* half-rate keys and pads: every chords-role patch, notes to MIDI 84, two velocities. Per octave band below 8 kHz
       the half-rate render's energy is within 1 dB of the full rate's; no aliasing or imaging product reaches -50 dB
       (or, where the full-rate render already has one there, it grows by under 1 dB) */
    for (c = 0; c < 6; c++)
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
       the difference of two renders whose noise differs (a silent guitar-lick pluck first draws from the noise), is
       within 0.5 dB of the full rate's, over three notes and two velocities */
    {
        static const int BN[3] = {48, 60, 67}, BV[2] = {100, 127};
        double eb[2] = {0.0, 0.0};
        int full, nn, vv, pass;
        for (full = 0; full < 2; full++)
            for (nn = 0; nn < 3; nn++)
                for (vv = 0; vv < 2; vv++) {
                    for (pass = 0; pass < 2; pass++) {
                        vx_fx_off(full ? VX_OFF_HALF : 0u);
                        vx_init();
                        vx_assign(V_CHORD, INS_WURLI);
                        vx_assign(V_LEAD, INS_GUITAR_LICK);
                        if (pass)
                            vt_ev(V_LEAD, 60, 0, 0, 100);
                        vt_ev(V_CHORD, BN[nn], BV[vv], 0, 8820);
                        memset(vt_l, 0, 1764 * sizeof(float));
                        memset(vt_r, 0, 1764 * sizeof(float));
                        vx_render(vt_l, vt_r, 1764);
                        memcpy(pass ? cm_y : cm_x, vt_l, 1764 * sizeof(float));
                    }
                    for (b = 0; b < 1764; b++)
                        eb[full] += (double)(cm_x[b] - cm_y[b]) * (double)(cm_x[b] - cm_y[b]);
                }
        vx_fx_off(0u);
        printf("half rate: the Wurli's bark %.2f dB against the full rate's\n", (double)db_of(eb[0] / eb[1]));
        CHECK(eb[1] > 0.0 && fm_fabsf(db_of(eb[0] / eb[1])) <= 0.5f,
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
static void lv_song(uint32_t seed, int bed)
{
    static song_t s;
    song_ev_t ev[64];
    float frac = 0.0f;
    memset(&lv, 0, sizeof lv);
    roll_song(NULL, seed, &lv.roll);
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

static int lv_cmp(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;
    return x < y ? -1 : x > y;
}

/* Step 1: over 50 rolled songs, each song's full-band RMS within 3 dB of the median song's; inside a song no block's
   RMS more than 6 dB from the last full-level block's (intros, outros, breaks and fades may be quieter, never louder
   than the song's full band); the peak under 0 dBFS. */
#define LV_N 50
static float lv_song_db[LV_N], lv_song_gr[LV_N], lv_intro_gr[LV_N], lv_intro_db[LV_N], lv_worst_jump;
static void test_levels_steady(void)
{
    float sorted[LV_N], med, peak = 0.0f, worst = 0.0f;
    int k, bad_jump = 0, bad_quiet = 0;
    lv_worst_jump = 0.0f;
    for (k = 0; k < LV_N; k++) {
        const lib_set_t *set;
        float full, prev = -999.0f;
        double e = 0.0;
        uint32_t n = 0;
        int j, b;
        lv_song((uint32_t)k + 1u, 0);
        set = lib_set_by_id((uint8_t)lv.roll.v[RF_SET]);
        for (j = 0; j < lv.plan.n; j++)
            if (lv_full(&lv.plan.b[j]))
                for (b = lv.plan.b[j].start; b < lv.plan.b[j].start + lv.plan.b[j].bars && b < LV_BARS; b++) {
                    e += lv.e[b];
                    n += lv.n[b];
                }
        full = n ? db_of(e / n) : -999.0f;
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
                    printf("levels: seed %d block %d (%s) %.1f dB, louder than the full band's %.1f\n", k + 1, j,
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
                    printf("levels: seed %d block %d (%s) %.1f dB after %.1f\n", k + 1, j,
                           plan_section_name(pb->section), (double)d, (double)prev);
                }
            }
            prev = d;
        }
        printf("levels: seed %2d set %-17s full %.1f dB, glue %.1f dB; intro %.1f dB, glue %.1f; peak %.3f\n", k + 1,
               set ? set->name : "?", (double)full, (double)lv_song_gr[k], (double)lv_intro_db[k],
               (double)lv_intro_gr[k], (double)lv.peak);
    }
    memcpy(sorted, lv_song_db, sizeof sorted);
    qsort(sorted, LV_N, sizeof sorted[0], lv_cmp);
    med = 0.5f * (sorted[LV_N / 2 - 1] + sorted[LV_N / 2]);
    for (k = 0; k < LV_N; k++)
        worst = fm_maxf(worst, fm_fabsf(lv_song_db[k] - med));
    printf("levels: median %.1f dB, spread %.1f..%.1f (worst %.1f off), worst block jump %.1f dB, peak %.3f\n",
           (double)med, (double)sorted[0], (double)sorted[LV_N - 1], (double)worst, (double)lv_worst_jump,
           (double)peak);
    for (k = 0; k < LV_N; k++)
        CHECK(fm_fabsf(lv_song_db[k] - med) <= 3.0f, "levels: seed %d's full band %.1f dB, the median %.1f", k + 1,
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
            for (k = 0; k < LV_N; k++) {
                float d = 20.0f * fm_log2f(bed[j]) * 0.30103f - lv_song_db[k];
                lo = fm_minf(lo, d);
                hi = fm_maxf(hi, d);
                CHECK(d >= -40.0f && d <= -30.0f, "the bed (%s) %.1f dB under seed %d's music, wanted 30-40",
                      j ? "tape" : "vinyl and rain", (double)-d, k + 1);
            }
            printf("levels: the bed (%s) %.1f..%.1f dB under the songs' music\n", j ? "tape" : "vinyl and rain",
                   (double)-hi, (double)-lo);
        }
    }
    /* the glue: 1-4 dB on average over a song's full band, under 1 dB in its intro */
    for (k = 0; k < LV_N; k++) {
        CHECK(lv_song_gr[k] >= 1.0f && lv_song_gr[k] <= 4.0f, "glue: seed %d's full band %.2f dB, wanted 1-4", k + 1,
              (double)lv_song_gr[k]);
        CHECK(lv_intro_gr[k] < 1.0f, "glue: seed %d's intro %.2f dB, wanted under 1", k + 1, (double)lv_intro_gr[k]);
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
    RUN(test_register_floor);
    RUN(test_pad_places);
    RUN(test_line_wanders);
    RUN(test_hybrid_follows_motion);
    RUN(test_never_both_silent);
    RUN(test_pushes);
    RUN(test_loop_voicings);
    RUN(test_dominant_extensions);
    RUN(test_no_substitution);
    RUN(test_theory);
    RUN(test_library);
    RUN(test_roll);
    RUN(test_plan);
    RUN(test_groove);
    RUN(test_harmony);
    RUN(test_melody);
    RUN(test_voices);
    RUN(test_voices_fix);
    RUN(test_vx_patches);
    RUN(test_vx_keys);
    RUN(test_vx_plucks_basses);
    RUN(test_vx_leads);
    RUN(test_drum_kits);
    RUN(test_fx_chain);
    RUN(test_fx_engine);
    RUN(test_moves);
    RUN(test_dynamics);
    RUN(test_wet);
    RUN(test_cost_moves);
    RUN(test_rings_pair);
    RUN(test_mix_parts);
    RUN(test_levels_steady);
    if (fails) {
        printf("%d check(s) failed\n", fails);
        return 1;
    }
    printf("song: all checks passed\n");
    return 0;
}

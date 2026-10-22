/* SPDX-License-Identifier: MPL-2.0 */
/* The song engine on the host: seeded random streams (and, in later tasks, the rest of app/song). */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "rng.h"
#include "theory.h"
#include "library.h"
#include "roll.h"
#include "plan.h"

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
                        if (out[i] > 72 || out[i] < 48)
                            bad++;
                        for (k = i + 1; k < 4; k++)
                            if (out[i] == out[k])
                                bad++;
                    }
                    if (out[0] < 48 || out[0] > 55)
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
    {   /* spot-check the claimed chords by name */
        static const struct { int id, mode; const char *want; } sp[] = {
            {9, MODE_IONIAN, "Fmaj7 G7 Em7 Am7"}, {15, MODE_IONIAN, "Dm7 Db7 Cmaj7 Cmaj7"},
            {6, MODE_DORIAN, "Cm7 Bb7"}, {18, MODE_AEOLIAN, "Cm7 Bb7 Abmaj7 G7"}, {13, MODE_MIXOLYDIAN, "Fmaj7 E7 Am7 C7"},
            {22, MODE_AEOLIAN, "Cm7 Fm7 Abmaj7 G7"}, {24, MODE_DORIAN, "Cm7 Dbmaj7"},
            {25, MODE_DORIAN, "Cm7 F7 Bbmaj7 Ebmaj7"}, {26, MODE_AEOLIAN, "Cm7 Dm7 Ebmaj7 F7"},
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
        /* the real library: sad + Lydian allows exactly progressions 5 and 10 */
        roll_set(&cur, RF_MOOD, 0);
        CHECK(roll_set(&cur, RF_MODE, MODE_LYDIAN) == MODE_LYDIAN, "sad Lydian pickable");
        CHECK(prog_legal(&cur, cur.v[RF_PROG]), "prog re-picked on mode change");
        k = cur.v[RF_PROG];
        CHECK(roll_dice(&cur, RF_PROG, 9) == 1 && cur.v[RF_PROG] == (k == 5 ? 10 : 5), "sad Lydian dice");
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
    int bad_m0 = 0, bad_at = 0;
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
                if (b->section != SEC_INTRO && b->section != SEC_OUTRO) {
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

int main(void)
{
    test_rng();
    test_theory();
    test_library();
    test_roll();
    test_plan();
    if (fails) {
        printf("%d check(s) failed\n", fails);
        return 1;
    }
    printf("song: all checks passed\n");
    return 0;
}

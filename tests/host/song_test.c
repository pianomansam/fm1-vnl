/* SPDX-License-Identifier: MPL-2.0 */
/* The song engine on the host: seeded random streams (and, in later tasks, the rest of app/song). */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "rng.h"
#include "theory.h"
#include "library.h"

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

int main(void)
{
    test_rng();
    test_theory();
    test_library();
    if (fails) {
        printf("%d check(s) failed\n", fails);
        return 1;
    }
    printf("song: all checks passed\n");
    return 0;
}

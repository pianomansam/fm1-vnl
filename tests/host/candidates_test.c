/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Candidates are workbench-only until approved (patterns and edits, Task 4). Built against the real generated tables
   and against tools/patterns/variant.py's (one entry added to every drawn family: a candidate, or approved) by
   tests/candidates.sh:
     candidates_test never   no candidate is drawn by the roll, the dice or the strum over 1000 seeds; every candidate can be forced (roll_set, the overrides) and it plays
     candidates_test drawn   the last entry of every drawn table (variant.py's, approved) is drawn at least once
   Both print "songs <hash>": every event of 100 chained songs, which candidates.sh compares across the builds. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "library.h"
#include "roll.h"
#include "harmony.h"
#include "strum.h"
#include "song.h"
#include "override.h"
#include "patterns_gen.h"

static int fails;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        if (!(c)) {                                                                                                \
            if (fails < 40) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); }      \
            fails++;                                                                                               \
        }                                                                                                          \
    } while (0)

static const song_ov_t NONE = {-1, -1, -1, -1, 0, 0, 0u, 0u, 0u, 0u, 0u, -1, -1, 0};

static int prog_index(int id)
{
    const lib_prog_t *p = lib_prog_by_id((uint8_t)id);
    return p ? (int)(p - LIB_PROGS) : -1;
}
static int pattern_index(int id)
{
    const lib_pattern_t *p = lib_pattern_by_id((uint8_t)id);
    return p ? (int)(p - LIB_PATTERNS) : -1;
}

static int prog_ok_here(const song_roll_t *r)
{
    const lib_prog_t *p = lib_prog_by_id((uint8_t)r->v[RF_PROG]);
    return p && (p->moods >> r->v[RF_MOOD] & 1) && (p->modes >> r->v[RF_MODE] & 1);
}

static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

/* every event of one song (voice mask: the voices hashed) */
static uint32_t song_hash(const song_roll_t *r, unsigned voices)
{
    static song_t s;
    song_ev_t ev[64];
    uint32_t h = 2166136261u;
    int i, n;
    song_start(&s, r);
    while (s.state == SS_PLAYING) {
        n = song_tick(&s, ev, 64);
        for (i = 0; i < n; i++) {
            if (!(voices >> ev[i].voice & 1u)) continue;
            h = hash(h, ev[i].voice); h = hash(h, ev[i].note); h = hash(h, ev[i].vel);
            h = hash(h, ev[i].pad); h = hash(h, ev[i].delay); h = hash(h, ev[i].len);
        }
    }
    return h;
}

/* 100 chained songs (the novelty passes run), every event */
static uint32_t songs_hash(void)
{
    song_roll_t prev, r;
    uint32_t h = 2166136261u, seed;
    for (seed = 1; seed <= 100; seed++) {
        roll_song(seed > 1 ? &prev : NULL, seed, &r);
        h = hash(h, song_hash(&r, ~0u));
        prev = r;
    }
    return h;
}

/* what the roll and the parts draw over 1000 chained seeds, counted per table entry */
typedef struct { int prog[256], pattern[256], strum[64]; } drawn_t;

static void draw_all(drawn_t *d)
{
    song_roll_t prev, r;
    uint32_t seed, k;
    int slow;
    memset(d, 0, sizeof *d);
    for (seed = 1; seed <= 1000; seed++) {
        roll_song(seed > 1 ? &prev : NULL, seed, &r);
        prev = r;
        d->prog[prog_index(r.v[RF_PROG])]++;
        d->pattern[pattern_index(r.v[RF_PATTERN])]++;
        for (k = 1; k <= 3; k++) {          /* the dice too */
            song_roll_t t = r;
            if (roll_dice(&t, RF_PROG, k)) d->prog[prog_index(t.v[RF_PROG])]++;
            t = r;
            if (roll_dice(&t, RF_PATTERN, k)) d->pattern[pattern_index(t.v[RF_PATTERN])]++;
            t = r;
            if (roll_dice(&t, RF_MOOD, k)) d->prog[prog_index(t.v[RF_PROG])]++;   /* a re-picked progression */
            t = r;                              /* a mood request (the firmware's): the mode and progression kept legal */
            roll_set(&t, RF_MOOD, (int)((r.v[RF_MOOD] + k) % 4));
            d->prog[prog_index(t.v[RF_PROG])]++;
        }
        d->strum[strum_pattern(&r, &slow)]++;
    }
}

static void test_never(void)
{
    drawn_t d;
    int i, n = 0;
    draw_all(&d);
    for (i = 0; i < LIB_NPROGS; i++) CHECK(LIB_PROGS_STATUS[i] == 1 || !d.prog[i], "candidate prog %d drawn %d times", LIB_PROGS[i].id, d.prog[i]);
    for (i = 0; i < LIB_NPATTERNS; i++) CHECK(LIB_PATTERNS_STATUS[i] == 1 || !d.pattern[i], "candidate pattern %d drawn", LIB_PATTERNS[i].id);
    for (i = 0; i < STRUM_NPAT; i++) CHECK(STRUM_PAT_STATUS[i] == 1 || !d.strum[i], "candidate strum %d drawn", i);
    for (i = 0; i < LIB_NPROGS; i++) n += LIB_PROGS_STATUS[i] != 1;
    for (i = 0; i < LIB_NPATTERNS; i++) n += LIB_PATTERNS_STATUS[i] != 1;
    for (i = 0; i < STRUM_NPAT; i++) n += STRUM_PAT_STATUS[i] != 1;
    printf("never: %d candidate(s), none drawn over 1000 seeds (the roll, the dice, the strum)\n", n);
}

/* a seed whose song strums (style) */
static int first_seed(int strum, song_roll_t *r)
{
    uint32_t seed;
    for (seed = 1; seed < 400; seed++) {
        roll_song(NULL, seed, r);
        if (!strum || harmony_style(r) == CS_STRUM) return 1;
    }
    return 0;
}

/* every candidate can be forced and changes what plays */
static void test_forced(void)
{
    song_roll_t r, t;
    song_ov_t o;
    int i, slow, forced = 0;
    uint32_t base;
    for (i = 0; i < LIB_NPATTERNS; i++) {
        if (LIB_PATTERNS_STATUS[i] == 1) continue;
        roll_song(NULL, 5, &r);
        t = r;
        CHECK(roll_set(&t, RF_PATTERN, LIB_PATTERNS[i].id) == LIB_PATTERNS[i].id, "pattern %d can be picked", LIB_PATTERNS[i].id);
        CHECK(song_hash(&t, ~0u) != song_hash(&r, ~0u), "pattern %d plays", LIB_PATTERNS[i].id);
        forced++;
    }
    for (i = 0; i < LIB_NPROGS; i++) {
        const lib_prog_t *p = &LIB_PROGS[i];
        int mood = 0, mode = 0;
        if (LIB_PROGS_STATUS[i] == 1) continue;
        while (!(p->moods >> mood & 1)) mood++;
        while (!(p->modes >> mode & 1)) mode++;
        roll_song(NULL, 5, &r);
        roll_set(&r, RF_MOOD, mood);
        CHECK(roll_set(&r, RF_MODE, mode) == mode, "prog %d's mode %d can be picked", p->id, mode);
        CHECK(prog_ok_here(&r), "prog %d: the song's progression suits the picked mode", p->id);
        t = r;
        CHECK(roll_set(&t, RF_PROG, p->id) == p->id, "prog %d can be picked", p->id);
        r = t;                                  /* (raw: the same song on another progression) */
        r.v[RF_PROG] = LIB_PROGS[1].id;
        CHECK(song_hash(&t, ~0u) != song_hash(&r, ~0u), "prog %d plays", p->id);
        forced++;
    }
    CHECK(first_seed(1, &r), "a strummed song");
    base = song_hash(&r, 1u << V_CHORD);
    for (i = 0; i < STRUM_NPAT; i++) {
        if (STRUM_PAT_STATUS[i] == 1) continue;
        o = NONE;
        o.strum_pat = (int8_t)i;
        song_override(&o);
        CHECK(strum_pattern(&r, &slow) == i, "strum %d forced", i);
        CHECK(song_hash(&r, 1u << V_CHORD) != base, "strum %d plays", i);
        song_override(NULL);
        forced++;
    }
    printf("forced: %d candidate(s) picked and heard\n", forced);
}

/* the last entry of each drawn table (variant.py's, approved) is drawn */
static void test_drawn(void)
{
    drawn_t d;
    draw_all(&d);
    CHECK(LIB_PROGS_STATUS[LIB_NPROGS - 1] == 1 && d.prog[LIB_NPROGS - 1] > 0, "the last prog drawn %d times", d.prog[LIB_NPROGS - 1]);
    CHECK(LIB_PATTERNS_STATUS[LIB_NPATTERNS - 1] == 1 && d.pattern[LIB_NPATTERNS - 1] > 0, "the last pattern drawn %d times", d.pattern[LIB_NPATTERNS - 1]);
    CHECK(d.strum[STRUM_NPAT - 1] > 0, "the last strum pattern drawn %d times", d.strum[STRUM_NPAT - 1]);
    printf("drawn: prog %d, pattern %d, strum %d (of 1000 seeds)\n", d.prog[LIB_NPROGS - 1],
           d.pattern[LIB_NPATTERNS - 1], d.strum[STRUM_NPAT - 1]);
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "never";
    if (!strcmp(mode, "drawn")) {
        test_drawn();
    } else {
        test_never();
        test_forced();
    }
    printf("songs %08x\n", (unsigned)songs_hash());
    printf(fails ? "candidates: %d FAILED\n" : "candidates: all checks passed\n", fails);
    return fails ? 1 : 0;
}

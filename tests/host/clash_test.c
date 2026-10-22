/* SPDX-License-Identifier: MPL-2.0 */
/* The sequencer on the host: 1000 chained songs checked for clashes, registers, length and determinism, and the
   boundary-quantized requests (tempo, key, mood, motion) checked where they land. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "theory.h"
#include "library.h"
#include "roll.h"
#include "plan.h"
#include "groove.h"
#include "harmony.h"
#include "song.h"

static int fails;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        if (!(c)) {                                                                                                \
            if (fails < 60) {                                                                                      \
                printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                        \
                printf(__VA_ARGS__);                                                                               \
                printf("\n");                                                                                      \
            }                                                                                                      \
            fails++;                                                                                               \
        }                                                                                                          \
    } while (0)

#define MAXT (76 * 16)
#define MAXE 40000

/* one recorded event, with absolute sample times */
typedef struct { uint32_t start, end; int tick; uint8_t voice, note; } rev_t;

typedef struct {
    int ticks, ended;
    uint32_t hash, samples;
    float ss[MAXT];               /* step samples of each tick */
    uint32_t at[MAXT + 1];        /* start sample of each tick */
    int bar[MAXT], step[MAXT];    /* where each tick was */
    int key[MAXT];                /* the roll's key while it played */
    int first[MAXT + 1];          /* index of each tick's first event */
    int ne;
    rev_t ev[MAXE];
} rec_t;

typedef void (*hook_t)(song_t *s, int tick, void *ctx);

static rec_t R1, R2;
static int clash_checks = 1;
static long strong_checked, strong_skipped, strong_in, weak_checked, rubs[2][4];   /* rubs[strong?0:1][kind] */

static float base_ss(int bpm) { return 44100.0f * 60.0f / ((float)bpm * 4.0f); }

static uint32_t fnv(uint32_t h, uint32_t x)
{
    int i;
    for (i = 0; i < 4; i++) {
        h ^= (x >> (8 * i)) & 0xFFu;
        h *= 16777619u;
    }
    return h;
}

static int pitched(int v) { return v == V_CHORD || v == V_BASS || v == V_LEAD || v == V_CANDY; }

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

/* every rule a single step's events must keep, against the roll and plan the sequencer used for it */
static void check_step(const song_t *s, int bar, int step, const song_ev_t *ev, int n, uint32_t pos,
                       uint32_t *lead_end, uint32_t *candy_end, uint32_t *last_start, uint32_t seed)
{
    const lib_prog_t *pr = lib_prog_by_id((uint8_t)s->roll.v[RF_PROG]);
    const plan_block_t *b = plan_at(&s->plan, bar);
    int loop = pr ? pr->n * pr->bars_per_chord : 4, pass = bar / loop + 1, dark = b && (b->flags & PB_DARK);
    int pcs[5], np, i, j, key = s->roll.v[RF_KEY], mode = s->roll.v[RF_MODE];
    np = harmony_chord_at(&s->roll, bar, pass, dark, pcs);
    for (i = 0; i < n; i++) {
        const song_ev_t *e = &ev[i];
        uint32_t a = pos + e->delay, z = a + e->len;
        CHECK(i == 0 || ev[i - 1].delay <= e->delay, "seed %u bar %d step %d: events not sorted by delay", seed, bar,
              step);
        CHECK(e->vel >= 25 && e->vel <= 110, "seed %u: velocity %d", seed, e->vel);
        CHECK(e->voice < V_N, "seed %u: voice %d", seed, e->voice);
        if (!pitched(e->voice)) {
            CHECK(e->note == 0 && e->len == 0, "seed %u: drum voice %d note %d len %u", seed, e->voice, e->note,
                  e->len);
            continue;
        }
        CHECK(e->len > 0, "seed %u: pitched note with len 0", seed);
        switch (e->voice) {
        case V_CHORD: CHECK(e->note >= 48 && e->note <= 76, "seed %u: chord note %d", seed, e->note); break;
        case V_BASS: CHECK(e->note >= 28 && e->note <= 47, "seed %u: bass note %d", seed, e->note); break;
        case V_LEAD: CHECK(e->note >= 62 && e->note <= 86, "seed %u: lead note %d", seed, e->note); break;
        default: CHECK(e->note >= 70 && e->note <= 98, "seed %u: candy note %d", seed, e->note); break;
        }
        if (e->voice != V_LEAD && e->voice != V_CANDY) continue;
        CHECK(in_chord_scale(key, mode, pcs, np, e->note), "seed %u bar %d step %d: note %d out of the chord-scale",
              seed, bar, step, e->note);
        CHECK(!th_is_avoid(key, mode, pcs, np, e->note), "seed %u bar %d step %d: avoid note %d", seed, bar, step,
              e->note);
        if (step == 0 || step == 8) {
            int ct = 0;
            for (j = 0; j < np; j++) ct |= pcs[j] % 12 == e->note % 12;
            CHECK(ct, "seed %u bar %d step %d: note %d not a chord tone", seed, bar, step, e->note);
        }
        CHECK(a >= *last_start, "seed %u: lead/candy onsets out of order", seed);
        *last_start = a;
        if (e->voice == V_LEAD) {
            CHECK(a >= *candy_end, "seed %u bar %d step %d: lead over candy", seed, bar, step);
            if (z > *lead_end) *lead_end = z;
        } else {
            CHECK(a >= *lead_end, "seed %u bar %d step %d: candy over lead", seed, bar, step);
            if (z > *candy_end) *candy_end = z;
        }
    }
}

/* Checks against what actually sounds, from the recorded events (no harmony derivation):
   - no chord note still sounds when the next chord strike begins;
   - on every step (R25; R20 checked only steps 0 and 8), every lead/candy note starting then or held into it, against
     the chord-part and bass notes sounding at that moment (the later of the step and the note's start): struck at or
     before the step and not yet ended, the notes of a chord rolled from this very step included. None of them may lie
     a semitone above or below it (a hard failure, tested first, so a pitch class heard elsewhere excuses nothing). On
     steps 0 and 8, whether it is one of them is only counted: the voicing may leave out a color tone (a 9th) that the
     lead then plays; check_step holds onsets to harmony's chord tones. */
static void check_sounding(const rec_t *rec, uint32_t seed)
{
    static uint32_t strike[MAXT + 1];
    int t, k, j, nxt = -1;
    uint32_t none = 0xFFFFFFFFu;
    for (t = rec->ticks - 1; t >= 0; t--) {
        uint32_t on = none;
        for (k = rec->first[t]; k < rec->first[t + 1]; k++)
            if (rec->ev[k].voice == V_CHORD && rec->ev[k].start < on) on = rec->ev[k].start;
        strike[t] = on;
        for (k = rec->first[t]; k < rec->first[t + 1]; k++)
            if (rec->ev[k].voice == V_CHORD && nxt >= 0)
                CHECK(rec->ev[k].end <= strike[nxt], "seed %u step %d: chord note %d sounds %u samples into the next "
                      "strike", seed, t, rec->ev[k].note, rec->ev[k].end - strike[nxt]);
        if (on != none) nxt = t;
    }
    for (t = 0; t < rec->ticks; t++) {
        int st = rec->step[t], lo = t - 64 < 0 ? 0 : t - 64;
        uint32_t now = rec->at[t];
        int strong = st == 0 || st == 8;
        for (k = rec->first[lo]; k < rec->first[t + 1]; k++) {
            const rev_t *e = &rec->ev[k];
            uint32_t m;
            unsigned mask = 0, cmask = 0, near;
            if (e->voice != V_LEAD && e->voice != V_CANDY) continue;
            if (e->tick != t && !(e->start < now && e->end > now)) continue;
            m = e->start > now ? e->start : now;
            for (j = rec->first[lo]; j < rec->first[t + 1]; j++) {
                const rev_t *x = &rec->ev[j];
                if (x->voice != V_CHORD && x->voice != V_BASS) continue;
                if (x->end <= m || (x->start > m && x->tick != t)) continue;
                mask |= 1u << (x->note % 12);
                if (x->voice == V_CHORD) cmask |= 1u << (x->note % 12);
            }
            if (!mask) {
                if (strong) strong_skipped++;
                continue;
            }
            if (strong) strong_checked++;
            else weak_checked++;
            near = 1u << ((e->note + 11) % 12) | 1u << ((e->note + 1) % 12);
            if (mask & near) {
                int kind = (cmask & near ? 0 : 2) + (e->tick == t ? 0 : 1);
                rubs[strong ? 0 : 1][kind]++;
                CHECK(0, "rub (%s, %s): seed %u bar %d step %d %s note %d over chord %03x bass+chord %03x",
                      kind < 2 ? "chord part" : "bass only", kind & 1 ? "held" : "onset", seed, rec->bar[t], st,
                      e->voice == V_LEAD ? "lead" : "candy", e->note, cmask, mask);
                continue;
            }
            if (strong && ((mask >> (e->note % 12)) & 1)) strong_in++;
        }
    }
}

/* Run a started song to its end, recording into rec; hook (may be NULL) runs before every tick; other (may be
   NULL) is a second song ticked between this song's ticks, to show the two share nothing. */
static void run(song_t *s, rec_t *rec, hook_t hook, void *ctx, song_t *other)
{
    song_ev_t ev[64], oev[64];
    uint32_t pos = 0, lead_end = 0, candy_end = 0, last_start = 0;
    float frac = 0.0f;
    int t, n, i;
    rec->ticks = rec->ne = rec->ended = 0;
    rec->hash = 2166136261u;
    for (t = 0; t < MAXT + 8 && s->state != SS_ENDED; t++) {
        int bar, step;
        float adv;
        uint32_t ip;
        if (hook) hook(s, t, ctx);
        if (other) song_tick(other, oev, 64);
        bar = s->bar;
        step = s->step;
        n = song_tick(s, ev, 64);
        if (t < MAXT) {
            rec->ss[t] = song_step_samples(s);
            rec->at[t] = pos;
            rec->bar[t] = bar;
            rec->step[t] = step;
            rec->key[t] = s->roll.v[RF_KEY];
            rec->first[t] = rec->ne;
        }
        if (clash_checks) check_step(s, bar, step, ev, n, pos, &lead_end, &candy_end, &last_start, s->roll.seed);
        for (i = 0; i < n; i++) {
            rec->hash = fnv(rec->hash, (uint32_t)t);
            rec->hash = fnv(rec->hash, (uint32_t)(ev[i].voice | ev[i].note << 8 | ev[i].vel << 16));
            rec->hash = fnv(rec->hash, ev[i].delay);
            rec->hash = fnv(rec->hash, ev[i].len);
            if (rec->ne < MAXE) {
                rev_t *r = &rec->ev[rec->ne++];
                r->start = pos + ev[i].delay;
                r->end = r->start + ev[i].len;
                r->tick = t;
                r->voice = ev[i].voice;
                r->note = ev[i].note;
            }
        }
        adv = song_step_samples(s) + frac;
        ip = (uint32_t)adv;
        frac = adv - (float)ip;
        pos += ip;
        rec->ticks++;
    }
    if (rec->ticks <= MAXT) {
        rec->at[rec->ticks] = pos;
        rec->first[rec->ticks] = rec->ne;
    }
    rec->samples = pos;
    rec->ended = s->state == SS_ENDED;
    if (clash_checks && rec->ticks <= MAXT) check_sounding(rec, s->roll.seed);
    {
        song_ev_t x[8];
        CHECK(song_tick(s, x, 8) == 0, "song_tick after the end must return 0");
    }
}

/* ---- 1000 chained songs ---- */

static void test_chain(void)
{
    static song_t s, o;
    song_roll_t prev, r, ro;
    uint32_t hashes[51];
    long count[V_N], gestures = 0;
    int i, v, worst_t = 0, with_candy = 0;
    memset(count, 0, sizeof count);
    for (i = 1; i <= 1000; i++) {
        float ss;
        roll_song(i == 1 ? NULL : &prev, (uint32_t)i, &r);
        song_start(&s, &r);
        ss = base_ss(r.v[RF_TEMPO]);
        CHECK(song_step_samples(&s) == ss, "seed %d: first step samples", i);
        run(&s, &R1, NULL, NULL, NULL);
        CHECK(R1.ended, "seed %d: the song never ended", i);
        CHECK(R1.ticks == r.v[RF_BARS] * 16, "seed %d: %d steps for %d bars", i, R1.ticks, r.v[RF_BARS]);
        {
            float want = (float)(r.v[RF_BARS] * 16) * ss, got = (float)R1.samples;
            CHECK(got >= want - ss && got <= want + ss, "seed %d: %u samples, want %.0f", i, R1.samples,
                  (double)want);
            CHECK(got >= 120.0f * 44100.0f - ss && got <= 210.0f * 44100.0f + ss, "seed %d: %u samples outside 120-210 s",
                  i, R1.samples);
        }
        {
            int k;
            for (k = 0; k < R1.ne; k++)
                CHECK(!pitched(R1.ev[k].voice) || R1.ev[k].end <= R1.samples, "seed %d: a note rings past the end", i);
        }
        if (R1.ticks > worst_t) worst_t = R1.ticks;
        {
            int k;
            for (k = 0; k < R1.ne; k++) count[R1.ev[k].voice]++;
            CHECK(R1.ne < MAXE, "seed %d: event record full", i);
        }
        {   /* ear candy: a gesture is a run of candy notes less than a beat apart (end to next start) */
            int k, nc = 0;
            uint32_t last_end = 0, beat = (uint32_t)(4.0f * ss);
            for (k = 0; k < R1.ne; k++) {
                if (R1.ev[k].voice != V_CANDY) continue;
                if (nc == 0 || R1.ev[k].start >= last_end + beat) gestures++;
                if (R1.ev[k].end > last_end) last_end = R1.ev[k].end;
                nc++;
            }
            with_candy += nc > 0;
        }
        if (i <= 50) hashes[i] = R1.hash;
        prev = r;
    }
    /* R19: ear candy is occasional, not absent: in at least 70% of songs, at least 3 gestures a song on average */
    CHECK(with_candy >= 700, "candy in %d of 1000 songs, want at least 700", with_candy);
    CHECK(gestures >= 3000, "%ld candy gestures in 1000 songs, want at least 3 a song", gestures);
    printf("candy: in %d of 1000 songs, %ld gestures (%.2f a song)\n", with_candy, gestures, (double)gestures / 1000.0);
    /* determinism: songs 1-50 again, each with a second song ticking in between */
    clash_checks = 0;
    for (i = 1; i <= 50; i++) {
        roll_song(i == 1 ? NULL : &prev, (uint32_t)i, &r);
        roll_song(&r, (uint32_t)(5000 + i), &ro);
        song_start(&s, &r);
        song_start(&o, &ro);
        run(&s, &R2, NULL, NULL, &o);
        CHECK(R2.hash == hashes[i], "seed %d: second run hash %08x != %08x", i, R2.hash, hashes[i]);
        prev = r;
    }
    clash_checks = 1;
    for (v = 0; v < V_N; v++) CHECK(count[v] > 0, "voice %d never played in 1000 songs", v);
    printf("chain: 1000 songs, longest %d steps; events chord %ld bass %ld lead %ld candy %ld kick %ld snare %ld hat %ld\n",
           worst_t, count[V_CHORD], count[V_BASS], count[V_LEAD], count[V_CANDY], count[V_KICK], count[V_SNARE],
           count[V_HAT]);
}

/* ---- requests ---- */

typedef struct { int bar, step, field, value, fired, ok; } req_t;

static void at_hook(song_t *s, int tick, void *ctx)
{
    req_t *q = (req_t *)ctx;
    (void)tick;
    if (!q->fired && s->bar == q->bar && s->step == q->step) {
        song_request(s, q->field, q->value);
        q->fired = 1;
    }
}

static void two_hook(song_t *s, int tick, void *ctx)
{
    at_hook(s, tick, (req_t *)ctx);
    at_hook(s, tick, (req_t *)ctx + 1);
}

/* a roll from the chain-style seeds whose tempo is at least `far` BPM from bpm and with at least `bars` bars */
static void find_roll(int bpm, int far, int bars, song_roll_t *out)
{
    uint32_t seed;
    for (seed = 1; seed < 100000; seed++) {
        roll_song(NULL, seed, out);
        if ((out->v[RF_TEMPO] - bpm >= far || bpm - out->v[RF_TEMPO] >= far) && out->v[RF_BARS] >= bars) return;
    }
}

static void tempo_lands_at(const song_roll_t *r, int qbar, int qstep, int land, const char *what)
{
    static song_t s;
    req_t q = {0, 0, RF_TEMPO, 78, 0, 0};
    float a = base_ss(r->v[RF_TEMPO]), b = base_ss(78);
    int t, f = -1, left = -1;
    q.bar = qbar;
    q.step = qstep;
    song_start(&s, r);
    while (s.state != SS_ENDED && !(s.bar == qbar && s.step == qstep)) {
        song_ev_t ev[64];
        song_tick(&s, ev, 64);
    }
    song_request(&s, RF_TEMPO, 78);
    CHECK(song_pending(&s, &f, &left) == 1 && f == RF_TEMPO && left == land - qbar,
          "%s: pending field %d bars_left %d, want %d", what, f, left, land - qbar);
    /* the run from the top with the request issued at the same place */
    song_start(&s, r);
    run(&s, &R1, at_hook, &q, NULL);
    CHECK(R1.ended && R1.ticks == r->v[RF_BARS] * 16, "%s: %d steps, want %d", what, R1.ticks, r->v[RF_BARS] * 16);
    CHECK(s.roll.v[RF_BARS] == r->v[RF_BARS], "%s: bars changed to %d", what, s.roll.v[RF_BARS]);
    CHECK(s.roll.v[RF_TEMPO] == 78, "%s: tempo %d after the change", what, s.roll.v[RF_TEMPO]);
    for (t = 0; t < R1.ticks; t++) {
        if (t < land * 16) CHECK(R1.ss[t] == a, "%s: step %d changed before bar %d", what, t, land);
        else if (t == land * 16) CHECK(R1.ss[t] != a, "%s: no change at bar %d", what, land);
        else if (t >= land * 16 + 32) CHECK(R1.ss[t] == b, "%s: step %d not at 78 after the glide", what, t);
        else CHECK((a < b) ? (R1.ss[t] > R1.ss[t - 1]) : (R1.ss[t] < R1.ss[t - 1]), "%s: glide not monotone at %d",
                   what, t);
    }
}

/* Review Focus 1 */
static void test_tempo_edges(void)
{
    static song_t s;
    song_roll_t r, next;
    req_t q = {0, 0, RF_TEMPO, 78, 0, 0};
    int t, f, left, last;
    find_roll(78, 4, 24, &r);
    tempo_lands_at(&r, 15, 15, 16, "issued before the last step of bar 15");
    tempo_lands_at(&r, 16, 0, 16, "issued after the last step of bar 15");
    tempo_lands_at(&r, 16, 1, 20, "issued after bar 16 began");

    /* inside the outro: the song still ends on schedule, at its own tempo when the boundary is past the end */
    song_start(&s, &r);
    last = s.plan.b[s.plan.n - 1].start;
    q.bar = r.v[RF_BARS] - 3;
    q.step = 5;
    CHECK(q.bar >= last, "outro request: bar %d before the last block %d", q.bar, last);
    run(&s, &R1, at_hook, &q, NULL);
    CHECK(q.fired && R1.ended && R1.ticks == r.v[RF_BARS] * 16, "outro request: %d steps, want %d", R1.ticks,
          r.v[RF_BARS] * 16);
    for (t = 0; t < R1.ticks; t++) CHECK(R1.ss[t] == base_ss(r.v[RF_TEMPO]), "outro request: tempo moved at %d", t);
    CHECK(song_pending(&s, &f, &left) == 0, "outro request: still pending after the end");

    /* after the end: ignored; the next song plays its own roll */
    song_request(&s, RF_TEMPO, 78);
    CHECK(song_pending(&s, &f, &left) == 0, "a request after the end must be ignored");
    roll_song(&r, 4242, &next);
    song_start(&s, &next);
    CHECK(s.state == SS_PLAYING && s.bar == 0, "song_start must restart at bar 0");
    CHECK(s.roll.v[RF_TEMPO] == next.v[RF_TEMPO] && song_step_samples(&s) == base_ss(next.v[RF_TEMPO]),
          "the next song must use its own tempo");
    CHECK(song_pending(&s, &f, &left) == 0, "the next song starts with nothing pending");
}

/* sample x as a position in steps on a run's grid */
static float grid_pos(const rec_t *rec, uint32_t x)
{
    int k = 0;
    while (k + 1 < rec->ticks && rec->at[k + 1] <= x) k++;
    return (float)k + (float)(x - rec->at[k]) / (float)(rec->at[k + 1] - rec->at[k]);
}

/* Review Focus 2: 70 -> 90 at bar 8 */
static void test_tempo_glide(void)
{
    static song_t s;
    song_roll_t r;
    song_plan_t before;
    req_t q = {8, 0, RF_TEMPO, 90, 0, 0};
    int i, t, bars;
    roll_song(NULL, 7, &r);
    roll_set(&r, RF_TEMPO, 70);
    bars = r.v[RF_BARS];
    song_start(&s, &r);
    before = s.plan;
    run(&s, &R1, at_hook, &q, NULL);
    CHECK(R1.ended && R1.ticks == bars * 16, "70->90: %d steps for %d bars", R1.ticks, bars);
    CHECK(s.roll.v[RF_BARS] == bars && s.roll.v[RF_TEMPO] == 90, "70->90: bars %d tempo %d", s.roll.v[RF_BARS],
          s.roll.v[RF_TEMPO]);
    CHECK(memcmp(&before, &s.plan, sizeof before) == 0, "70->90: the plan must not change");
    for (i = 0; i < s.plan.n; i++) {
        int st = s.plan.b[i].start;
        CHECK(R1.bar[st * 16] == st && R1.step[st * 16] == 0, "70->90: block %d start off its bar", i);
    }
    for (t = 0; t < R1.ticks; t++) {
        if (t < 128) CHECK(R1.ss[t] == base_ss(70), "70->90: early change at %d", t);
        else if (t < 160) CHECK(R1.ss[t] < R1.ss[t - 1], "70->90: glide not speeding up at %d", t);
        else CHECK(R1.ss[t] == base_ss(90), "70->90: step %d not at 90", t);
    }
    {   /* every event sits within the step that emitted it; a rolled chord's notes within its roll (3 gaps of at
           most 60 ms, which at 90 BPM is longer than a step) */
        int k, bad = 0;
        for (k = 0; k < R1.ne; k++) {
            uint32_t off = R1.ev[k].start - R1.at[R1.ev[k].tick];
            uint32_t lim = R1.ev[k].voice == V_CHORD ? 3u * 2646u + 1u : R1.at[R1.ev[k].tick + 1] - R1.at[R1.ev[k].tick] - 1u;
            if (off > lim) bad++;
        }
        CHECK(bad == 0, "70->90: %d events land after their step", bad);
    }
    {   /* on the step grid, no chord or bass note ends later than the same note at a steady 70 BPM */
        int k, bad = 0;
        song_start(&s, &r);
        run(&s, &R2, NULL, NULL, NULL);
        CHECK(R1.ne == R2.ne, "70->90: %d events, %d at a steady tempo", R1.ne, R2.ne);
        for (k = 0; k < R1.ne && k < R2.ne; k++) {
            const rev_t *a = &R1.ev[k], *b = &R2.ev[k];
            CHECK(a->tick == b->tick && a->voice == b->voice && a->note == b->note, "70->90: event %d differs", k);
            if ((a->voice == V_CHORD || a->voice == V_BASS) && grid_pos(&R1, a->end) > grid_pos(&R2, b->end) + 0.001f)
                bad++;
        }
        CHECK(bad == 0, "70->90: %d chord/bass notes outlast their place on the grid", bad);
    }
}

/* 1 if a bass note of bar `bar` (as groove_step writes it) would ring past the bar's end */
static int spills(const song_roll_t *r, const plan_block_t *b, int bar)
{
    const lib_prog_t *pr = lib_prog_by_id((uint8_t)r->v[RF_PROG]);
    float ss = base_ss(r->v[RF_TEMPO]);
    int pcs[5], np, st, k, n;
    song_ev_t ev[32];
    np = harmony_chord_at(r, bar, bar / (pr->n * pr->bars_per_chord) + 1, (b->flags & PB_DARK) != 0, pcs);
    for (st = 8; st < 16; st++) {
        n = groove_step(r, b, bar, st, ss, pcs, np, r->seed, ev, 32);
        for (k = 0; k < n; k++)
            if (ev[k].voice == V_BASS && (float)st * ss + (float)(ev[k].delay + ev[k].len) > 16.0f * ss + 1.0f) return 1;
    }
    return 0;
}

/* Review Focus 5: a key change while chords sound (and a swung bass note would spill over the boundary) */
static void test_key_change(void)
{
    static song_t s;
    song_roll_t r;
    uint32_t seed;
    int k, old, nk, B = 16, bad = 0, chords_after = 0, crossing = 0;
    for (seed = 1; seed < 1000; seed++) {
        const plan_block_t *b1, *b2;
        roll_song(NULL, seed, &r);
        song_start(&s, &r);
        b1 = plan_at(&s.plan, 15);
        b2 = plan_at(&s.plan, 16);
        if (b1 && b2 && (b1->layers & L_KEYS) && (b2->layers & L_KEYS) && spills(&r, b1, 15)) break;
    }
    CHECK(seed < 1000, "key change: no song with a note spilling over bar 16");
    old = r.v[RF_KEY];
    nk = (old + 5) % 12;
    {
        req_t q = {14, 3, RF_KEY, 0, 0, 0};
        q.value = nk;
        song_start(&s, &r);
        run(&s, &R1, at_hook, &q, NULL);
    }
    CHECK(R1.ended && R1.ticks == r.v[RF_BARS] * 16, "key change: the song must end on schedule");
    CHECK(s.roll.v[RF_KEY] == nk, "key change: key %d, want %d", s.roll.v[RF_KEY], nk);
    for (k = 0; k < R1.ticks; k++)
        CHECK(R1.key[k] == (k < B * 16 ? old : nk), "key change: step %d plays key %d", k, R1.key[k]);
    for (k = 0; k < R1.ne; k++) {
        const rev_t *e = &R1.ev[k];
        if (!pitched(e->voice)) continue;
        if (e->tick < B * 16) {
            if (e->end > R1.at[B * 16]) bad++;
            if (e->voice == V_CHORD && e->tick >= (B - 2) * 16) crossing++;
        } else if (e->voice == V_CHORD && e->tick < (B + 1) * 16) {
            const lib_prog_t *pr = lib_prog_by_id((uint8_t)s.roll.v[RF_PROG]);
            const plan_block_t *b = plan_at(&s.plan, B);
            int pcs[5], n, j, in = 0;
            n = harmony_chord_at(&s.roll, B, B / (pr->n * pr->bars_per_chord) + 1, (b->flags & PB_DARK) != 0, pcs);
            for (j = 0; j < n; j++) in |= pcs[j] == e->note % 12;
            CHECK(in, "key change: chord note %d at the boundary is not in the new key's chord", e->note);
            chords_after++;
        }
    }
    CHECK(crossing > 0, "key change: the test needs chords sounding before the boundary");
    CHECK(chords_after > 0, "key change: chords must play after the boundary");
    CHECK(bad == 0, "key change: %d old-key notes ring past the boundary", bad);
}

/* Key changes on 20 songs by +-1, +-5, +-7 semitones: the key turns exactly at bar 16 and no note of the old key
   rings past it; every clash check holds after the change (registers included: the motif is transposed) */
static void test_key_sweep(void)
{
    static const int dk[6] = {1, -1, 5, -5, 7, -7};
    static song_t s;
    song_roll_t r;
    int seed, d, k, B = 16;
    for (seed = 1; seed <= 20; seed++)
        for (d = 0; d < 6; d++) {
            req_t q = {14, 3, RF_KEY, 0, 0, 0};
            int bad = 0, old;
            roll_song(NULL, (uint32_t)seed, &r);
            old = r.v[RF_KEY];
            q.value = (old + dk[d] + 12) % 12;
            song_start(&s, &r);
            run(&s, &R1, at_hook, &q, NULL);
            CHECK(R1.ended && R1.ticks == r.v[RF_BARS] * 16, "key sweep %d %+d: the song must end", seed, dk[d]);
            CHECK(R1.key[B * 16 - 1] == old && R1.key[B * 16] == q.value, "key sweep %d %+d: key not turned at bar 16",
                  seed, dk[d]);
            for (k = 0; k < R1.ne; k++)
                if (pitched(R1.ev[k].voice) && R1.ev[k].tick < B * 16 && R1.ev[k].end > R1.at[B * 16]) bad++;
            CHECK(bad == 0, "key sweep %d %+d: %d old-key notes ring past the boundary", seed, dk[d], bad);
        }
}

typedef struct { int n; req_t q[4]; } reqs_t;

static void n_hook(song_t *s, int tick, void *ctx)
{
    reqs_t *rs = (reqs_t *)ctx;
    int i;
    for (i = 0; i < rs->n; i++) at_hook(s, tick, &rs->q[i]);
}

/* Determinism with requests: key, tempo, motion and mood at various points, run twice (the second time with
   another song ticking in between) */
static void test_request_determinism(void)
{
    static song_t s, o;
    song_roll_t r, ro;
    int i, pass;
    uint32_t h[21];
    for (pass = 0; pass < 2; pass++)
        for (i = 1; i <= 20; i++) {
            reqs_t rs;
            memset(&rs, 0, sizeof rs);
            roll_song(NULL, (uint32_t)(100 + i), &r);
            rs.n = 4;
            rs.q[0].bar = 4 + i % 4; rs.q[0].step = i % 16; rs.q[0].field = RF_KEY;
            rs.q[0].value = (r.v[RF_KEY] + 1 + i % 11) % 12;
            rs.q[1].bar = 9; rs.q[1].step = 3; rs.q[1].field = RF_TEMPO; rs.q[1].value = 70 + (i * 7) % 21;
            rs.q[2].bar = 13; rs.q[2].step = 0; rs.q[2].field = RF_MOTION; rs.q[2].value = (i * 37) % 101;
            rs.q[3].bar = 17 + i % 8; rs.q[3].step = 5; rs.q[3].field = RF_MOOD; rs.q[3].value = i % 4;
            song_start(&s, &r);
            if (pass == 0) {
                run(&s, &R1, n_hook, &rs, NULL);
                h[i] = R1.hash;
                CHECK(R1.ended && R1.ticks == r.v[RF_BARS] * 16, "requests %d: the song must end on schedule", i);
                CHECK(rs.q[0].fired && rs.q[1].fired && rs.q[2].fired && rs.q[3].fired, "requests %d: not all fired",
                      i);
            } else {
                roll_song(&r, (uint32_t)(9000 + i), &ro);
                song_start(&o, &ro);
                run(&s, &R2, n_hook, &rs, &o);
                CHECK(R2.hash == h[i], "requests %d: second run hash %08x != %08x", i, R2.hash, h[i]);
            }
        }
}

/* Mood and Motion requests: the merged plan keeps what played, and nothing clashes after the change */
static void test_mood_motion(void)
{
    static song_t s;
    song_roll_t r;
    song_plan_t before;
    req_t q = {5, 4, RF_MOOD, 0, 0, 0}, q2 = {9, 2, RF_MOTION, 100, 0, 0};
    int i, total = 0, ok = 1, t;
    roll_song(NULL, 11, &r);
    q.value = (r.v[RF_MOOD] + 2) % 4;
    song_start(&s, &r);
    before = s.plan;
    for (t = 0; s.state != SS_ENDED; t++) {
        song_ev_t ev[64];
        at_hook(&s, t, &q);
        at_hook(&s, t, &q2);
        song_tick(&s, ev, 64);
    }
    CHECK(s.roll.v[RF_MOOD] == q.value, "mood: %d, want %d", s.roll.v[RF_MOOD], q.value);
    CHECK(s.roll.v[RF_MOTION] == 100, "motion: %d", s.roll.v[RF_MOTION]);
    CHECK(s.plan.bars == r.v[RF_BARS], "motion: plan bars %d", s.plan.bars);
    for (i = 0; i < s.plan.n; i++) {
        if (s.plan.b[i].start != total) ok = 0;
        total += s.plan.b[i].bars;
    }
    CHECK(ok && total == r.v[RF_BARS], "motion: merged blocks must tile the song");
    for (i = 0; i < 12; i++) {
        const plan_block_t *a = plan_at(&before, i), *b = plan_at(&s.plan, i);
        CHECK(a && b && a->layers == b->layers && a->section == b->section && a->flags == b->flags,
              "motion: bar %d before the boundary changed", i);
    }
    /* the same requests again, with every clash check on every step */
    {
        req_t qs[2];
        qs[0] = q;
        qs[1] = q2;
        qs[0].fired = qs[1].fired = 0;
        song_start(&s, &r);
        run(&s, &R1, two_hook, qs, NULL);
        CHECK(qs[0].fired && qs[1].fired, "mood and motion requests must fire");
    }
    CHECK(R1.ended && R1.ticks == r.v[RF_BARS] * 16, "mood: the song must end on schedule");
}

/* song_pending counts down each bar until the change lands; a later request for the same field replaces it */
static void test_pending(void)
{
    static song_t s;
    song_roll_t r;
    song_ev_t ev[64];
    int f, left, got;
    find_roll(84, 3, 24, &r);
    song_start(&s, &r);
    CHECK(song_pending(&s, &f, &left) == 0, "nothing pending at the start");
    while (!(s.bar == 5 && s.step == 7)) song_tick(&s, ev, 64);
    song_request(&s, RF_TEMPO, 80);
    song_request(&s, RF_TEMPO, 84);
    while (s.bar < 8 || s.step == 0) {
        if (s.step == 0 || (s.bar == 5 && s.step == 7)) {
            got = song_pending(&s, &f, &left);
            CHECK(got == 1 && f == RF_TEMPO && left == 8 - s.bar, "pending at bar %d: %d field %d left %d", s.bar,
                  got, f, left);
        }
        if (s.bar == 8 && s.step == 0) {
            song_tick(&s, ev, 64);
            break;
        }
        song_tick(&s, ev, 64);
    }
    CHECK(song_pending(&s, &f, &left) == 0, "pending after the change landed");
    CHECK(s.roll.v[RF_TEMPO] == 84, "the later request must replace the earlier: tempo %d", s.roll.v[RF_TEMPO]);

    /* two fields pending for the same boundary: the first one applied (lowest field) is reported */
    song_start(&s, &r);
    while (!(s.bar == 4 && s.step == 3)) song_tick(&s, ev, 64);
    song_request(&s, RF_MOTION, 90);
    song_request(&s, RF_TEMPO, 80);
    CHECK(song_pending(&s, &f, &left) == 1 && f == RF_TEMPO && left == 4, "earliest pending: field %d left %d", f,
          left);
    song_request(&s, 99, 1);
    song_request(&s, -1, 1);
    CHECK(song_pending(&s, &f, &left) == 1 && f == RF_TEMPO, "bad fields must be ignored");
}

int main(void)
{
    int i;
    test_chain();
    test_tempo_edges();
    test_tempo_glide();
    test_key_change();
    test_key_sweep();
    test_request_determinism();
    test_mood_motion();
    test_pending();
    printf("strong steps: %ld lead/candy notes checked against the sounding chord and bass (%ld of them sounding "
           "tones), %ld with neither\n", strong_checked, strong_in, strong_skipped);
    printf("other steps: %ld lead/candy notes checked against the sounding chord and bass\n", weak_checked);
    for (i = 0; i < 2; i++)
        printf("semitone rubs on %s steps (0 allowed): over the chord part %ld onset / %ld held; over the bass only "
               "%ld onset / %ld held\n", i ? "other" : "strong", rubs[i][0], rubs[i][1], rubs[i][2], rubs[i][3]);
    if (fails) {
        printf("%d check(s) failed\n", fails);
        return 1;
    }
    printf("clash: all checks passed\n");
    return 0;
}

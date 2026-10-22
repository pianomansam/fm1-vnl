/* SPDX-License-Identifier: MPL-2.0 */
/* The roll. Every field draws from rng stream = its field index, so re-rolling one choice never shifts another.
   Each field's legal values and weights come from one place (cands), shared by the song roll, the dice and picks. */
#include <string.h>
#include "rng.h"
#include "roll.h"

#define NC 128          /* largest candidate list: swing, 101 values */
#define NOVELTY_MIN 5
#define NOVELTY_PASSES 16

typedef struct { int16_t v[NC]; uint32_t w[NC]; int n; } cands_t;
enum { KEEP_ALL, KEEP_NE, KEEP_FAR2, KEEP_FAR6, KEEP_OTHER_FAMILY };

static const lib_prog_t *g_progs;
static int g_nprogs;

void roll_use_progs(const lib_prog_t *progs, int n)
{
    g_progs = progs;
    g_nprogs = progs ? n : 0;
}

static const lib_prog_t *progs(int *n)
{
    if (g_progs) {
        *n = g_nprogs;
        return g_progs;
    }
    *n = LIB_NPROGS;
    return LIB_PROGS;
}

static const lib_prog_t *prog_find(int id)
{
    int i, n;
    const lib_prog_t *p = progs(&n);
    for (i = 0; i < n; i++) if (p[i].id == id) return &p[i];
    return NULL;
}

static int prog_ok(const lib_prog_t *p, int mood, int mode)
{
    return p && (p->moods & (1 << mood)) && (p->modes & (1 << mode));
}

static int set_ok(int id, int mood)
{
    const lib_set_t *s = lib_set_by_id((uint8_t)id);
    return s && (s->moods & (1 << mood));
}

static int mode_has_prog(int mood, int mode)
{
    int i, n;
    const lib_prog_t *p = progs(&n);
    for (i = 0; i < n; i++) if (prog_ok(&p[i], mood, mode)) return 1;
    return 0;
}

static int major_family(int mode) { return mode == MODE_IONIAN || mode == MODE_LYDIAN || mode == MODE_MIXOLYDIAN; }

static int family_of(int pattern_id)
{
    const lib_pattern_t *p = lib_pattern_by_id((uint8_t)pattern_id);
    return p ? p->family : -1;
}

/* Bars, a multiple of 4, with bars * 240 / bpm within 120-210 s (R3: 36-76 over 70-90 BPM). */
static int bars_lo(int bpm) { return ((bpm + 1) / 2 + 3) / 4 * 4; }
static int bars_hi(int bpm) { return (7 * bpm / 8) / 4 * 4; }
static int clamp(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }

static uint32_t mix(uint32_t a, uint32_t b)   /* murmur3 finalizer over (a, b): fresh draws per pass / salt */
{
    uint32_t h = a ^ (b * 0x9E3779B9u);
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

static void add(cands_t *c, int v, uint32_t w)
{
    if (c->n < NC) {
        c->v[c->n] = (int16_t)v;
        c->w[c->n] = w;
        c->n++;
    }
}

/* Integer triangle weight over [a, b] peaking at m. */
static uint32_t tri(int x, int a, int m, int b)
{
    return x <= m ? (uint32_t)(x - a + 1) * (uint32_t)(b - m) : (uint32_t)(b - x + 1) * (uint32_t)(m - a);
}

/* Every legal value of field f for roll r, with its weight. */
static void cands(const song_roll_t *r, int f, cands_t *c)
{
    static const uint8_t kind_w[3] = {40, 45, 15};
    int i, n, mood = r->v[RF_MOOD], mode = r->v[RF_MODE];
    const lib_prog_t *p;
    c->n = 0;
    switch (f) {
    case RF_KEY: for (i = 0; i < 12; i++) add(c, i, 1); break;
    case RF_TEMPO: for (i = 70; i <= 90; i++) add(c, i, i < 80 ? (uint32_t)(i - 69) : i <= 84 ? 11u : (uint32_t)(91 - i)); break;
    case RF_MOTION: for (i = 20; i <= 80; i++) add(c, i, 1); break;
    case RF_MOOD: for (i = 0; i < 4; i++) add(c, i, 1); break;
    case RF_MODE:
        for (i = 0; i < MODE_N; i++)
            if (mode_has_prog(mood, i)) add(c, i, LIB_MOODS[mood].mode_w[i]);
        break;
    case RF_MAP: for (i = 0; i < LIB_NMAPS; i++) add(c, LIB_MAPS[i].id, 1); break;
    case RF_PROG: {
        int cnt[3] = {0, 0, 0};
        p = progs(&n);
        for (i = 0; i < n; i++) if (prog_ok(&p[i], mood, mode) && p[i].kind <= PK_JAZZY) cnt[p[i].kind]++;
        for (i = 0; i < n; i++)   /* kind weights 40/45/15, spread evenly over the legal entries of each kind */
            if (prog_ok(&p[i], mood, mode) && p[i].kind <= PK_JAZZY) add(c, p[i].id, kind_w[p[i].kind] * 840u / (uint32_t)cnt[p[i].kind]);
        break;
    }
    case RF_COLOR: for (i = 0; i < 3; i++) add(c, i, LIB_MOODS[mood].color_w[i]); break;
    case RF_SET: for (i = 0; i < LIB_NSETS; i++) if (LIB_SETS[i].moods & (1 << mood)) add(c, LIB_SETS[i].id, 1); break;
    case RF_PATTERN: for (i = 0; i < LIB_NPATTERNS; i++) add(c, LIB_PATTERNS[i].id, 1); break;
    case RF_SWING: for (i = 520; i <= 620; i++) add(c, i, tri(i, 520, 525, 620)); break;   /* mean 555, median ~551 */
    case RF_DUST: for (i = 0; i <= 3; i++) add(c, i, 1); break;
    case RF_NOISE: for (i = 0; i <= 2; i++) add(c, i, 1); break;
    case RF_AMBIENCE: for (i = 0; i <= 1; i++) add(c, i, 1); break;
    case RF_BARS: for (i = bars_lo(r->v[RF_TEMPO]); i <= bars_hi(r->v[RF_TEMPO]); i += 4) add(c, i, 1); break;
    default: break;
    }
}

/* Drop candidates that fail the keep rule against ref. */
static void filter(cands_t *c, int f, int keep, int ref)
{
    int i, j = 0;
    for (i = 0; i < c->n; i++) {
        int v = c->v[i], d = v - ref, ok;
        switch (keep) {
        case KEEP_NE: ok = v != ref; break;
        case KEEP_FAR2: ok = d >= 2 || d <= -2; break;
        case KEEP_FAR6: ok = d >= 6 || d <= -6; break;
        case KEEP_OTHER_FAMILY: ok = f == RF_PATTERN ? family_of(v) != family_of(ref) : v != ref; break;
        default: ok = 1; break;
        }
        if (ok) {
            c->v[j] = c->v[i];
            c->w[j] = c->w[i];
            j++;
        }
    }
    c->n = j;
}

/* Weighted pick from stream (seed, f); uniform if every remaining weight is zero; -1 if empty. */
static int pick(const cands_t *c, uint32_t seed, int f)
{
    rng_t g;
    uint32_t tot = 0, x;
    int i;
    if (c->n == 0) return -1;
    rng_seed(&g, seed, (uint32_t)f);
    for (i = 0; i < c->n; i++) tot += c->w[i];
    if (tot == 0) return c->v[rng_int(&g, c->n)];
    x = rng_next(&g) % tot;
    for (i = 0; i < c->n; i++) {
        if (x < c->w[i]) return c->v[i];
        x -= c->w[i];
    }
    return c->v[c->n - 1];
}

/* Re-roll field f under a keep rule; falls back to any legal value if the rule leaves none. 1 if it set a value. */
static int draw(song_roll_t *r, int f, uint32_t seed, int keep, int ref)
{
    cands_t c;
    int v;
    cands(r, f, &c);
    filter(&c, f, keep, ref);
    v = pick(&c, seed, f);
    if (v < 0 && keep != KEEP_ALL) {
        cands(r, f, &c);
        v = pick(&c, seed, f);
    }
    if (v < 0) return 0;
    r->v[f] = (int16_t)v;
    return 1;
}

/* Key flow: a circle-of-fifths neighbor of the last key, or its relative when the mode family flips. */
static void draw_key(song_roll_t *r, const song_roll_t *prev, uint32_t seed)
{
    cands_t c;
    int pk = prev->v[RF_KEY], pm = major_family(prev->v[RF_MODE]), cm = major_family(r->v[RF_MODE]);
    c.n = 0;
    add(&c, (pk + 7) % 12, 1);
    add(&c, (pk + 5) % 12, 1);
    if (pm && !cm) add(&c, (pk + 9) % 12, 1);         /* relative minor: down a minor third */
    if (!pm && cm) add(&c, (pk + 3) % 12, 1);         /* relative major: up a minor third */
    r->v[RF_KEY] = (int16_t)pick(&c, seed, RF_KEY);
}

static void clamp_bars(song_roll_t *r)
{
    int b = r->v[RF_BARS];
    b = (b + 2) / 4 * 4;
    r->v[RF_BARS] = (int16_t)clamp(b, bars_lo(r->v[RF_TEMPO]), bars_hi(r->v[RF_TEMPO]));
}

/* After a mood or mode change: keep mode, progression and set legal, re-picking only what became illegal. */
static void fix_deps(song_roll_t *r, uint32_t seed)
{
    if (!mode_has_prog(r->v[RF_MOOD], r->v[RF_MODE])) draw(r, RF_MODE, seed, KEEP_ALL, 0);
    if (!prog_ok(prog_find(r->v[RF_PROG]), r->v[RF_MOOD], r->v[RF_MODE])) draw(r, RF_PROG, seed, KEEP_ALL, 0);
    if (!set_ok(r->v[RF_SET], r->v[RF_MOOD])) draw(r, RF_SET, seed, KEEP_ALL, 0);
}

static int novelty(const song_roll_t *a, const song_roll_t *b)
{
    int d = 0, t = a->v[RF_TEMPO] - b->v[RF_TEMPO];
    d += a->v[RF_MAP] != b->v[RF_MAP];
    d += a->v[RF_PROG] != b->v[RF_PROG];
    d += a->v[RF_SET] != b->v[RF_SET];
    d += family_of(a->v[RF_PATTERN]) != family_of(b->v[RF_PATTERN]);
    d += a->v[RF_MODE] != b->v[RF_MODE];
    d += a->v[RF_DUST] != b->v[RF_DUST];
    d += a->v[RF_NOISE] != b->v[RF_NOISE];
    d += a->v[RF_AMBIENCE] != b->v[RF_AMBIENCE];
    d += t >= 6 || t <= -6;
    return d;
}

static int same_choice(const song_roll_t *a, const song_roll_t *b, int f)
{
    return f == RF_PATTERN ? family_of(a->v[f]) == family_of(b->v[f]) : a->v[f] == b->v[f];
}

void roll_song(const song_roll_t *prev_in, uint32_t seed, song_roll_t *out)
{
    static const int nov[7] = {RF_MAP, RF_SET, RF_PATTERN, RF_DUST, RF_NOISE, RF_AMBIENCE, RF_PROG};
    song_roll_t prev, r;
    cands_t c;
    int pass, i, secs;

    if (prev_in) prev = *prev_in;
    memset(&r, 0, sizeof r);
    r.seed = seed;
    draw(&r, RF_MOOD, seed, KEEP_ALL, 0);
    draw(&r, RF_MODE, seed, KEEP_ALL, 0);
    if (prev_in) draw_key(&r, &prev, seed);
    else draw(&r, RF_KEY, seed, KEEP_ALL, 0);
    draw(&r, RF_TEMPO, seed, prev_in ? KEEP_FAR6 : KEEP_ALL, prev_in ? prev.v[RF_TEMPO] : 0);
    draw(&r, RF_MAP, seed, KEEP_ALL, 0);
    draw(&r, RF_PROG, seed, KEEP_ALL, 0);
    draw(&r, RF_SET, seed, KEEP_ALL, 0);
    draw(&r, RF_PATTERN, seed, KEEP_ALL, 0);
    draw(&r, RF_COLOR, seed, KEEP_ALL, 0);
    draw(&r, RF_SWING, seed, KEEP_ALL, 0);
    draw(&r, RF_DUST, seed, KEEP_ALL, 0);
    draw(&r, RF_NOISE, seed, KEEP_ALL, 0);
    draw(&r, RF_AMBIENCE, seed, KEEP_ALL, 0);
    draw(&r, RF_MOTION, seed, KEEP_ALL, 0);

    /* length: a target in 120-210 s (mode 150 s), to bars, clamped to the seconds rule */
    c.n = 0;
    for (i = 120; i <= 210; i++) add(&c, i, tri(i, 120, 150, 210));
    secs = pick(&c, seed, RF_BARS);
    r.v[RF_BARS] = (int16_t)(4 * ((secs * r.v[RF_TEMPO] + 480) / 960));
    clamp_bars(&r);

    /* novelty: re-roll the choices that match the last song from fresh draws; the last pass must differ */
    for (pass = 1; prev_in && pass <= NOVELTY_PASSES && novelty(&r, &prev) < NOVELTY_MIN; pass++) {
        uint32_t s = mix(seed, (uint32_t)pass);
        int keep = pass == NOVELTY_PASSES ? KEEP_OTHER_FAMILY : KEEP_ALL;
        if (r.v[RF_MODE] == prev.v[RF_MODE]) {
            draw(&r, RF_MODE, s, keep, prev.v[RF_MODE]);
            draw_key(&r, &prev, s);   /* the relative depends on the modes */
        }
        for (i = 0; i < 7; i++)
            if (same_choice(&r, &prev, nov[i])) draw(&r, nov[i], s, keep, prev.v[nov[i]]);
        if (!prog_ok(prog_find(r.v[RF_PROG]), r.v[RF_MOOD], r.v[RF_MODE]))
            draw(&r, RF_PROG, s, keep, prev.v[RF_PROG]);
    }
    *out = r;
}

int roll_dice(song_roll_t *r, int field, uint32_t salt)
{
    uint32_t s = mix(mix(r->seed, 0xD1CEu), salt);
    cands_t c;
    int v;
    if (field < 0 || field >= RF_N) return 0;
    cands(r, field, &c);
    filter(&c, field, field == RF_TEMPO ? KEEP_FAR2 : KEEP_NE, r->v[field]);
    v = pick(&c, s, field);
    if (v < 0) return 0;
    r->v[field] = (int16_t)v;
    if (field == RF_TEMPO) clamp_bars(r);
    if (field == RF_MOOD || field == RF_MODE) fix_deps(r, s);
    return 1;
}

int roll_set(song_roll_t *r, int field, int value)
{
    static const int16_t lo[RF_N] = {0, 70, 0, 0, 0, 0, 0, 0, 0, 0, 520, 0, 0, 0, 0};
    static const int16_t hi[RF_N] = {11, 90, 100, 3, MODE_N - 1, 255, 255, 2, 255, 255, 620, 3, 2, 1, 255};
    uint32_t s;
    if (field < 0 || field >= RF_N) return -1;
    if (field == RF_BARS) {
        r->v[RF_BARS] = (int16_t)clamp(value, 0, 1000);
        clamp_bars(r);
        return r->v[RF_BARS];
    }
    value = clamp(value, lo[field], hi[field]);
    switch (field) {
    case RF_MODE: if (!mode_has_prog(r->v[RF_MOOD], value)) return r->v[field]; break;
    case RF_MAP: if (!lib_map_by_id((uint8_t)value)) return r->v[field]; break;
    case RF_PROG: if (!prog_ok(prog_find(value), r->v[RF_MOOD], r->v[RF_MODE])) return r->v[field]; break;
    case RF_SET: if (!set_ok(value, r->v[RF_MOOD])) return r->v[field]; break;
    case RF_PATTERN: if (!lib_pattern_by_id((uint8_t)value)) return r->v[field]; break;
    default: break;
    }
    r->v[field] = (int16_t)value;
    s = mix(mix(r->seed, 0x5E7u), (uint32_t)(field << 8 | value));
    if (field == RF_TEMPO) clamp_bars(r);
    if (field == RF_MOOD || field == RF_MODE) fix_deps(r, s);
    return value;
}

int roll_seconds(const song_roll_t *r)
{
    return r->v[RF_TEMPO] > 0 ? r->v[RF_BARS] * 240 / r->v[RF_TEMPO] : 0;
}

/* SPDX-License-Identifier: MPL-2.0 */
/* The sequencer. One tick = one 16th: the block covering the bar (plan_at), the bar's chord, then groove, harmony
   and melody for that step, sorted by delay. The sample position of every step is tracked as an integer plus a
   float fraction, advanced by song_step_samples() after each tick, so a caller that accumulates the same way sees
   the same boundary samples.

   Every 4-bar boundary (and the song's end) is a note-off line: a pitched note that would ring past it is clipped
   to end on it. With the library's chords (at most 2 bars each, starting on their bar) and the 2-bar pedal bass,
   that only trims swing and rounding spill, and it means a key or harmony change never overlaps the old key's
   notes, even when the request came after those notes were emitted.

   Requests: one slot per field, landing on the next 4-bar boundary, applied at that boundary's first tick (Mood
   first, then by field). What each field rebuilds:
     RF_KEY      the roll's key; the motif is transposed (same scale steps; another starting degree, or an octave,
                 if that keeps it in the lead's register); melody_plan again (which phrases may restate it)
     RF_TEMPO    a linear glide of step samples over 2 bars; RF_BARS is kept
     RF_MOTION,  plan_build again; blocks before the boundary stay, the new plan's blocks from it on; melody_plan
     RF_MAP       again on the merged plan
     RF_MOOD     roll_set (re-picks an illegal mode/progression/set), one roll_dice of the progression toward the
                 mood; motif and phrases rebuilt (as for RF_MODE, RF_PROG, RF_COLOR: the motif depends on the chords)
   A boundary rebuilds at most one plan, one motif and one phrase plan (all bounded loops).
     RF_BARS     ignored: the bar count never changes
     the rest    roll_set only (set, pattern, swing, dust, noise, ambience take effect from the boundary) */
#include <stdint.h>
#include <string.h>
#include "song.h"
#include "harmony.h"
#include "library.h"
#include "theory.h"

#define BUF 64

static float ss_of(int bpm) { return 44100.0f * 60.0f / ((float)bpm * 4.0f); }

/* step samples of song step idx (bar * 16 + step), following a glide */
static float ss_at(const song_t *s, int idx)
{
    int g = idx - s->glide_t0;
    if (g >= 0 && g < 32) return s->glide_from + (s->glide_to - s->glide_from) * (float)(g + 1) / 32.0f;
    return s->glide_to;
}

static void advance(uint32_t *pos, float *frac, float ss)
{
    float adv = ss + *frac;
    uint32_t ip = (uint32_t)adv;
    *frac = adv - (float)ip;
    *pos += ip;
}

/* the sample position of the next 4-bar boundary (or the end), from the step idx that starts a 4-bar group */
static void find_edge(song_t *s, int idx)
{
    uint32_t pos = s->pos;
    float frac = s->frac;
    int k, end = idx + 64, last = s->roll.v[RF_BARS] * 16;
    if (end > last) end = last;
    for (k = idx; k < end; k++) advance(&pos, &frac, ss_at(s, k));
    s->edge = pos;
}

static int loop_bars(const song_roll_t *r)
{
    const lib_prog_t *p = lib_prog_by_id((uint8_t)r->v[RF_PROG]);
    int n = p ? p->n * p->bars_per_chord : 4;
    return n > 0 ? n : 4;
}

/* ---- the motif under a new key: the same scale steps from the same degree, kept in the lead's register ---- */

static void mode_offsets(int mode, int *off)
{
    uint16_t pool = th_mode_pool(mode);
    int i, n = 0;
    for (i = 0; i < 12 && n < 7; i++)
        if ((pool >> i) & 1) off[n++] = i;
    while (n < 7) { off[n] = n ? off[n - 1] : 0; n++; }
}

static int deg_of(int key, const int *off, int note)    /* as melody.c reads a motif */
{
    int d = note - key + 12, pc, i;
    if (d < 0) d = 0;
    pc = d % 12;
    for (i = 6; i > 0 && off[i] > pc; i--) {}
    return d / 12 * 7 + i;
}

static int note_of(int key, const int *off, int deg)
{
    if (deg < 0) deg = 0;
    return key - 12 + deg / 7 * 12 + off[deg % 7];
}

/* semitones the motif's notes fall outside [lo, hi] when it starts on `first` */
static int motif_spill(const motif_t *m, int key, const int *off, int first, int lo, int hi)
{
    int i, d0 = deg_of(key, off, first), spill = 0;
    for (i = 0; i < m->n && i < 16; i++) {
        int n = note_of(key, off, d0 + m->step[i]);
        spill += n < lo ? lo - n : n > hi ? n - hi : 0;
    }
    return spill;
}

/* The motif moved by d semitones keeps its scale steps. If no octave of it fits the lead's register, it starts on
   the nearest scale degree that fits (same steps, from another degree); failing that (never with 3-8 notes of a
   20-semitone register), it is rebuilt. Returns 0 when a rebuild is needed. */
static int transpose_motif(song_t *s, int d)
{
    static const int8_t shift[15] = {0, -7, 7, -1, 1, -2, 2, -3, 3, -4, 4, -5, 5, -6, 6};
    int off[7], i, key = s->roll.v[RF_KEY], d0;
    int mood = s->roll.v[RF_MOOD] < 0 ? 0 : s->roll.v[RF_MOOD] > 3 ? 3 : s->roll.v[RF_MOOD];
    int lo = 64 + LIB_MOODS[mood].lead_shift;
    if (s->motif.n == 0) return 1;
    mode_offsets(s->roll.v[RF_MODE], off);
    d0 = deg_of(key, off, s->motif.first + d);
    for (i = 0; i < 15; i++) {
        int f = note_of(key, off, d0 + shift[i]);
        if (f >= 0 && f <= 127 && motif_spill(&s->motif, key, off, f, lo, lo + 20) == 0) {
            s->motif.first = (uint8_t)f;
            return 1;
        }
    }
    return 0;
}

/* ---- the plan from a boundary on ---- */

static void merge_plan(song_t *s, int B)
{
    song_plan_t np, m;
    int i;
    plan_build(&s->roll, &np);
    memset(&m, 0, sizeof m);
    m.bars = s->plan.bars;
    for (i = 0; i < s->plan.n && s->plan.b[i].start < B; i++) {
        plan_block_t b = s->plan.b[i];
        if (b.start + b.bars > B) b.bars = (uint8_t)(B - b.start);
        m.b[m.n++] = b;
    }
    for (i = 0; i < np.n && m.n < 24; i++) {
        plan_block_t b = np.b[i];
        if (b.start + b.bars <= B) continue;
        if (b.start < B) {
            b.bars = (uint8_t)(b.start + b.bars - B);
            b.start = (uint8_t)B;
        }
        m.b[m.n++] = b;
    }
    s->plan = m;
}

/* ---- requests landing at B ---- */

enum { RE_PLAN = 1, RE_MELODY = 2, RE_PHRASES = 4 };

static int apply_one(song_t *s, int f, int B)
{
    song_roll_t before = s->roll;
    int v = s->pend_val[f];
    s->pend[f] = 0;
    switch (f) {
    case RF_KEY: {
        int d;
        roll_set(&s->roll, RF_KEY, v);
        d = ((s->roll.v[RF_KEY] - before.v[RF_KEY]) % 12 + 18) % 12 - 6;   /* -6..5 */
        if (d && !transpose_motif(s, d)) return RE_MELODY;
        return d ? RE_PHRASES : 0;
    }
    case RF_TEMPO: {
        float to;
        roll_set(&s->roll, RF_TEMPO, v);
        s->roll.v[RF_BARS] = before.v[RF_BARS];                           /* the bar count never changes */
        to = ss_of(s->roll.v[RF_TEMPO]);
        if (to != s->glide_to) {
            s->glide_from = s->glide_to;
            s->glide_to = to;
            s->glide_t0 = B * 16;
        }
        return 0;
    }
    case RF_MOOD:
        roll_set(&s->roll, RF_MOOD, v);
        if (s->roll.v[RF_MOOD] == before.v[RF_MOOD]) return 0;
        roll_dice(&s->roll, RF_PROG, (uint32_t)B);
        return RE_MELODY;
    case RF_BARS:
        return 0;
    default:
        roll_set(&s->roll, f, v);
        s->roll.v[RF_BARS] = before.v[RF_BARS];
        if (memcmp(&before, &s->roll, sizeof before) == 0) return 0;
        if (f == RF_MOTION || f == RF_MAP) return RE_PLAN;
        if (f == RF_MODE || f == RF_PROG || f == RF_COLOR) return RE_MELODY;
        return 0;
    }
}

static void apply_requests(song_t *s, int B)
{
    int f, re = 0;
    if (s->pend[RF_MOOD] && s->pend_bar[RF_MOOD] == B) re |= apply_one(s, RF_MOOD, B);
    for (f = 0; f < RF_N; f++)
        if (s->pend[f] && s->pend_bar[f] == B) re |= apply_one(s, f, B);
    if (re & RE_PLAN) merge_plan(s, B);
    if (re & RE_MELODY) melody_motif_for(&s->roll, &s->plan, s->roll.seed, &s->motif);
    if (re) melody_plan_for(&s->roll, &s->plan, &s->motif, &s->ph);
}

/* ---- the public face ---- */

void song_start(song_t *s, const song_roll_t *r)
{
    memset(s, 0, sizeof *s);
    s->roll = *r;
    plan_build(&s->roll, &s->plan);
    melody_motif_for(&s->roll, &s->plan, s->roll.seed, &s->motif);
    melody_plan_for(&s->roll, &s->plan, &s->motif, &s->ph);
    s->glide_to = s->glide_from = s->ss = ss_of(s->roll.v[RF_TEMPO] > 0 ? s->roll.v[RF_TEMPO] : 80);
    s->glide_t0 = -64;
    s->state = s->roll.v[RF_BARS] > 0 ? SS_PLAYING : SS_ENDED;
}

int song_tick(song_t *s, song_ev_t *out, int max)
{
    song_ev_t buf[BUF];
    const plan_block_t *b;
    int bar, step, idx, n = 0, i, j, k;
    uint32_t salt;
    float gs;
    if (!s || s->state != SS_PLAYING) return 0;
    bar = s->bar;
    step = s->step;
    idx = bar * 16 + step;
    if (step == 0 && bar % 4 == 0) {
        apply_requests(s, bar);
        find_edge(s, idx);
    }
    s->ss = ss_at(s, idx);
    /* the generators size held notes from one step's samples; during a speed-up glide later steps are shorter, so
       they get the glide's target and held notes end early rather than late */
    gs = s->ss < s->glide_to ? s->ss : s->glide_to;
    salt = s->roll.seed;
    b = plan_at(&s->plan, bar);
    if (b) {
        int pcs[5], np, pass = bar / loop_bars(&s->roll) + 1, dark = (b->flags & PB_DARK) != 0;
        np = harmony_chord_at(&s->roll, bar, pass, dark, pcs);
        n += groove_step(&s->roll, b, bar, step, gs, pcs, np, salt, buf + n, BUF - n);
        n += harmony_step(&s->roll, b, bar, step, pass, gs, s->voicing, salt, buf + n, BUF - n);
        n += melody_step(&s->roll, &s->plan, &s->ph, &s->motif, bar, step, gs, salt, buf + n, BUF - n);
    }
    for (i = k = 0; i < n; i++) {                   /* the 4-bar note-off line */
        uint32_t at = s->pos + buf[i].delay;
        if (buf[i].voice <= V_CANDY && buf[i].len) {
            if (at >= s->edge) continue;            /* would start past the line: dropped, not clicked */
            if (at + buf[i].len > s->edge) buf[i].len = s->edge - at;
        }
        buf[k++] = buf[i];
    }
    n = k;
    for (i = 1; i < n; i++)                         /* stable sort by delay */
        for (j = i; j > 0 && buf[j].delay < buf[j - 1].delay; j--) {
            song_ev_t t = buf[j];
            buf[j] = buf[j - 1];
            buf[j - 1] = t;
        }
    if (n > max) n = max < 0 ? 0 : max;
    if (n) memcpy(out, buf, sizeof(song_ev_t) * (size_t)n);
    advance(&s->pos, &s->frac, s->ss);
    if (++s->step == 16) {
        s->step = 0;
        if (++s->bar >= s->roll.v[RF_BARS]) s->state = SS_ENDED;
    }
    return n;
}

float song_step_samples(const song_t *s) { return s->ss; }

void song_request(song_t *s, int field, int value)
{
    int B;
    if (!s || s->state != SS_PLAYING || field < 0 || field >= RF_N || field == RF_BARS) return;
    B = (s->step == 0 && s->bar % 4 == 0) ? s->bar : (s->bar / 4 + 1) * 4;
    if (B >= s->roll.v[RF_BARS]) return;            /* the song ends first */
    s->pend[field] = 1;
    s->pend_val[field] = (int16_t)value;
    s->pend_bar[field] = (int16_t)B;
}

int song_pending(const song_t *s, int *field, int *bars_left)
{
    int f, best = -1;
    if (!s || s->state != SS_PLAYING) return 0;
    for (f = 0; f < RF_N; f++)
        if (s->pend[f] && (best < 0 || s->pend_bar[f] < s->pend_bar[best])) best = f;
    if (best < 0) return 0;
    if (field) *field = best;
    if (bars_left) *bars_left = s->pend_bar[best] - s->bar;
    return 1;
}

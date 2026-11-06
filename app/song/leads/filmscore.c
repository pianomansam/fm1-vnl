/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Film Score (lead-styles design, 2026-10-29; ported unchanged from its spike but for the shared register): a
   singable 2-bar motif, drawn once per song: a bar of 3-5 notes on the 8th grid over the mode's pentatonic, mostly
   by step, into a long note held through the next bar. A 4-bar phrase twice per 8 (question, answer):
     question  bars 1-2 the motif; bars 3-4 the motif sequenced up a pentatonic step (the lift), its long note open
               (not the root), then a breath
     answer    bars 1-2 the motif again; bar 3 its rhythm falling by step home (root or 3rd), at later phrases often
               from a leap up a 6th or an octave (the climax); bar 4 the home note held, then silence
   Every phrase head restates the motif (rhythm and up/down shape, refitted to the chord); a head after the first
   varies one thing at most (a step higher, or its long note an 8th early or late). Bar 2 may add a 2-note pickup
   into bar 3. Rubato: a long note lands a third of a 16th late about a quarter of the time. Every note passes
   leads_legal(); a strong-beat onset (steps 0, 8) is a chord tone; a tie is legal over both chords. Generic shapes
   drawn from the seed; no melody is stored. */
#include <stdint.h>
#include "leads.h"
#include "rng.h"
#include "override.h"

typedef struct { int n, on[6], off[6], lo, hi; } fs_motif_t;   /* onsets (16ths, bar 1) and pentatonic offsets
                                                                  (the last note the long one), their span */
typedef struct { int var, tail, tie, late, climax, leap; } fs_pass_t;
enum { FV_SAME, FV_UP, FV_EARLY, FV_LATE };
enum { FK_ANY, FK_LONG, FK_OPEN, FK_HOME, FK_LEAP };   /* what a note is: any; the long note; the question's long
                                                          note (not the root); home (the root or 3rd); a climax leap */

static void fs_song(uint32_t seed, int motion, fs_motif_t *m)
{
    static const uint8_t LW[3] = {40, 25, 35}, SW[3] = {60, 20, 20}, CW[4] = {35, 25, 20, 20};
    static const int8_t LAND[3] = {8, 10, 12}, START[3] = {0, 2, 4};
    int slots[8], ns = 0, i, j, t, land, start, shape, skip;
    rng_t g;
    rng_seed(&g, seed, 300);
    land = LAND[rng_weighted(&g, LW, 3)];
    start = START[rng_weighted(&g, SW, 3)];
    m->n = motion < 40 ? 3 + rng_int(&g, 2) : 4 + rng_int(&g, 2);
    for (i = start + 2; i < land; i += 2) slots[ns++] = i;
    if (m->n - 2 > ns) m->n = ns + 2;
    for (i = 0; i < m->n - 2; i++) {                     /* the middle onsets: n-2 of the 8th slots, in order */
        j = i + rng_int(&g, ns - i);
        t = slots[i]; slots[i] = slots[j]; slots[j] = t;
    }
    for (i = 0; i < m->n - 2; i++)
        for (j = i + 1; j < m->n - 2; j++)
            if (slots[j] < slots[i]) { t = slots[i]; slots[i] = slots[j]; slots[j] = t; }
    m->on[0] = start;
    for (i = 0; i < m->n - 2; i++) m->on[i + 1] = slots[i];
    m->on[m->n - 1] = land;
    shape = rng_weighted(&g, CW, 4);                     /* arch, rise, fall, valley; by step, one skip of 2 */
    skip = rng_int(&g, 10) < 3 ? 1 + rng_int(&g, m->n - 1) : 0;
    m->off[0] = m->lo = m->hi = 0;
    for (i = 1; i < m->n; i++) {
        int up = shape == 0 ? i < m->n - 1 : shape == 1 ? 1 : shape == 2 ? 0 : i == m->n - 1 || i > (m->n - 1) / 2;
        int d = i == skip ? 2 : 1;
        m->off[i] = m->off[i - 1] + (up ? d : -d);
        if (m->off[i] < m->lo) m->lo = m->off[i];
        if (m->off[i] > m->hi) m->hi = m->off[i];
    }
}

static void fs_pass(uint32_t seed, int run, int pass, int half, fs_pass_t *p)
{
    static const uint8_t VW[4] = {50, 20, 15, 15};
    rng_t g;
    rng_seed(&g, seed ^ ((uint32_t)run * 0x27D4EB2Fu) ^ ((uint32_t)(pass * 2 + half) * 0x165667B1u), 301);
    p->var = rng_weighted(&g, VW, 4);
    p->tail = rng_int(&g, 2);
    p->tie = rng_int(&g, 10) < 7;
    p->late = rng_int(&g, 4) == 0;
    p->climax = rng_int(&g, 100) < (pass ? 60 : 25);
    p->leap = rng_int(&g, 2) ? 12 : 9;                   /* an octave, or a 6th */
}

/* the note for a planned one: legal (over the next chord too, x), a chord tone on a strong beat, near want, moving
   from prev in dir (+1, -1; 0 any), no leap past a 6th unless it is the climax; by kind (FK_*) */
static int fs_fit(const leads_ctx_t *c, int want, int prev, int dir, int strong, const leads_next_t *x, int kind,
                  unsigned pent)
{
    int note, best = 0, bc = 1 << 30, lo = x ? (c->lo_x > c->lo ? c->lo_x : c->lo) : c->lo;
    for (note = lo; note <= c->hi; note++) {
        int iv = (note - c->pcs[0] + 120) % 12, m = prev ? note - prev : 0, ct = leads_is_ct(note, c->pcs, c->np);
        int cost;
        if (!leads_ok(c, note, strong)) continue;
        if (x && !leads_legal(c->r, note, x->pcs, x->np, x->bass)) continue;
        cost = (note > want ? note - want : want - note) * 2;
        if (prev && dir && ((m > 0) - (m < 0) != dir || !m)) cost += 40;
        if (prev && kind != FK_LEAP && (m > 9 || m < -9)) cost += 30;
        if (!ct && !((pent >> (note % 12)) & 1u)) cost += 3;
        if (kind != FK_ANY && kind != FK_LEAP && !ct && iv != 2) cost += 3;   /* a long note: the chord's, or the 9th */
        if (kind == FK_OPEN && iv == 0) cost += 6;
        if (kind == FK_HOME && !(ct && (iv == 0 || iv == 3 || iv == 4))) cost += 12;
        if (c->keys[3] && note == c->keys[3] + 12) cost += 4;
        if (cost < bc) { bc = cost; best = note; }
    }
    return best;
}

/* the start pitch t0 moved by pentatonic steps until the whole shape (offsets off[]) sits in the bar's range, a
   step to spare under it */
static int fs_fit_range(const leads_ctx_t *c, int t0, const int *off, int n, unsigned pent)
{
    int k, lo = 0, hi = 0, s;
    for (k = 0; k < n; k++) {
        if (off[k] < lo) lo = off[k];
        if (off[k] > hi) hi = off[k];
    }
    for (s = 0; s < 6 && leads_coll_step(pent, t0, lo - 1) < c->lo; s++) t0 = leads_coll_step(pent, t0, 1);
    for (s = 0; s < 6 && leads_coll_step(pent, t0, hi) > c->hi; s++) t0 = leads_coll_step(pent, t0, -1);
    return t0;
}

/* one statement: the rhythm on[] with pentatonic offsets off[] from start pitch t0, each step from the note sung
   before; the long note (kind lk) held to the bar's end, or tied `tie` 16ths into the next bar where legal over both.
   Returns the last note sung (0: none); *tied the 16ths tied over. */
static int fs_state(lead_state_t *ls, const leads_ctx_t *c, const int *on, const int *off, int n, int t0, int from,
                    const leads_next_t *x, int tie, int late, int lk, int v0, unsigned pent, int *tied)
{
    int k, prev = 0, want, note;
    *tied = 0;
    want = t0 = fs_fit_range(c, t0, off, n, pent);
    for (k = 0; k < n; k++) {
        int at = on[k], longn = k == n - 1, dir = k ? (off[k] > off[k - 1]) - (off[k] < off[k - 1]) : 0;
        const leads_next_t *xx = longn && tie ? x : 0;
        if (at < from) continue;
        if (k) want = leads_coll_step(pent, prev ? prev : want, off[k] - off[k - 1]);
        note = fs_fit(c, want, prev, dir, at % 8 == 0, xx, longn ? lk : FK_ANY, pent);
        if (!note && xx) { xx = 0; note = fs_fit(c, want, prev, dir, at % 8 == 0, 0, longn ? lk : FK_ANY, pent); }
        if (!note) continue;
        if (longn) {
            int end = xx && note >= c->lo_x ? 16 + tie : 16;
            leads_put(ls, c, at * 3 + (late ? 1 : 0), note, end, v0 + 8);
            ls->acc |= 1u << at;
            if (end > 16) { ls->ring |= 1u << at; *tied = tie; }
        } else {
            leads_put(ls, c, at * 3, note, on[k + 1], v0 + 3 * (off[k] > 0 ? off[k] : 0));
        }
        prev = note;
    }
    return prev;
}

void filmscore_plan(lead_state_t *ls, const leads_ctx_t *c, const plan_block_t *b, int bar, const leads_next_t *nx)
{
    int run, pass, bip, pbars, last, role, half, i, key = c->r->v[RF_KEY], center, prev = ls->last, sung;
    int from = ls->tied ? ls->tie_end : 0, base, on[6], n, tied = 0, lvl, answer_at;
    uint32_t seed = song_ov_phrase_seed(c->r->seed);
    unsigned pent = leads_pent_mask(c->r, 0);
    fs_motif_t m;
    fs_pass_t p;
    const leads_next_t *x = c->can_push && nx->np > 0 ? nx : 0;
    ls->tied = 0;
    ls->tie_end = 0;
    leads_phrase_at(b, bar, &run, &pass, &bip, &pbars, &last);
    role = bip % 4;
    half = bip / 4;
    if (pbars < 8 && last) half = 1;
    fs_song(seed, c->r->v[RF_MOTION], &m);
    fs_pass(seed, run, pass, half, &p);
    center = c->hi - 7 < 74 ? c->hi - 7 : 74;
    if (center < c->lo + 5) center = c->lo + 5;
    base = leads_place(key % 12, center - 6, center + 5, center);   /* home, the motif's span centered on it */
    if (!base) base = center;
    base = leads_coll_step(pent, base, -(m.lo + m.hi + 1) / 2);
    lvl = (half || pass) && p.var == FV_UP ? 1 : 0;          /* the head a step higher (its one change) */
    answer_at = m.hi > 1 ? m.hi - 1 : 1;                     /* the answer's fall starts near the motif's top */
    if (bip == 0) prev = 0;
    n = m.n;
    for (i = 0; i < n; i++) on[i] = m.on[i];
    if ((half || pass) && (p.var == FV_EARLY || p.var == FV_LATE)) {   /* or its long note an 8th early or late */
        int nl = on[n - 1] + (p.var == FV_EARLY ? -2 : 2);
        if (nl > on[n - 2] && nl <= 14) on[n - 1] = nl;
    }
    switch (role) {
    case 0:     /* the motif */
        sung = fs_state(ls, c, on, m.off, n, leads_coll_step(pent, base, lvl), from, x, p.tie ? (p.tail ? 6 : 8) : 0,
                        p.late, FK_LONG, 64, pent, &tied);
        if (sung) prev = sung;
        break;
    case 2:
        if (!half) {   /* the question: the motif a step up (the lift), its long note open, held into bar 4 */
            sung = fs_state(ls, c, on, m.off, n, leads_coll_step(pent, base, lvl + 1), from, x, 10, p.late, FK_OPEN,
                            70, pent, &tied);
            if (sung) prev = sung;
        } else {       /* the answer: the motif's rhythm falling by step home, from a climax leap at later phrases */
            int off[6], peak = 0, k0 = 0;
            for (i = 0; i < n; i++) off[i] = -i;
            if (p.climax && prev && on[0] >= from) {
                peak = fs_fit(c, prev + p.leap, prev, 1, on[0] % 8 == 0, 0, FK_LEAP, pent);
                if (peak && peak - prev < 8) peak = 0;       /* a 6th at least, or no climax */
            }
            if (peak) {
                leads_put(ls, c, on[0] * 3 + (p.late ? 1 : 0), peak, on[1], 86);
                ls->acc |= 1u << on[0];
                k0 = 1;
            }
            sung = fs_state(ls, c, on + k0, off + k0, n - k0, peak ? leads_coll_step(pent, peak, -1)
                            : leads_coll_step(pent, base, answer_at), from, x, p.tie ? 10 : 0, 0, FK_HOME,
                            peak ? 76 : 70, pent, &tied);
            prev = sung ? sung : peak ? peak : prev;
        }
        break;
    case 1: {   /* the hold, then a pickup into bar 3 (or nothing) */
        int t0 = from, fall[6], tgt;
        for (i = 0; i < n; i++) fall[i] = -i;
        tgt = fs_fit_range(c, leads_coll_step(pent, base, half ? answer_at : lvl + 1), half ? fall : m.off, n, pent);
        if (!from && prev) {                                 /* not tied: the long note again on 1 */
            int a = fs_fit(c, prev, 0, 0, 1, 0, FK_LONG, pent);
            if (a) { leads_put(ls, c, 0, a, p.tail ? 8 : 10, 66); prev = a; }
            t0 = 8;
        }
        if (half && p.climax) tgt = leads_coll_step(pent, base, m.lo - 2);   /* the answer's climax: the pickup dips
                                                                                low, so the leap up has room */
        if ((p.tail || (half && p.climax)) && prev && t0 <= 10) {
            int d = tgt > prev ? -1 : 1, a = fs_fit(c, leads_coll_step(pent, tgt, 2 * d), prev, 0, 0, 0, FK_ANY, pent);
            int bn = a ? fs_fit(c, leads_coll_step(pent, tgt, d), a, -d, 0, 0, FK_ANY, pent) : 0;
            if (a && bn && a != prev) {
                leads_put(ls, c, 36, a, 14, 60);
                leads_put(ls, c, 42, bn, 16, 64);
                prev = bn;
            }
        }
        break;
    }
    default:    /* the phrase's end: held (tied in), else struck on 1; then silence */
        if (!from && prev) {
            int a = fs_fit(c, prev, 0, 0, 1, 0, half ? FK_HOME : FK_OPEN, pent);
            if (a) { leads_put(ls, c, 0, a, half ? 12 : 10, 60); prev = a; }
        }
        break;
    }
    if (tied) { ls->tied = 1; ls->tie_end = tied; }
    ls->last = prev;
}

/* The song's motif: its phrase heads' notes as the song draws them (before the legal notes and the range bend them):
   3-5 notes on the 8th grid in the mode's pentatonic, the last the long note, held to the bar's end (often tied on
   through the next). */
void filmscore_motif(const song_roll_t *r, lead_motif_t *m)
{
    fs_motif_t f;
    int at[6], len[6], k;
    fs_song(song_ov_phrase_seed(r->seed), r->v[RF_MOTION], &f);
    for (k = 0; k < f.n; k++) {
        at[k] = f.on[k] - f.on[0];
        len[k] = (k + 1 < f.n ? f.on[k + 1] : 16) - f.on[k];
    }
    lead_motif_from(m, f.n, at, len, f.off);
    m->scale = LM_PENT;
}

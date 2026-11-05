/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Impressionism (lead-styles design, 2026-10-29; ported unchanged from its spike but for the shared register): a
   4-bar phrase, twice per 8 (question, answer), in four roles:
     bar 1  the arabesque: 3-5 quick notes (16ths or 8th triplets) curling into a long color note (a 9th, 13th or
            11th where legal, the root least), landing behind the beat, often tied over the bar line
     bar 2  the float: the held note, then a sigh (a step down), a pickup, or nothing (long notes)
     bar 3  the arabesque again, varied (extended, shifted, re-timed, or a whole-tone tetrachord over a dominant),
            landing on the phrase's peak (the question) or lower (the answer)
     bar 4  the fall: a descending sigh into a resting note, then at least 6 16ths of silence
   Pentatonic-leaning (over the chord), the chord's color note in modal songs. Every note passes leads_legal();
   a strong-beat onset (steps 0, 8) is a chord tone. Planned a bar at a time from (seed, run, pass). */
#include <stdint.h>
#include "leads.h"
#include "rng.h"
#include "override.h"

enum { SH_TURN, SH_RISE, SH_FALL, SH_CURL, SH_N };
static const int8_t SHAPE[SH_N][6] = {
    {-1, 0, +1, 0, +1, -1}, {-6, -5, -4, -3, -2, -1}, {+6, +5, +4, +3, +2, +1}, {-3, -2, -1, +1, +2, +1}};

typedef struct { int shape, trip, n, land, modal; } im_song_t;
typedef struct { int var, float_kind, tie, late, shift; } im_pass_t;
enum { DV_SAME, DV_EXT, DV_SHIFT, DV_UNIT, DV_WT };

static void im_song(uint32_t seed, int motion, im_song_t *s)
{
    static const uint8_t SW[SH_N] = {30, 25, 25, 20}, LW[4] = {20, 35, 25, 20};
    static const int8_t LAND[4] = {4, 6, 8, 10};
    rng_t g;
    rng_seed(&g, seed, 200);
    s->shape = rng_weighted(&g, SW, SH_N);
    s->trip = rng_int(&g, 10) < 4;
    s->n = 3 + rng_int(&g, 3);
    if (motion < 40 && s->n > 3) s->n = 3;
    if (motion > 70 && s->n < 5) s->n++;
    s->land = LAND[rng_weighted(&g, LW, 4)];
    s->modal = rng_int(&g, 100) < 35;
}

static void im_pass(uint32_t seed, int run, int pass, int half, im_pass_t *p)
{
    static const uint8_t VW[5] = {15, 25, 20, 15, 25}, FW[3] = {50, 30, 20};
    rng_t g;
    rng_seed(&g, seed ^ ((uint32_t)run * 0x27D4EB2Fu) ^ ((uint32_t)(pass * 2 + half) * 0x165667B1u), 201);
    p->var = rng_weighted(&g, VW, 5);
    p->float_kind = rng_weighted(&g, FW, 3);
    p->tie = rng_int(&g, 10) < 6;
    p->late = rng_int(&g, 4) == 0;
    p->shift = rng_int(&g, 2) ? 2 : -2;
}

static unsigned im_mask(const leads_ctx_t *c, int which)   /* 0 the scale, 1 its pentatonic (+ color: 2) */
{
    return which == 0 ? leads_scale_mask(c->r, c->pcs, c->np) : leads_pent_mask(c->r, c->pcs, c->np, which == 2);
}

/* the long note: legal (over the next chord too when tied, x), a chord tone on a strong beat; near target, the
   root least, the color tones (9th, 13th; the 11th over a minor chord) first */
static int im_color(const leads_ctx_t *c, int target, int strong, const leads_next_t *x, int prev, unsigned pent)
{
    int note, best = 0, bc = 1 << 30, lo = x ? c->lo_x : c->lo, minor = 0, j;
    for (j = 0; j < c->np; j++) minor |= (c->pcs[j] - c->pcs[0] + 12) % 12 == 3;
    for (note = lo; note <= c->hi; note++) {
        int iv = (note - c->pcs[0] + 120) % 12, cost;
        if (!leads_legal(c->r, note, c->pcs, c->np, c->bass)) continue;
        if (x && !leads_legal(c->r, note, x->pcs, x->np, x->bass)) continue;
        if (strong && !leads_is_ct(note, c->pcs, c->np)) continue;
        cost = (note > target ? note - target : target - note) * 2;
        if (iv == 0) cost += 8;
        if (iv == 7) cost += 2;
        if (iv == 2 || iv == 9 || (minor && iv == 5)) cost -= 4;
        if (!leads_is_ct(note, c->pcs, c->np) && !((pent >> (note % 12)) & 1u)) cost += 3;
        if (prev && (note - prev > 9 || prev - note > 9)) cost += 20;
        if (c->keys[3] && note == c->keys[3] + 12) cost += 4;
        if (cost < bc) { bc = cost; best = note; }
    }
    return best;
}

/* The scale's whole-tone tetrachord (three whole steps in a row: F G A B in C), all four legal over the chord: run
   through it (dir +1 up, -1 down) and turn back a whole step onto its third note, the landing. Diatonic, so it lives
   only where the chord leaves the four clear (a dominant's b7-1-9-3); the notes just past it are a semitone from
   them, so it can't run on into a landing beyond. Returns the landing nearest tgt (0: none), *st the run's first. */
static int im_wt(const leads_ctx_t *c, int tgt, int strong, unsigned scale, int dir, int *st)
{
    int s, k, best = 0, bc = 1 << 30;
    for (s = c->lo; s + 6 <= c->hi; s++) {
        int ok = 1, land, first = dir > 0 ? s : s + 6;
        for (k = 0; k <= 6 && ok; k += 2) ok = ((scale >> ((s + k) % 12)) & 1u) && leads_ok(c, s + k, 0);
        for (k = 1; k < 6 && ok; k += 2) ok = !((scale >> ((s + k) % 12)) & 1u);
        land = first + dir * 4;
        if (!ok || (strong && !leads_is_ct(land, c->pcs, c->np))) continue;
        if ((land > tgt ? land - tgt : tgt - land) < bc) {
            bc = land > tgt ? land - tgt : tgt - land;
            best = land;
            *st = first;
        }
    }
    return best;
}

/* the arabesque into a landing at 16th `at`, its notes from 16th `from` on */
static void im_arabesque(lead_state_t *ls, const leads_ctx_t *c, int shape, int n, int trip, int at, int land,
                         int from, unsigned coll, int wt, int v0)
{
    int u = trip ? 4 : 3, k, p, prev = 0, notes[8], ps[8], m = 0;
    if (wt) {   /* wt: the tetrachord's first note, its direction from where it sits against the landing */
        int dir = land > wt ? 1 : -1;
        for (k = 0; k < 4; k++) {
            p = at * 3 - (4 - k) * u;
            if (p >= from * 3) { notes[m] = wt + dir * 2 * k; ps[m] = p; m++; }
        }
    }
    if (!m) {   /* the shape, or its mirror where more of it fits (the range's edge), walked over the collection's
                   legal notes only (a note that would rub is stepped past, not leapt over) */
        int v, mm[4], nn[4][8], pp[4][8], sc[4], bv = 0;
        unsigned lg = leads_legal_pcs(c->r, c->pcs, c->np, c->bass), cl[2];
        cl[0] = coll & lg;                                   /* the pentatonic first, the scale where it's smoother */
        cl[1] = lg;                                          /* (legal: the scale and the chord's own tones) */
        for (v = 0; v < 4; v++) {                            /* v: bit 0 the mirror, bit 1 the scale */
            int flip = v & 1, prevn = land, big = 0;
            unsigned cm = cl[v >> 1];
            mm[v] = 0;
            if (!cm) { sc[v] = -999; continue; }
            for (k = n - 1; k >= 0; k--) {                   /* backward from the landing: the gaps it leaves */
                int off = SHAPE[shape][6 - n + k] * (flip ? -1 : 1), note = leads_coll_step(cm, land, off);
                p = at * 3 - (n - k) * u;
                if (p < from * 3 || !leads_ok(c, note, p % 24 == 0) || note == land) continue;
                if (note - prevn > 4 || prevn - note > 4) { big++; break; }   /* a gap: it starts after it */
                nn[v][mm[v]] = note; pp[v][mm[v]] = p; mm[v]++;
                prevn = note;
            }
            sc[v] = mm[v] * 10 - big * 12 - (v & 1) * 6 - (v >> 1) * 4;
            if (sc[v] > sc[bv]) bv = v;
        }
        for (k = mm[bv] - 1; k >= 0; k--) { notes[m] = nn[bv][k]; ps[m] = pp[bv][k]; m++; }
    }
    for (k = 0; k < m; k++) {
        if (ps[k] < from * 3 || notes[k] == prev) continue;
        leads_put(ls, c, ps[k], notes[k], k + 1 < m ? (ps[k + 1] + 2) / 3 : at, v0 + 3 * k);
        prev = notes[k];
    }
}

void impressionism_plan(lead_state_t *ls, const leads_ctx_t *c, const plan_block_t *b, int bar,
                        const leads_next_t *nx)
{
    int run, pass, bip, pbars, last, role, half, key = c->r->v[RF_KEY], center, prev = ls->last;
    int from = ls->tied ? ls->tie_end : 0, anchor, tgt, land, at, end;
    uint32_t seed = song_ov_phrase_seed(c->r->seed);
    unsigned scale = im_mask(c, 0), coll;
    im_song_t s;
    im_pass_t p;
    const leads_next_t *x = c->can_push && nx->np > 0 ? nx : 0;
    ls->tied = 0;
    ls->tie_end = 0;
    leads_phrase_at(b, bar, &run, &pass, &bip, &pbars, &last);
    role = bip % 4;
    half = bip / 4;
    if (pbars < 8 && last) half = 1;
    im_song(seed, c->r->v[RF_MOTION], &s);
    im_pass(seed, run, pass, half, &p);
    coll = im_mask(c, s.modal ? 2 : 1);
    center = c->hi - 7 < 74 ? c->hi - 7 : 74;                /* the flute's middle, never its top */
    if (center < c->lo + 5) center = c->lo + 5;
    anchor = leads_place(key % 12, center - 6, center + 5, center);   /* the phrase's home pitch */
    if (!anchor) anchor = center;
    if (bip == 0) prev = 0;
    switch (role) {
    case 0:   /* the arabesque into a long color note */
    case 2: {
        int n = s.n, trip = s.trip, wt = 0, la = s.land, shape = s.shape;
        if (role == 0 && (half || pass) && p.var != DV_SAME) {   /* the head after the first: varied (re-timed,
                                                                     extended by one, or turned) */
            if (p.var == DV_SHIFT) la = la + p.shift < 4 ? 4 : la + p.shift > 10 ? 10 : la + p.shift;
            else if (p.var == DV_UNIT) trip = !trip;
            else if (p.var == DV_EXT) n = n + 1 > 6 ? 6 : n + 1;
            else shape = (shape + 1 + (p.shift > 0)) % SH_N;
        }
        if (role == 2) {
            if (p.var == DV_EXT) n = n + 2 > 6 ? 6 : n + 2;
            if (p.var == DV_SHIFT) la = la + p.shift < 4 ? 4 : la + p.shift > 10 ? 10 : la + p.shift;
            if (p.var == DV_UNIT) trip = !trip;
            if (p.var == DV_WT) wt = 1;
        }
        tgt = leads_coll_step(scale, anchor, role == 0 ? 0 : half ? -1 : 2);
        at = la;
        if (at < from + 2) at = from + 2;
        while (at * 3 - n * (trip ? 4 : 3) < from * 3 && n > 2) n--;
        land = 0;
        if (wt) {
            int st = 0, dir = SHAPE[shape][5] < 0 ? 1 : -1;   /* rising into the landing, or falling */
            if (at * 3 - 4 * (trip ? 4 : 3) < from * 3) at = from + (trip ? 6 : 4);
            land = im_wt(c, tgt, at % 8 == 0, scale, dir, &st);
            if (!land) land = im_wt(c, tgt, at % 8 == 0, scale, -dir, &st);
            if (land && (land - tgt > 7 || tgt - land > 7)) land = 0;
            wt = land ? st : 0;
        }
        if (!land) land = im_color(c, tgt, at % 8 == 0, p.tie && role == 0 ? x : 0, prev, coll);
        if (!land && p.tie) land = im_color(c, tgt, at % 8 == 0, 0, prev, coll);
        if (!land) break;
        im_arabesque(ls, c, shape, n, trip, at, land, from, coll, wt, 56);
        if (role == 0 && p.tie && x && leads_legal(c->r, land, x->pcs, x->np, x->bass) && land >= c->lo_x) {
            end = 16 + (p.float_kind == 2 ? 12 : 6);
            ls->ring |= 1u << at;
            ls->tied = 1;
            ls->tie_end = end - 16;
        } else {
            end = role == 0 ? 16 : 15;
        }
        leads_put(ls, c, at * 3 + (p.late ? 1 : 0), land, end, 80);
        ls->acc |= 1u << at;
        prev = land;
        break;
    }
    case 1: {   /* the float: the held note, then a sigh, a pickup, or nothing */
        int t0 = from;
        if (!from) {                                          /* not tied: a long chord tone on 1 */
            land = im_color(c, leads_coll_step(scale, anchor, 0), 1, 0, prev, coll);
            if (land) { leads_put(ls, c, 0, land, 8, 74); prev = land; }
            t0 = 8;
        }
        if (p.float_kind == 0 && prev) {                     /* a sigh: up a step, then down to rest on 4 */
            int a = 0, bn = 0, k;
            for (k = 1; k <= 2 && !a; k++)
                if (leads_ok(c, leads_coll_step(scale, prev, k), 0)) a = leads_coll_step(scale, prev, k);
            for (k = 1; a && k <= 2 && !bn; k++)
                if (leads_ok(c, leads_coll_step(scale, a, -k), 0)) bn = leads_coll_step(scale, a, -k);
            if (a && bn && t0 <= 10) {
                leads_put(ls, c, 30, a, 12, 70);
                leads_put(ls, c, 36, bn, 16, 60);
                prev = bn;
            }
        } else if (p.float_kind == 1 && prev && t0 <= 12) {  /* a pickup rising into bar 3 */
            int a = leads_coll_step(coll, prev, 1), bn = leads_coll_step(coll, prev, 2);
            if (leads_ok(c, a, 0) && leads_ok(c, bn, 0)) {
                leads_put(ls, c, 42, a, 15, 60);
                leads_put(ls, c, 45, bn, 16, 64);
                prev = bn;
            }
        }
        break;
    }
    default: {  /* the fall: a descending sigh into a resting note, then silence */
        int goal = leads_coll_step(scale, anchor, half ? -3 : -1), a = 0, rest = 0, k;
        for (k = 0; k < 4 && !rest; k++) {
            int cand = im_color(c, goal - 2 * k, half, 0, prev, coll);   /* the answer rests on a chord tone */
            if (cand && (!prev || cand < prev || k == 3)) rest = cand;
        }
        if (!rest) break;
        if (prev > rest + 2 && from <= 2) {                  /* down from the peak by steps: a chain of sighs in 8ths
                                                                from the "and" of 1, the rest note after it */
            int ch[3], nc = 0, q = prev, rt;
            while (nc < 3 && (q = leads_coll_step(coll, q, -1)) > rest)
                if (leads_ok(c, q, 0)) ch[nc++] = q;
            if (nc == 3 && !leads_is_ct(rest, c->pcs, c->np)) nc = 2;   /* the rest would land on beat 3 */
            rt = 2 + 2 * nc;
            for (k = 0; k < nc; k++) leads_put(ls, c, (2 + 2 * k) * 3, ch[k], 4 + 2 * k, 70 - 4 * k);
            leads_put(ls, c, rt * 3, rest, rt + 8 > 16 ? 16 : rt + 8, half ? 56 : 62);
            prev = rest;
            break;
        }
        for (k = 1; k <= 2 && !a; k++)
            if (leads_ok(c, leads_coll_step(scale, rest, k), 0) && leads_coll_step(scale, rest, k) != prev)
                a = leads_coll_step(scale, rest, k);
        if (from <= 2 && a) leads_put(ls, c, 6, a, 4, 68);
        if (from <= 4) { leads_put(ls, c, 12, rest, half ? 12 : 10, half ? 56 : 62); prev = rest; }
        break;
    }
    }
    ls->last = prev;
}

/* The song's motif: its phrase heads' arabesque as the song draws it (before any pass varies it, and before the
   legal notes and the range bend it): the shape's last n steps through the collection into the landing, a 16th or
   an 8th triplet apart, the landing held to the bar's end. */
void impressionism_motif(const song_roll_t *r, lead_motif_t *m)
{
    im_song_t s;
    int at[7], len[7], si[7], k, n = 0, u, p0 = -1;
    im_song(song_ov_phrase_seed(r->seed), r->v[RF_MOTION], &s);
    u = s.trip ? 4 : 3;
    for (k = 0; k <= s.n; k++) {
        int p = k < s.n ? s.land * 3 - (s.n - k) * u : s.land * 3;
        if (p < 0) continue;                                 /* before the bar: the plan leaves it out too */
        if (p0 < 0) p0 = p / 3;
        at[n] = p / 3 - p0;
        si[n] = k < s.n ? SHAPE[s.shape][6 - s.n + k] : 0;
        n++;
    }
    for (k = 0; k < n; k++) len[k] = k + 1 < n ? at[k + 1] - at[k] : 16 - s.land;   /* (a triplet's 16ths
                                                                                       never meet: 4 thirds apart) */
    lead_motif_from(m, n, at, len, si);
    m->scale = s.modal ? LM_PENT_COLOR : LM_PENT;
}

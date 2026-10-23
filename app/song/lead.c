/* SPDX-License-Identifier: MPL-2.0 */
#include <stdint.h>
#include "lead.h"
#include "harmony.h"
#include "library.h"
#include "theory.h"
#include "rng.h"

typedef struct {                  /* what one bar's planning reads */
    const song_roll_t *r;
    const int *keys, *pcs;
    int np;
    unsigned bass;                /* pitch classes the bass strikes in the bar */
    int lo, hi;                   /* the lead's range this bar (a 4th over percussive keys' top; set by a pad's place) */
    int lo_x;                     /* the floor for a note that sounds on into the next chord */
    int shift;                    /* the keys' octave shift (a pad with or above the lead) */
} ctx_t;

static int is_ct(int note, const int *pcs, int np)
{
    int j;
    for (j = 0; j < np; j++)
        if ((note - pcs[j] + 120) % 12 == 0) return 1;
    return 0;
}

/* a chord tone or a scale note, never a semitone from a chord tone or a note the bass strikes (R25: the maj7 over
   the root rubs), and not an avoid note */
static int legal(const song_roll_t *r, int note, const int *pcs, int np, unsigned bass)
{
    int j, pc = note % 12;
    if (th_is_avoid(r->v[RF_KEY], r->v[RF_MODE], pcs, np, note)) return 0;
    if (bass & (1u << ((pc + 1) % 12) | 1u << ((pc + 11) % 12))) return 0;
    for (j = 0; j < np; j++) {
        int d = (note - pcs[j] + 120) % 12;
        if (d == 1 || d == 11) return 0;
    }
    return is_ct(note, pcs, np) || th_in_scale(r->v[RF_KEY], r->v[RF_MODE], note);
}

/* the skeleton's range: E4-A5 as the spike sang it, raised to the floor, a pad placed above keeping it to C4-C5 */
static void skel_range(const ctx_t *c, int floor, int *lo, int *hi)
{
    *lo = c->hi <= 72 ? 60 : floor > 67 ? floor : 67;
    *hi = c->hi <= 72 ? 72 : *lo + 12 > 81 ? *lo + 12 : 81;
    if (*hi > c->hi) *hi = c->hi;
}

/* the chord tone nearest `from`, root least, within a 5th of the last skeleton note `prev` where one is */
static int skel_near(const ctx_t *c, const int *pcs, int np, int from, int prev, int floor)
{
    int best = -1, bc = 1 << 30, note, j, lo, hi, pass;
    for (pass = 0; pass < 3 && best < 0; pass++) {      /* the usual range, the lead's whole range, then a leap */
        skel_range(c, floor, &lo, &hi);
        if (pass == 1) { lo = floor > 60 ? floor : 60; hi = c->hi; }
        for (note = lo; note <= hi; note++)
            for (j = 0; j < np; j++)
                if ((note - pcs[j] + 120) % 12 == 0 && legal(c->r, note, pcs, np, c->bass) &&
                    (pass == 2 || !prev || (note > prev ? note - prev : prev - note) <= 7)) {
                int iv = (pcs[j] - pcs[0] + 12) % 12, cost = (note > from ? note - from : from - note) * 4;
                if (iv == 0) cost += 6;
                if (iv == 7) cost += 3;
                if (c->keys[3] && note == c->keys[3] + 12) cost += 4;
                if (cost < bc) { bc = cost; best = note; }
            }
    }
    return best;
}

static int skel_from(const ctx_t *c, const int *pcs, int np, int from, int floor)
{
    return skel_near(c, pcs, np, from, from, floor);
}

static int skel(const ctx_t *c, const int *pcs, int np, int from) { return skel_from(c, pcs, np, from, c->lo); }

static int place(int pc, int lo, int hi, int near)          /* pc's octave in [lo, hi] nearest `near`; 0 if none */
{
    int best = 0, note;
    for (note = lo; note <= hi; note++)
        if (note % 12 == pc && (!best || (note > near ? note - near : near - note) < (best > near ? best - near : near - best)))
            best = note;
    return best;
}

static int step_toward(const ctx_t *c, int from, int to)    /* a scale step from `from` toward `to` */
{
    int dir = to >= from ? 1 : -1, d;
    for (d = 1; d <= 2; d++)
        if (legal(c->r, from + dir * d, c->pcs, c->np, c->bass) && from + dir * d != to && from + dir * d >= c->lo &&
            from + dir * d <= c->hi)
            return from + dir * d;
    return 0;
}

static void put(lead_state_t *ls, int at, int note, int len)
{
    if (at < 0 || at > 15 || note <= 0) return;
    ls->on[at] = note;
    ls->len[at] = len;
}

static void land(lead_state_t *ls, int at, int note, int len)   /* the skeleton note, the arpeggio out of its way */
{
    int i;
    for (i = at < 0 ? 0 : at; i < at + len && i < 16; i++) ls->on[i] = 0;
    put(ls, at, note, len);
    if (at >= 0 && at < 16 && note > 0) ls->acc |= 1u << at;
}

/* a push: the next chord's note an 8th early, only when it is also legal over this chord */
static int push_ok(const ctx_t *c, int note) { return note > 0 && legal(c->r, note, c->pcs, c->np, c->bass); }

/* the sung line: the skeleton note per chord in the song's two rhythm cells, a breath at the phrase's end */
static void plan_line(lead_state_t *ls, const ctx_t *c, int bar, int bpc, const int *npcs, int nn)
{
    static const int8_t ARC[4] = {0, 4, 7, 3};          /* each phrase arcs away and back: up, then down the next */
    rng_t g;
    int prev = ls->last ? ls->last : 74, cur;
    int next, cells[2], cell, phrase_end = bar % 4 == 3, i;
    int start = ls->tied ? -1 : 0;
    if (bar % 4 == 0 || !ls->anchor) ls->anchor = prev;
    {
        int dir = (bar / 4) & 1 ? -1 : 1, ndir = ((bar + 1) / 4) & 1 ? -1 : 1;
        cur = bar % bpc && ls->last ? ls->last
                                    : skel_near(c, c->pcs, c->np, ls->anchor + dir * ARC[bar % 4], prev, c->lo);
        next = nn > 0 ? skel_near(c, npcs, nn, (bar + 1) % 4 ? ls->anchor + ndir * ARC[(bar + 1) % 4] : cur, cur,
                                  c->lo_x)
                      : 0;
    }
    rng_seed(&g, c->r->seed, 140);
    cells[0] = rng_int(&g, 5);
    cells[1] = (cells[0] + 1 + rng_int(&g, 4)) % 5;
    cell = cells[(bar / 2) & 1];
    if (ls->tied) cur = ls->last;                        /* pushed in from the last bar: it still rings */
    if (bar % bpc) {                                     /* a chord's second bar: hold, maybe a neighbor */
        if (cell == 3) { put(ls, 10, step_toward(c, cur, cur + 2), 2); put(ls, 12, cur, 4); }
        ls->last = cur;
        ls->tied = 0;
        return;
    }
    switch (cell) {
    case 0: put(ls, start, cur, 12); put(ls, 12, step_toward(c, cur, next ? next : cur), 4); break;   /* passing */
    case 1: put(ls, start, cur, 6); put(ls, 6, cur, 2); put(ls, 8, cur, 8); break;                     /* re-strike */
    case 2: put(ls, 2, ls->tied ? 0 : cur, 12); break;                                                /* late */
    case 3: put(ls, start, cur, 10); put(ls, 10, step_toward(c, cur, cur + 2), 2); put(ls, 12, cur, 4); break;
    default: put(ls, start, cur, 10); break;                                                          /* then a push */
    }
    if (ls->on[0]) ls->acc |= 1u;
    ls->tied = 0;
    if (phrase_end) {                                    /* breathe at the phrase's end */
        for (i = 8; i < 16; i++) ls->on[i] = 0;
        for (i = 0; i < 8; i++) if (ls->on[i] && i + ls->len[i] > 8) ls->len[i] = 8 - i;
    } else if ((cell == 4 || cell == 2) && (bar + 1) % bpc == 0 && push_ok(c, next)) {
        for (i = 14; i < 16; i++) ls->on[i] = 0;
        put(ls, 14, next, 2);
        ls->tied = 1;
        cur = next;
    } else if ((bar + 1) % bpc == 0 && next > 0 && (next - cur >= 3 || cur - next >= 3) && !ls->on[12] &&
               !ls->on[14]) {
        int pn = step_toward(c, cur, next);              /* a passing note into the next chord's note */
        for (i = 0; i < 14; i++) if (ls->on[i] && i + ls->len[i] > 14) ls->len[i] = 14 - i;
        put(ls, 14, pn, 2);
    }
    ls->last = cur;
}

/* the hybrid's arpeggio rhythms; the song's is the one that lands least on the snare (Sam), not forced */
static const int8_t RHY[6][9][2] = {
    {{0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2}, {12, 2}, {14, 2}, {-1, 0}},   /* 8ths */
    {{0, 3}, {3, 1}, {4, 3}, {7, 1}, {8, 3}, {11, 1}, {12, 4}, {-1, 0}},             /* dotted */
    {{0, 2}, {2, 2}, {6, 2}, {8, 2}, {10, 2}, {14, 2}, {-1, 0}},                     /* gaps */
    {{0, 4}, {4, 2}, {6, 2}, {8, 4}, {12, 1}, {13, 1}, {14, 2}, {-1, 0}},            /* long-short */
    {{0, 3}, {3, 3}, {6, 2}, {8, 3}, {11, 3}, {14, 2}, {-1, 0}},                     /* 3+3+2 */
    {{2, 2}, {4, 2}, {6, 3}, {10, 2}, {12, 2}, {14, 2}, {-1, 0}}};                   /* off the one */
static const int8_t PAT[6][6] = {{0, 1, 2, 3, -1}, {3, 2, 1, 0, -1}, {0, 1, 2, 3, 2, 1}, {0, 2, 1, 3, -1},
                                 {0, 3, 1, 3, -1}, {3, 0, 2, 1, -1}};

static void choose(lead_state_t *ls, const song_roll_t *r)   /* one pattern, rhythm, direction and swap per song */
{
    const lib_pattern_t *dp = lib_pattern_by_id((uint8_t)r->v[RF_PATTERN]);
    unsigned snare = dp ? (unsigned)dp->hit[D_SNARE] : 0x1010u;
    int i, j, best = 1 << 30, tie;
    rng_t g;
    rng_seed(&g, r->seed, 142);
    ls->pat = rng_int(&g, 6);
    ls->rev = rng_int(&g, 3) == 0;
    ls->swap = rng_int(&g, 2) ? rng_int(&g, 5) : -1;    /* two neighboring notes swapped, or none */
    tie = rng_int(&g, 1000);
    for (i = 0; i < 6; i++) {                            /* share of notes on a snare hit, in per mille */
        int on = 0, all = 0, cost;
        for (j = 0; RHY[i][j][0] >= 0; j++) { all++; on += (snare >> RHY[i][j][0]) & 1; }
        cost = on * 1000 / all * 4 + (tie + i * 377) % 1000;
        if (cost < best) { best = cost; ls->rhy = i; }
    }
    ls->chosen = 1;
}

static uint32_t note_hash(uint32_t seed, int bar, int i)
{
    uint32_t h = seed * 0x9E3779B1u ^ (uint32_t)(bar + 1) * 0x85EBCA6Bu ^ (uint32_t)(i + 1) * 0xC2B2AE35u;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 13;
    return h;
}

/* the arpeggio keeps each note with 40% + 0.75% x motion: 55% at motion 20, all of them at 80; the motion lever (a
   request) reaches it at the next bar. Skeleton landings are placed after, so they always sound. */
static void plan_arp(lead_state_t *ls, const ctx_t *c, int bar)
{
    int i, k, plen, keep = 40 + c->r->v[RF_MOTION] * 3 / 4;
    if (!ls->chosen) choose(ls, c->r);
    for (plen = 0; plen < 6 && PAT[ls->pat][plen] >= 0; plen++)
        ;
    for (i = 0; RHY[ls->rhy][i][0] >= 0; i++) {
        int at = RHY[ls->rhy][i][0], pos = (bar * 8 + i) % plen, idx, v;
        if (ls->swap >= 0 && ls->swap % (plen - 1) == pos) pos++;
        else if (ls->swap >= 0 && ls->swap % (plen - 1) + 1 == pos) pos--;
        idx = PAT[ls->pat][pos];
        if ((bar & 1) && ls->rev) idx = 3 - idx;         /* the direction turned every other bar */
        if (bar % 4 == 3 && (i == 2 || i == 5)) continue;   /* the phrase's last bar drops two notes */
        if ((int)(note_hash(c->r->seed, bar, i) % 100u) >= keep) continue;   /* motion thins it (Sam) */
        v = c->keys[idx];
        for (k = 1; k < 4 && v && !legal(c->r, v, c->pcs, c->np, c->bass); k++)
            v = c->keys[(idx + k) & 3];                  /* a tone that would rub gives way to the next */
        if (v && legal(c->r, v, c->pcs, c->np, c->bass)) {
            int len = RHY[ls->rhy][i][1], note = place(v % 12, at + len > 16 ? c->lo_x : c->lo, c->hi, v + 12 - c->shift);
            put(ls, at, note, len);
        }
    }
}

/* how a chord change lands on the skeleton note: on 1 for an 8th, on 1 for a quarter, an 8th late, a 16th late,
   pushed an 8th early, or a beat's rest then on 2 (Sam: not always a quarter on 1; 1 can rest) */
static int landing(uint32_t seed, int bar)
{
    static const uint8_t W[6] = {15, 15, 20, 10, 20, 20};
    uint32_t h = seed * 0x9E3779B1u ^ (uint32_t)(bar + 1) * 0x85EBCA6Bu;
    int x, i;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 13;
    x = (int)(h % 100u);
    for (i = 0; i < 5 && x >= W[i]; i++) x -= W[i];
    return i;
}

static void plan_hybrid(lead_state_t *ls, const ctx_t *c, int bar, int bpc, const int *npcs, int nn)
{
    int pushed = ls->tied;
    ls->tied = 0;
    plan_arp(ls, c, bar);
    if (bar % bpc == 0) {
        int how = landing(c->r->seed, bar);
        if (how == 4 && pushed) {
            land(ls, 0, 0, 2);                           /* it already sounds from the last bar: let it ring */
        } else {
            static const int8_t AT[6] = {0, 0, 2, 3, 2, 4}, LEN[6] = {2, 4, 4, 3, 4, 4};
            int note = skel(c, c->pcs, c->np, ls->last ? ls->last : 74);
            ls->last = note;
            if (AT[how]) land(ls, 0, 0, AT[how]);        /* beat 1 rests until the landing */
            land(ls, AT[how], note, LEN[how]);
        }
    }
    if ((bar + 1) % bpc == 0 && nn > 0 && landing(c->r->seed, bar + 1) == 4) {
        int nxt = skel_from(c, npcs, nn, ls->last, c->lo_x);
        if (push_ok(c, nxt)) {
            ls->last = nxt;
            land(ls, 14, nxt, 4);                        /* the next chord's note an 8th early, across the bar */
            ls->tied = 1;
        }
    }
}

int lead_plays(const plan_block_t *b)
{
    return b && b->section != SEC_INTRO && b->section != SEC_OUTRO && (b->layers & L_KEYS) && (b->layers & L_LEAD);
}

int lead_plan_bar(lead_state_t *ls, const song_roll_t *r, const plan_block_t *b, int bar, const int keys[4],
                  const int nkeys[4], const int *pcs, int np)
{
    const lib_prog_t *pr = lib_prog_by_id((uint8_t)r->v[RF_PROG]);
    const lib_set_t *set = lib_set_by_id((uint8_t)r->v[RF_SET]);
    int bpc = pr && pr->bars_per_chord ? pr->bars_per_chord : 1, loop = pr && pr->n ? pr->n * bpc : 4;
    int i, n = 0, npcs[5], nn;
    ctx_t c;
    for (i = 0; i < 16; i++) ls->on[i] = 0;
    ls->acc = 0;
    ls->active = ls->tied;
    ls->onsets = 0;
    if (!lead_plays(b) || np <= 0) { ls->tied = 0; return 0; }
    c.r = r; c.keys = keys; c.pcs = pcs; c.np = np; c.bass = 0;
    if (set && lib_ins_sustained(set->chords)) {         /* a pad: the octave doesn't matter (Sam), its place does */
        int pl = harmony_pad_place(r);
        c.lo = c.lo_x = 60;
        c.hi = pl == PL_ABOVE ? 72 : 83;
        c.shift = pl == PL_BELOW ? 0 : 12;
    } else {                                             /* percussive keys: a 4th or more over their top */
        int top = 0, ntop = 0;
        for (i = 0; i < 4; i++) {
            if (keys[i] > top) top = keys[i];
            if (nkeys[i] > ntop) ntop = nkeys[i];
        }
        c.lo = top + 5;
        c.lo_x = (ntop > top ? ntop : top) + 5;
        c.hi = 88;
        c.shift = 0;
    }
    for (i = 0; i < 16; i++) {
        int bn = groove_bass_note(r, b, bar, i, pcs, np);
        if (bn) c.bass |= 1u << (bn % 12);
    }
    nn = harmony_chord_at(r, bar + 1, (bar + 1) / loop + 1, 0, npcs);
    if (set && lib_ins_sustained(set->lead)) plan_line(ls, &c, bar, bpc, npcs, nn);
    else plan_hybrid(ls, &c, bar, bpc, npcs, nn);
    for (i = 0; i < 16; i++) n += ls->on[i] > 0;
    ls->active |= n > 0;
    ls->onsets = n;
    return n;
}

int lead_step(const lead_state_t *ls, int step, float step_samples, song_ev_t *out, int max)
{
    float len;
    if (step < 0 || step > 15 || ls->on[step] <= 0 || max <= 0) return 0;
    len = (float)ls->len[step] * step_samples - 200.0f;
    out->voice = V_LEAD;
    out->note = (uint8_t)ls->on[step];
    out->vel = (uint8_t)((ls->acc >> step) & 1 ? 84 : 74);
    out->pad = 0;
    out->delay = 0;
    out->len = len > 1.0f ? (uint32_t)len : 1u;
    return 1;
}

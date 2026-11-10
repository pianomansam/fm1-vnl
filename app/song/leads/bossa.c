/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Bossa Nova (lead-styles design, 2026-10-29; ported unchanged from its spike but for the shared register): a soft,
   conversational line: one note repeated on the off-beats while the harmony moves under it, then a step to a long
   note struck on the "and" of 4 (anticipating the next bar) and held over the bar line, so the next chord re-colors
   it. A 4-bar phrase, twice per 8 (question, answer):
     bar 1  the motif: 2-4 repeated notes on a syncopated cell (or one from beat 1: about a third of songs, Sam
            2026-11-10), a step (up or down, per song) into the long note on
            the "and" of 4, held into bar 2 where it is legal over the next chord too
     bar 2  the held note, then a reply a step the other way, or a breath; where the held note would rub against
            the new chord it isn't held: the line moves by step onto the nearest legal note instead. Where the chord
            changes into bar 3, mostly a pickup: bar 3's repeated note struck early on the "and" of 4 and tied over
            the change (legal over both chords), so the new chord re-colors it
     bar 3  the motif again: the same cell and contour, its repeated note the same pitch where the new chord allows
            (one note re-colored), else a step down or up (a sequence), per song; about a third of the time its
            start flipped (leads_flip, Sam 2026-11-10): from beat 1 where the head began off it, else an 8th later
     bar 4  the close: a step down onto a resting note, now and then after a leap of a 6th up (gap-fill); then the
            next head's pickup on the "and" of 4 (a breath instead at the 8-bar answer's end, half the time)
   Every head restates the motif's rhythm and contour; one thing varies per phrase (an earlier start, another pitch
   level, or the step widened to a 3rd). Every note passes leads_legal() (no semitone rubs, no avoid notes); an onset
   on beat 1 is a chord tone, one on beat 3 mostly (a 9th or 13th there now and then). Velocities soft (52-66).
   With the lead loop on (leads/loop.c), Bossa records a riff (Sam 2026-11-04: the phrase looped was too sparse): the
   long note stays in its bar (no tie over), and bar 2 of each 4 answers the motif with the motif again, its long note
   a step under the last one (never on it), softer, its first note chopped where it has 3+; bar 4 closes as before (a
   step up where the range's floor leaves no step down). */
#include <stdint.h>
#include "leads.h"
#include "library.h"
#include "rng.h"
#include "override.h"

/* the repeated notes' onsets (16ths), 3 per cell; the long note is struck on 14 */
static const int8_t BN_RHY[5][3] = {{2, 6, 10}, {0, 3, 6}, {2, 4, 8}, {3, 6, 10}, {4, 6, 10}};
enum { BV_SAME, BV_SHIFT, BV_LEVEL, BV_WIDE, BV_N };
enum { BS_ONE, BS_DOWN, BS_UP };

typedef struct { int rhy, n, on[4], dir, iv, seq, leap6, flip; } bn_song_t;

static void bn_song(uint32_t seed, int motion, bn_song_t *s)
{
    static const uint8_t RW[5] = {20, 40, 15, 15, 10}, QW[3] = {60, 25, 15};
    rng_t g;
    int k;
    rng_seed(&g, seed, 220);
    s->rhy = rng_weighted(&g, RW, 5);
    s->dir = rng_int(&g, 2) ? 1 : -1;
    s->iv = 2;                                         /* a whole step (a half step almost always rubs) */
    s->seq = rng_weighted(&g, QW, 3);
    s->leap6 = 20 + rng_int(&g, 20);                   /* percent of closes */
    s->n = motion < 35 ? 2 : motion > 65 ? 4 : 3;
    for (k = 0; k < 3; k++) s->on[k] = BN_RHY[s->rhy][k];
    if (s->n == 2 && s->on[0]) { s->on[0] = s->on[1]; s->on[1] = s->on[2]; }   /* its last two; a cell on beat 1 */
    else if (s->n == 2) s->on[1] = s->on[2];                                    /* keeps it (Sam, 2026-11-10) */
    s->flip = 0;
    if (s->n == 4) s->on[3] = (s->on[2] + 14) / 2 & ~1;  /* one more, between the last and the long note */
    if (s->n == 4 && s->on[3] <= s->on[2]) s->on[3] = s->on[2] + 2;
}

/* the long note's function over a chord: the colors and the 3rd/5th first, the root least */
static int bn_fn_cost(int iv)
{
    switch (iv) {
    case 2: case 9: return -4;
    case 4: case 3: case 7: return -2;
    case 0: return 6;
    default: return 0;
    }
}

/* The repeated note R (legal, a chord tone where an onset falls on 0 or 8) and the long note L = R + dir * a step
   (iv semitones exactly in pass 0; 1-3 in pass 1, the direction kept), L legal over the next chord too when x.
   Near target, the same pitch as want (0: none) first. Returns L, *rep R. */
static int bn_fit(const leads_ctx_t *c, const leads_next_t *x, const int *on, int n, int dir, int iv, int target,
                  int want, int avoid, int force, int avl, int *rep)
{
    int R, L, d, pass, best = 0, bc = 1 << 30, strong = 0, k;
    for (k = 0; k < n; k++)                             /* beat 1 a chord tone; beat 3 mostly one */
        if (on[k] == 0) strong = 2;
        else if (on[k] == 8 && !strong) strong = 1;
    for (pass = 0; pass < 2 && !best; pass++)
        for (R = c->lo; R <= c->hi; R++) {
            if (force && R != force) continue;
            if (!leads_legal(c->r, R, c->pcs, c->np, c->bass) || (strong == 2 && !leads_is_ct(R, c->pcs, c->np)))
                continue;
            for (d = pass ? 1 : iv; d <= (pass ? 3 : iv); d++) {
                int cost;
                L = R + dir * d;
                if (L < (x ? c->lo_x : c->lo) || L > c->hi || !leads_legal(c->r, L, c->pcs, c->np, c->bass)) continue;
                if (x && !leads_legal(c->r, L, x->pcs, x->np, x->bass)) continue;
                cost = (R > target ? R - target : target - R) * 2 + bn_fn_cost((L - c->pcs[0] + 120) % 12);
                if (x) cost += bn_fn_cost((L - x->pcs[0] + 120) % 12);
                if (want && R == want) cost -= 8;
                if (avoid && R == avoid) cost += 6;    /* a sequence: not the head's pitch again */
                if (avl && L == avl) cost += 40;      /* the loop's answer: off the motif's own long note */
                if (strong == 1 && !leads_is_ct(R, c->pcs, c->np)) cost += 3;
                if (L > c->hi - 3) cost += (L - c->hi + 3) * 3;
                if (d != iv) cost += 3;
                if (c->keys[3] && L == c->keys[3] + 12) cost += 4;
                if (cost < bc) { bc = cost; best = L; *rep = R; }
            }
        }
    return best;
}

/* the statement's onsets under var; returns 0 where the shift was undone (the rhythm stays exact). A restatement with
   its start flipped (s->flip, leads_flip) then moves onto beat 1 where the head began off it, else an 8th later, the
   long note kept on 14 */
static int bn_onsets(const bn_song_t *s, int var, int *on)
{
    int k, n = s->n, d, r = 0;
    for (k = 0; k < n; k++) on[k] = s->on[k];
    if (var == BV_SHIFT) {
        d = on[0] >= 2 ? -2 : 2;                        /* an 8th earlier (later where it can't) */
        for (k = 0; k < n; k++) on[k] += d;
        r = on[n - 1] < 14;
        if (!r)
            for (k = 0; k < n; k++) on[k] -= d;
    }
    if (s->flip) {
        d = on[0] ? -on[0] : 2;
        if (on[n - 1] + d < 14)
            for (k = 0; k < n; k++) on[k] += d;
    }
    return r;
}

static int bn_var(uint32_t seed, int run, int pass, int half)
{
    static const uint8_t VW[BV_N] = {55, 15, 20, 10};
    rng_t g;
    rng_seed(&g, seed ^ ((uint32_t)run * 0x27D4EB2Fu) ^ ((uint32_t)(pass * 2 + half) * 0x165667B1u), 221);
    if (!run && !pass && !half) return BV_SAME;        /* the first statement is the motif itself */
    return rng_weighted(&g, VW, BV_N);
}

/* one statement of the motif; returns the long note (0 none). force: the repeated note, struck early as a pickup on
   the last bar's "and" of 4 (0 none) */
static int bn_state(lead_state_t *ls, const leads_ctx_t *c, const leads_next_t *nx, const bn_song_t *s, int var,
                    int target, int want, int avoid, int force, int from, int avl, int *rep)
{
    int on[4], k, n = s->n, R = 0, L = 0, iv = s->iv, end, held = 0, f;
    const leads_next_t *x = c->can_push && nx->np > 0 ? nx : 0;
    bn_onsets(s, var, on);
    if (var == BV_WIDE) iv = 3 + (s->iv == 2);          /* the step widened to a 3rd */
    for (f = force ? 0 : 1; f < 2 && !L; f++) {       /* the pickup's note first */
        int fo = f ? 0 : force;
        if (x) {
            L = bn_fit(c, x, on, n, s->dir, iv, target, want, avoid, fo, avl, &R);
            if (L) held = 1;
        }
        if (!held) L = bn_fit(c, 0, on, n, s->dir, iv, target, want, avoid, fo, avl, &R);
    }
    if (!L) return 0;
    for (k = 0; k < n; k++) {
        if (on[k] < from) continue;
        leads_put(ls, c, on[k] * 3, R, k + 1 < n ? on[k + 1] : 14, 54 + 2 * k);
    }
    end = held ? 16 + 8 : 16;
    leads_put(ls, c, 14 * 3, L, end, 62);
    ls->acc |= 1u << 14;
    if (held) {
        ls->ring |= 1u << 14;
        ls->tied = 1;
        ls->tie_end = end - 16;
    }
    *rep = R;
    return L;
}

/* the nearest legal note a step (1-2 semitones) from note in direction dir (0: either, the same pitch first) */
static int bn_step(const leads_ctx_t *c, int note, int dir, int strong)
{
    int d, s;
    if (!dir && leads_ok(c, note, strong)) return note;
    for (d = 1; d <= 2; d++)
        for (s = -1; s <= 1; s += 2)
            if ((!dir || s == dir) && leads_ok(c, note + s * d, strong)) return note + s * d;
    return 0;
}

/* The repeated note the next bar's statement will sing (onsets on, target, want), over the next chord: struck as a
   pickup on this bar's "and" of 4 and tied into the next bar to its first onset, so the new chord re-colors it
   (where it is legal over both). Returns 1 when placed. */
static int bn_pickup(lead_state_t *ls, const leads_ctx_t *c, const leads_next_t *nx, const bn_song_t *s,
                     const int *on, int target, int want, int avoid, int from)
{
    leads_ctx_t cn = *c;
    int R = 0, end;
    if (!c->can_push || nx->np <= 0 || from > 12) return 0;
    cn.pcs = nx->pcs; cn.np = nx->np; cn.bass = nx->bass; cn.lo = c->lo_x; cn.can_push = 0;
    if (!bn_fit(&cn, 0, on, s->n, s->dir, s->iv, target, want, avoid, 0, 0, &R) || R < c->lo_x ||
        !leads_legal(c->r, R, c->pcs, c->np, c->bass) || !leads_legal(c->r, R, nx->pcs, nx->np, nx->bass))
        return 0;
    end = 16 + (on[0] >= 2 ? on[0] : 0);
    leads_put(ls, c, 14 * 3, R, end, 56);
    if (end > 16) { ls->ring |= 1u << 14; ls->tied = 1; ls->tie_end = end - 16; }
    ls->akey = R;
    return 1;
}

void bossa_plan(lead_state_t *ls, const leads_ctx_t *c, const plan_block_t *b, int bar, const leads_next_t *nx)
{
    int run, pass, bip, pbars, last, role, half, i, center, prev = ls->last, var, anchor;
    int from = ls->tied ? ls->tie_end : 0, rep, pick, on[4], seq, k, lp = leads_loop_plays(LS_BOSSA);
    leads_next_t nx0 = *nx;
    const leads_next_t *nh = lp ? &nx0 : nx;           /* the loop: the long note stays in its bar */
    uint32_t seed = song_ov_phrase_seed(c->r->seed);
    bn_song_t s;
    rng_t g;
    ls->tied = 0;
    ls->tie_end = 0;
    leads_phrase_at(b, bar, &run, &pass, &bip, &pbars, &last);
    role = bip % 4;
    half = bip / 4;
    if (pbars < 8 && last) half = 1;
    nx0.np = 0;
    bn_song(seed, c->r->v[RF_MOTION], &s);
    rng_seed(&g, seed ^ ((uint32_t)run * 0x27D4EB2Fu) ^ ((uint32_t)(pass * 2 + half) * 0x165667B1u) ^ (uint32_t)bar,
             222);
    var = bn_var(seed, run, pass, half);
    seq = s.seq;                                        /* one note re-colored only where the chord moves under it */
    if (seq == BS_ONE && ls->ctrun && (role == 1 ? nx->np > 0 && nx->pcs[0] % 12 == ls->ctrun - 1
                                                 : c->pcs[0] % 12 == ls->ctrun - 1))
        seq = BS_DOWN;
    center = c->hi - 8 < 74 ? c->hi - 8 : 74;           /* the middle of the range: a soft voice */
    if (center < c->lo + 5) center = c->lo + 5;
    anchor = center;                                    /* home: the range's middle (the cost picks the note) */
    if (bip == 0 && !ls->akey) prev = 0;
    if (bip == 0) ls->anchor = 0;
    pick = ls->akey;                                    /* a pickup struck in the last bar (0 none) */
    ls->akey = 0;
    if (lp && role == 1) {
        /* the loop's riff (only its recording plans here): the motif again, its long note a step under the last
           (never on it), softer, its first note chopped (3+ notes) */
        int t = prev ? prev - 2 - s.dir * s.iv : anchor;
        i = bn_state(ls, c, nh, &s, BV_SAME, t, 0, 0, 0, s.n >= 3 ? s.on[0] + 1 : 0, prev, &rep);
        for (k = 0; k < 16; k++)
            if (ls->on[k] > 0) ls->vel[k] = (uint8_t)(ls->vel[k] - 8);
        if (i) prev = i;
        ls->last = prev;
        return;
    }
    switch (role) {
    case 0: { /* the motif */
        int t = anchor + (pass & 1 ? 3 : 0) + (half ? (anchor - 4 >= c->lo + 4 ? -4 : 5) : 0);   /* the answer a 3rd
                                                                   lower (a 4th up off the floor); next pass higher */
        if (var == BV_LEVEL) t += rng_int(&g, 2) ? 3 : -3;
        i = bn_state(ls, c, nh, &s, var, t, 0, 0, pick, from, 0, &rep);
        if (i) { prev = i; ls->anchor = rep; ls->ctrun = c->pcs[0] % 12 + 1; }
        break;
    }
    case 2: { /* the motif again: one note re-colored, or a step along the scale */
        int head = ls->anchor ? ls->anchor : anchor, want = 0, t = head;
        bn_song_t s2 = s;
        s2.flip = leads_flip(seed, run, pass, bip);
        if (seq == BS_ONE) want = head;
        else t = head + (seq == BS_DOWN ? -2 : 2);
        i = bn_state(ls, c, nh, &s2, var == BV_LEVEL ? BV_SAME : var, t, want, want ? 0 : head, pick, from, 0, &rep);
        if (i) prev = i;
        break;
    }
    case 1: { /* the held note, then a reply a step the other way; or, not held, the step that replaces the tie; then,
                 where the chord changes into bar 3, the pickup into it (the one note re-colored) */
        int head = ls->anchor ? ls->anchor : anchor, chg = nx->np > 0 && nx->pcs[0] % 12 != c->pcs[0] % 12, pk = 0;
        if (!prev) break;
        if (!from) {                                    /* not held: move by step to a legal note, the "and" of 1 */
            int n = bn_step(c, prev, 0, 0);
            if (n && n != prev) {
                leads_put(ls, c, 6, n, 10, 58);
                prev = n;
            } else if (n) {
                leads_put(ls, c, 6, n, 10, 56);         /* the same note, struck again over the new chord */
            }
        }
        if (chg && rng_int(&g, 10) < 8) {
            bn_song_t s2 = s;
            s2.flip = leads_flip(seed, run, pass, bip + 1);   /* bar 3's start, as it will sing it */
            bn_onsets(&s2, var == BV_LEVEL ? BV_SAME : var, on);
            pk = bn_pickup(ls, c, nx, &s2, on, seq == BS_ONE ? head : head + (seq == BS_DOWN ? -2 : 2),
                           seq == BS_ONE ? head : 0, seq == BS_ONE ? 0 : head, from);
        }
        if (!pk && rng_int(&g, 10) < 6 && from <= 10) {
            int n = bn_step(c, prev, -s.dir, 0);
            if (n) { leads_put(ls, c, 36, n, 15, 56); prev = n; }
        }
        break;
    }
    default: { /* the close: a step down onto a rest note, now and then after a 6th up */
        int t0 = from ? from + 2 : 2, a = 0, rest, k;
        if (!prev || t0 > 10) break;
        if (rng_int(&g, 100) < s.leap6)
            for (k = 9; k >= 8 && !a; k--) if (leads_ok(c, prev + k, 0)) a = prev + k;   /* a major or minor 6th */
        if (a) {                                        /* the leap, then the step down that fills it: the rest */
            rest = bn_step(c, a, -1, 0);
            leads_put(ls, c, t0 * 3, a, t0 + 2, 60);
            if (!rest) { prev = a; break; }
            t0 += 2;
        } else {
            rest = 0;
            for (k = 1; k <= 3 && !rest; k++) {         /* down by step; the answer rests on a chord tone */
                int q = bn_step(c, prev - k + 1, -1, 0);
                if (q && q < prev && (!half || leads_is_ct(q, c->pcs, c->np))) rest = q;
            }
            if (!rest && lp) rest = bn_step(c, prev, 1, 0);   /* the loop: at the range's floor, a step up instead */
            if (!rest) break;
            if (t0 == 8 && !leads_is_ct(rest, c->pcs, c->np)) t0 = 10;
        }
        leads_put(ls, c, t0 * 3, rest, t0 + 6 < 14 ? t0 + 6 : 14, 52);
        prev = rest;
        if (bip == 7 && rng_int(&g, 2)) break;          /* the answer's end: a breath, half the time */
        if (bar + 1 < b->start + b->bars || ((b + 1)->bars && lead_plays(b + 1))) {
            int np2 = bip == 7 ? pass + 1 : pass, nh = bip == 7 ? 0 : 1, nv = bn_var(seed, run, np2, nh);
            int t = anchor + (np2 & 1 ? 3 : 0);
            rng_t g2;
            if (nh) t += anchor - 4 >= c->lo + 4 ? -4 : 5;
            if (nv == BV_LEVEL) {                      /* the next head's own level, from its own stream */
                rng_seed(&g2, seed ^ ((uint32_t)run * 0x27D4EB2Fu) ^ ((uint32_t)(np2 * 2 + nh) * 0x165667B1u) ^
                         (uint32_t)(bar + 1), 222);
                t += rng_int(&g2, 2) ? 3 : -3;
            }
            bn_onsets(&s, nv, on);
            bn_pickup(ls, c, nx, &s, on, t, 0, 0, t0 + 2);
        }
        break;
    }
    }
    ls->last = prev;
}

/* The song's motif: its statement as the song draws it (before the legal notes and the range bend it): 2-4 repeated
   notes on the song's syncopated cell (each to the next onset), then the long note struck on the "and" of 4, a step
   up or down (a whole step: one step of the mode's scale), held over the bar line to the next bar's beat 3. */
void bossa_motif(const song_roll_t *r, lead_motif_t *m)
{
    bn_song_t s;
    int at[5], len[5], si[5], k;
    bn_song(song_ov_phrase_seed(r->seed), r->v[RF_MOTION], &s);
    for (k = 0; k < s.n; k++) {
        at[k] = s.on[k] - s.on[0];
        len[k] = (k + 1 < s.n ? s.on[k + 1] : 14) - s.on[k];
        si[k] = 0;
    }
    at[s.n] = 14 - s.on[0];
    len[s.n] = 10;
    si[s.n] = s.dir;
    lead_motif_from(m, s.n + 1, at, len, si);
    m->scale = LM_MODE;
}

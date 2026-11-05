/* SPDX-License-Identifier: GPL-3.0-or-later */
/* '60s Jazz (lead-styles design, 2026-10-29; ported unchanged from its spike but for the shared register): a 4-bar
   unit, twice per 8 (question, answer), of two statements of one song motif:
     bars 1-2  the motif: 2-4 8th-note onsets starting off the beat, running over the bar line into a target on the
               next chord (its 3rd, 7th, 9th or 13th, the root least) on 1, the "and" of 1 or beat 2; then space
     bars 3-4  the motif again, sequenced (same contour in legal scale steps, re-anchored on bar 4's chord, its
               target a step from the last one: a guide-tone line) and often displaced (an 8th or a beat later or
               earlier); the answer's last target rests lower, on a chord tone
   Every note passes leads_legal() over the chord it sounds in (and the next one when it rings over the bar line);
   onsets on steps 0 and 8 are chord tones. No chromatic approach notes: they rub. A target the range leaves no room
   to run into (near its floor under a rise, near its top under a fall) moves away from that edge, so the statement
   keeps its run-in. */
#include <stdint.h>
#include "leads.h"
#include "library.h"
#include "rng.h"
#include "override.h"

enum { JR_N = 5, JC_N = 4 };
static const int8_t RHY[JR_N][5] = {    /* onsets in 16ths from the statement's bar (target last, over the bar line) */
    {4, 6, 8, 12, 16},      /* 2, &2, 3, 4 -> 1 */
    {6, 8, 10, 14, 18},     /* &2, 3, &3, &4 -> &1 (the late arrival) */
    {2, 6, 10, 14, 18},     /* off-beat quarters: &1 &2 &3 &4 -> &1 */
    {2, 8, 14, 20, -1},     /* dotted quarters: 3 against 4 (the hemiola) -> 2 */
    {10, 12, 14, 16, 20}};  /* &3, 4, &4, 1 -> 2: a late start */
static const int8_t CON[JC_N][4] = {    /* the notes before the target, in legal scale steps from it */
    {+4, +3, +2, +1}, {+1, +3, +2, +1}, {-4, -3, -2, -1}, {+2, +3, +1, -1}};   /* fall, turn, rise, enclosure */

typedef struct { int rhy, con, n, trip; } jz_song_t;

static void jz_song(uint32_t seed, int motion, jz_song_t *s)
{
    static const uint8_t RW[JR_N] = {25, 25, 15, 15, 20}, CW[JC_N] = {35, 25, 20, 20};
    rng_t g;
    rng_seed(&g, seed, 210);
    s->rhy = rng_weighted(&g, RW, JR_N);
    s->con = rng_weighted(&g, CW, JC_N);
    s->n = motion < 40 ? 3 : motion > 65 ? 4 : 3 + rng_int(&g, 2);   /* notes before the target */
    s->trip = motion >= 50 && rng_int(&g, 100) < 40;   /* the last two before the target as an 8th triplet */
}

/* the target: legal over its chord, a chord tone on 0 and 8; the guide tones (3rd, 7th) and colors (9th, 13th, the
   11th over minor) first, the root least; near `near`, in [lo, hi] */
static int jz_target(const leads_ctx_t *c, const int *pcs, int np, unsigned bass, int strong, int near, int rest,
                     int lo, int hi, unsigned amask, int adir)   /* the approach: its legal notes, the side it
                                                                    comes from */
{
    int note, best = 0, bc = 1 << 30, j, minor = 0;
    for (j = 0; j < np; j++) minor |= (pcs[j] - pcs[0] + 12) % 12 == 3;
    for (note = lo; note <= hi; note++) {
        int iv = (note - pcs[0] + 120) % 12, ct = leads_is_ct(note, pcs, np), cost;
        if (!leads_legal(c->r, note, pcs, np, bass) || (strong && !ct)) continue;
        cost = (note > near ? note - near : near - note) * 3;
        if (iv == 0) cost += rest ? 0 : 8;
        if (iv == 7) cost += 3;
        if (iv == 3 || iv == 4 || iv == 10 || iv == 11) cost -= rest ? 1 : 3;
        if (!rest && (iv == 2 || iv == 9 || (minor && iv == 5))) cost -= 3;
        if (rest && !ct) cost += 10;
        if (amask) {                             /* the approach note a step away, not a leap over rubs */
            int a = leads_coll_step(amask, note, adir), gap = a > note ? a - note : note - a;
            if (gap > 3) cost += 4 * (gap - 3);
        }
        if (c->keys[3] && note == c->keys[3] + 12) cost += 4;
        if (cost < bc) { bc = cost; best = note; }
    }
    return best;
}

static int jz_vel(const leads_ctx_t *c, int v, int note)
{
    const lib_set_t *set = lib_set_by_id((uint8_t)c->r->v[RF_SET]);
    if (set && !c->vibes && !lib_ins_sustained(set->lead)) v += 16;   /* a struck lead (the felt piano): the
                                                                         singing top voice, over the keys */
    if (c->vibes) v = 88 + (v - 50) / 3;
    else if (!c->pad && note < 76) v += (76 - note) / 3;
    return v < 25 ? 25 : v > 110 ? 110 : v;
}

/* one note at p (thirds of a 16th from this bar), to end (16ths): this bar's, or carried into the next */
static void jz_note(lead_state_t *ls, const leads_ctx_t *c, int bar, int p, int note, int end, int v)
{
    int t = p / 3;
    if (note <= 0) return;
    if (t < 16) {
        if (end <= t) return;
        ls->on[t] = note;
        ls->len[t] = end - t;
        ls->dl[t] = (uint8_t)(p % 3);
        ls->vel[t] = (uint8_t)jz_vel(c, v, note);
        if (end > 16) ls->ring |= 1u << t;
    } else if (ls->st.jz.cn < 8) {
        int k = ls->st.jz.cn++;
        ls->st.jz.bar = bar + 1;
        ls->st.jz.p[k] = p - 48;
        ls->st.jz.note[k] = note;
        ls->st.jz.end[k] = end - 16;
        ls->st.jz.vel[k] = jz_vel(c, v, note);
    }
}

/* the run-in to tgt: the contour's last n steps backward from it, onto the rhythm's onsets before it (notes, their
   onsets in thirds of a 16th, from the target back); its count. *out: -1 or 1 where a step fell under the range's
   floor or over its top and cut the run-in there, else 0 */
static int jz_runin(const leads_ctx_t *c, const leads_next_t *x, const int *pos, int m, int con, int n, int tgt,
                    unsigned lg0, unsigned lg1, int *notes, int *ps, int *out)
{
    int k, cnt = 0;
    *out = 0;
    for (k = 0; k < n && k < m - 1; k++) {
        int i = m - 2 - k, at = pos[i], note, tries, nt = at >= 16;
        unsigned lg = nt ? lg1 : lg0;
        const int *pp = nt ? x->pcs : c->pcs;
        int np = nt ? x->np : c->np;
        if (at < 0 || !lg) break;
        note = leads_coll_step(lg, tgt, CON[con][3 - k]);
        for (tries = 0; tries < 2 && at % 8 == 0 && !leads_is_ct(note, pp, np); tries++) {
            int up = leads_coll_step(lg, note, 1), dn = leads_coll_step(lg, note, -1);   /* a strong onset: a chord
                                                                                            tone a legal step away,
                                                                                            or a rest */
            if (leads_is_ct(up, pp, np) && up - note <= 3) note = up;
            else if (leads_is_ct(dn, pp, np) && note - dn <= 3) note = dn;
            else break;
        }
        if (note < (nt ? c->lo_x : c->lo) || note > c->hi) {
            *out = note > c->hi ? 1 : -1;
            break;
        }
        if (at % 8 == 0 && !leads_is_ct(note, pp, np)) continue;
        {
            int after = cnt ? notes[cnt - 1] : tgt;      /* no repeated note; a gap past a 5th: it starts after */
            if (note == after) continue;
            if (note - after > 7 || after - note > 7) break;
        }
        notes[cnt] = note; ps[cnt] = at * 3; cnt++;
    }
    return cnt;
}

/* one statement from this bar: shift in 16ths (the displacement), last: the answer's close (a lower chord tone) */
static void jz_statement(lead_state_t *ls, const leads_ctx_t *c, const leads_next_t *x, int bar, const jz_song_t *s,
                         int rhy, int con, int n, int shift, int near, int last, int v0)
{
    int pos[6], m = 0, k, tp, tgt, strong, notes[6], ps[6], cnt, out;
    unsigned lg0 = leads_legal_pcs(c->r, c->pcs, c->np, c->bass);
    unsigned lg1 = x ? leads_legal_pcs(c->r, x->pcs, x->np, x->bass) : 0;
    for (k = 0; k < 5 && RHY[rhy][k] >= 0; k++) pos[m++] = RHY[rhy][k] + shift;
    tp = pos[m - 1];
    if (!x && tp >= 16) {                        /* no next chord known: the statement closes inside this bar */
        int d = tp - 14;
        for (k = 0; k < m; k++) pos[k] -= d;
        tp = pos[m - 1];
    }
    if (tp >= 32) return;
    strong = tp % 8 == 0;
    {
        int adir = CON[con][3] > 0 ? 1 : -1, lo = tp >= 16 ? c->lo_x : c->lo, hi = c->hi, tries, t2 = 0;
        unsigned am = m >= 2 && n > 0 ? (pos[m - 2] >= 16 ? lg1 : lg0) : 0;
        const int *tpcs = tp >= 16 ? x->pcs : c->pcs;
        int tnp = tp >= 16 ? x->np : c->np;
        unsigned tbass = tp >= 16 ? x->bass : c->bass;
        tgt = jz_target(c, tpcs, tnp, tbass, strong, near, last, lo, hi, am, adir);
        if (!tgt) return;
        cnt = jz_runin(c, x, pos, m, con, n, tgt, lg0, lg1, notes, ps, &out);
        /* a run-in the range cuts (a rise into a target near the floor, a fall into one near the top): the target
           re-placed, further from that edge, until the whole run-in fits; the fullest run-in kept */
        for (tries = 0; tries < 6 && out; tries++) {
            int n2[6], p2[6], c2, o2;
            if (out < 0) lo = (t2 ? t2 : tgt) + 1;
            else hi = (t2 ? t2 : tgt) - 1;
            t2 = lo <= hi ? jz_target(c, tpcs, tnp, tbass, strong, near, last, lo, hi, am, adir) : 0;
            if (!t2) break;
            c2 = jz_runin(c, x, pos, m, con, n, t2, lg0, lg1, n2, p2, &o2);
            if (c2 > cnt) {
                tgt = t2;
                cnt = c2;
                for (k = 0; k < c2; k++) { notes[k] = n2[k]; ps[k] = p2[k]; }
            }
            out = o2;
        }
    }
    if (s->trip && cnt >= 2 && tp - pos[m - 3] >= 4) {   /* compressed into a triplet that starts with a rest */
        ps[0] = tp * 3 - 4;
        ps[1] = tp * 3 - 8;
    }
    for (k = cnt - 1; k >= 0; k--) {             /* legato to the next onset; over the bar line only if legal there */
        int p = ps[k], nxt = k ? ps[k - 1] : tp * 3, end = (nxt + 2) / 3;
        if (p < 48 && end > 16 && !(x && leads_legal(c->r, notes[k], x->pcs, x->np, x->bass) && notes[k] >= c->lo_x))
            end = 16;
        jz_note(ls, c, bar, p, notes[k], end, v0 + 4 * (cnt - 1 - k));
    }
    {
        int hold = last ? 10 : 6 + (tp % 16 == 0 ? 2 : 0), end = tp + hold, cap = tp >= 16 ? 32 : 16;
        jz_note(ls, c, bar, tp * 3, tgt, end > cap ? cap : end, last ? 70 : 84);
    }
    ls->st.jz.tgt = tgt;
}

void jazz60s_plan(lead_state_t *ls, const leads_ctx_t *c, const plan_block_t *b, int bar, const leads_next_t *nx)
{
    int run, pass, bip, pbars, last, role, half, i, key = c->r->v[RF_KEY], center, anchor;
    uint32_t seed = song_ov_phrase_seed(c->r->seed);
    jz_song_t s;
    const leads_next_t *x = c->can_push && nx->np > 0 ? nx : 0;
    ls->tied = 0;
    if (ls->st.jz.cn && ls->st.jz.bar == bar) {  /* the notes carried over the bar line: still legal here? */
        for (i = 0; i < ls->st.jz.cn; i++) {
            int t = ls->st.jz.p[i] / 3;
            if (t < 0 || t > 15 || !leads_legal(c->r, ls->st.jz.note[i], c->pcs, c->np, c->bass)) continue;
            if (t % 8 == 0 && !leads_is_ct(ls->st.jz.note[i], c->pcs, c->np)) continue;
            ls->on[t] = ls->st.jz.note[i];
            ls->len[t] = ls->st.jz.end[i] > 16 ? 16 - t : ls->st.jz.end[i] - t > 0 ? ls->st.jz.end[i] - t : 1;
            ls->dl[t] = (uint8_t)(ls->st.jz.p[i] % 3);
            ls->vel[t] = (uint8_t)ls->st.jz.vel[i];
        }
    }
    ls->st.jz.cn = 0;
    leads_phrase_at(b, bar, &run, &pass, &bip, &pbars, &last);
    role = bip % 4;
    half = bip / 4;
    if (pbars < 8 && last) half = 1;
    jz_song(seed, c->r->v[RF_MOTION], &s);
    center = c->hi - 7 < 76 ? c->hi - 7 : 76;
    if (center < c->lo + 5) center = c->lo + 5;
    anchor = leads_place(key % 12, center - 6, center + 5, center);
    if (!anchor) anchor = center;
    if (bip == 0 && pass == 0) ls->st.jz.tgt = 0;
    if (role == 0 || role == 2) {
        static const uint8_t DW[4] = {35, 30, 15, 20};
        static const int8_t DS[4] = {0, 2, -2, 4};
        rng_t g;
        int shift = 0, rhy = s.rhy, con = s.con, n = s.n, near, closing = role == 2 && half, lastpos;
        rng_seed(&g, seed ^ ((uint32_t)run * 0x27D4EB2Fu) ^ ((uint32_t)(pass * 4 + bip + 1) * 0x165667B1u), 211);
        if (role == 2 || half || pass) shift = DS[rng_weighted(&g, DW, 4)];   /* restatements: displaced */
        if (role == 0 && (half || pass) && rng_int(&g, 100) < 30) n = n + 1 > 4 ? 4 : n + 1;   /* extended */
        if ((role == 2 || half || pass) && rng_int(&g, 100) < 25) con = (con + 1) % JC_N;     /* re-contoured */
        lastpos = RHY[rhy][4] >= 0 ? RHY[rhy][4] : RHY[rhy][3];
        if (RHY[rhy][0] + shift < 0 || lastpos + shift >= 32) shift = 0;
        near = ls->st.jz.tgt ? ls->st.jz.tgt + (role == 2 ? (half ? -2 : 1) : 0) : anchor;
        if (closing) near = anchor - 3;
        jz_statement(ls, c, x, bar, &s, rhy, con, n, shift, near, closing, 64);
    }
    for (i = 15; i >= 0 && !ls->on[i]; i--) {}
    if (i >= 0) ls->last = ls->on[i];
}

/* The song's motif: its first statement as the song draws it (undisplaced, before the legal notes and the range bend
   it): the rhythm's last n onsets before the target, the contour's last n steps into it (in the chord's legal notes:
   the scale's, less those that rub), the last two an 8th triplet where the song compresses them (rounded down to
   their 16th); legato, the target held 6 16ths (8 on a beat 1). */
void jazz60s_motif(const song_roll_t *r, lead_motif_t *m)
{
    jz_song_t s;
    int pos[5], at[5], len[5], si[5], mm = 0, n, k, f;
    jz_song(song_ov_phrase_seed(r->seed), r->v[RF_MOTION], &s);
    for (k = 0; k < 5 && RHY[s.rhy][k] >= 0; k++) pos[mm++] = RHY[s.rhy][k];
    n = s.n < mm - 1 ? s.n : mm - 1;
    f = mm - 1 - n;                                      /* the first note sung */
    for (k = 0; k <= n; k++) {
        at[k] = pos[f + k];
        si[k] = k < n ? CON[s.con][4 - n + k] : 0;
    }
    if (s.trip && n >= 2 && pos[mm - 1] - pos[mm - 3] >= 4) {
        at[n - 1] = pos[mm - 1] - 2;
        at[n - 2] = pos[mm - 1] - 3;
    }
    for (k = 0; k <= n; k++) len[k] = k < n ? at[k + 1] - at[k] : 6 + (pos[mm - 1] % 16 == 0 ? 2 : 0);
    for (k = n; k >= 0; k--) at[k] -= at[0];
    lead_motif_from(m, n + 1, at, len, si);
    m->scale = LM_MODE;
}

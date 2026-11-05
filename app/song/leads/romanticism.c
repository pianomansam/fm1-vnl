/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Romanticism (lead-styles design, 2026-10-29; ported unchanged from its spike but for the shared register): a 4-bar
   phrase, twice per 8 (question, answer), sung almost without a break: the long line. One spine note per bar (on 1,
   or an 8th or a quarter late when the last bar's note is held over the bar line and resolves into it) moves a
   scale step a bar in one direction (rising question, falling answer, or an arch), so the line carries the
   harmony's motion; between spine notes the bar walks by step (one rhythm cell per song, repeated: an even,
   repetitive flow) to the note next to the next spine note, and steps over the bar line into it while the chord
   changes under it. Little ornament: no runs or turns; a phrase head may leap a 4th to a 6th and fill back by step;
   bar 3 may end in an 8th-note triplet, its first note tied (a melisma); the cadence bar may open on an appoggiatura
   a step over its resting chord tone; the answer may start with an 8th's pickup (a rising 4th or a step). Every note
   passes leads_legal(). */
#include <stdint.h>
#include "leads.h"
#include "rng.h"
#include "override.h"

enum { FL_RISE, FL_FALL, FL_ARCH, FL_N };
static const int8_t SPINE[FL_N][2][4] = {{{0, 1, 2, 3}, {4, 3, 1, 0}},   /* the bar heads, in scale steps over */
                                         {{4, 3, 2, 1}, {3, 2, 1, -1}},  /* the anchor: question, answer */
                                         {{0, 2, 3, 1}, {2, 4, 2, 0}}};
enum { FC_LONG, FC_DOTTED, FC_DOT_PAIR, FC_EVEN, FC_LILT, FC_N };   /* the rhythm cells: onsets in 16ths, -1 ends */
static const int8_t CELL[FC_N][6] = {{0, 8, 12, -1}, {0, 6, 12, -1}, {0, 6, 8, 12, -1}, {0, 4, 8, 12, -1},
                                     {0, 3, 4, 8, 12, -1}};

typedef struct { int line, cell, back; } ro_song_t;
typedef struct { int tie[3], te[3], trip, leap, appog, pick, thin, ld, line, back, lift; } ro_pass_t;

static void ro_song(uint32_t seed, int motion, ro_song_t *s)
{
    static const uint8_t LW[FL_N] = {50, 30, 20}, BW[3] = {30, 50, 20};
    static const uint8_t CW[3][FC_N] = {{40, 40, 20, 0, 0}, {10, 20, 40, 30, 0}, {0, 0, 30, 35, 35}};
    rng_t g;
    rng_seed(&g, seed, 300);
    s->line = rng_weighted(&g, LW, FL_N);
    s->cell = rng_weighted(&g, CW[motion < 40 ? 0 : motion <= 60 ? 1 : 2], FC_N);
    s->back = rng_weighted(&g, BW, 3);
}

static void ro_pass(uint32_t seed, int run, int pass, int half, const ro_song_t *s, ro_pass_t *p)
{
    rng_t g;
    int i;
    rng_seed(&g, seed ^ ((uint32_t)run * 0x27D4EB2Fu) ^ ((uint32_t)(pass * 2 + half) * 0x165667B1u), 301);
    for (i = 0; i < 3; i++) {
        p->tie[i] = rng_int(&g, 100) < 35;
        p->te[i] = rng_int(&g, 2) ? 4 : 2;
    }
    p->trip = rng_int(&g, 100) < 35;
    p->leap = rng_int(&g, 100) < 40;
    p->appog = rng_int(&g, 100) < 45;
    p->pick = rng_int(&g, 100) < 50;
    p->thin = rng_int(&g, 100) < 35;
    p->ld = rng_int(&g, 2) ? 4 : 3;
    p->line = rng_int(&g, 100) < 70 ? s->line : (s->line + 1 + rng_int(&g, 2)) % FL_N;   /* little direct */
    p->back = rng_int(&g, 100) < 60 ? s->back : (s->back + 1 + rng_int(&g, 2)) % 3;      /* repetition */
    p->lift = rng_int(&g, 100) < 40 ? (rng_int(&g, 2) ? 1 : -1) : 0;
}

/* the spine note over a chord: near target, a small move from prev (the glide: a held common tone or a step); ct 1
   (struck on 1 or 3) a chord tone (the chord's own 9th, 11th or 13th included), else any legal note */
static int ro_spine(const leads_ctx_t *c, const int *pcs, int np, unsigned bass, int target, int prev, int lo, int ct)
{
    int note, best = 0, bc = 1 << 30;
    for (note = lo; note <= c->hi; note++) {
        int iv = (note - pcs[0] + 120) % 12, chord = leads_is_ct(note, pcs, np), mv = prev ? note - prev : 0, cost;
        if (mv < 0) mv = -mv;
        if (!leads_legal(c->r, note, pcs, np, bass)) continue;
        if (!chord && ct) continue;
        cost = note > target ? note - target : target - note;
        if (prev) cost += mv == 0 ? 12 : mv <= 2 ? 0 : mv <= 4 ? 2 : (mv - 4) * 4 + 4;
        if (iv == 0) cost += 3;
        if (!chord) cost += 1;
        if (c->keys[3] && note == c->keys[3] + 12) cost += 4;
        if (cost < bc) { bc = cost; best = note; }
    }
    return best;
}

void romanticism_plan(lead_state_t *ls, const leads_ctx_t *c, const plan_block_t *b, int bar,
                      const leads_next_t *nx)
{
    int run, pass, bip, pbars, last, role, half, i, k = 0, key = c->r->v[RF_KEY], center, prev = ls->last;
    int from = ls->tied ? ls->tie_end : 0, sc[7], asi, anchor, spine, t3[10], nt[10], vel[10];
    uint32_t seed = song_ov_phrase_seed(c->r->seed);
    unsigned lg = leads_legal_pcs(c->r, c->pcs, c->np, c->bass);
    ro_song_t s;
    ro_pass_t p;
    const leads_next_t *x = c->can_push && nx->np > 0 ? nx : 0;
    ls->tied = 0;
    ls->tie_end = 0;
    leads_phrase_at(b, bar, &run, &pass, &bip, &pbars, &last);
    role = bip % 4;
    half = bip / 4;
    if (pbars < 8 && last) half = 1;
    ro_song(seed, c->r->v[RF_MOTION], &s);
    ro_pass(seed, run, pass, half, &s, &p);
    leads_scale(c->r, c->pcs, c->np, sc);
    center = c->hi - 9 < 72 ? c->hi - 9 : 72;              /* low in the range: the line has a 6th over it */
    if (center < c->lo + 3) center = c->lo + 3;
    asi = leads_note_si(key, sc, center);
    anchor = leads_si_note(key, sc, asi);
    if (bip == 0 && pass == 0) prev = 0;
    {   /* the spine: the answer's head as its pickup chose it, else near its planned step */
        int tgt = leads_si_note(key, sc, asi + SPINE[p.line][half][role] + (role == 0 ? p.lift : 0));
        spine = 0;
        if (role == 0 && half && ls->st.ro.head && !from && leads_ok(c, ls->st.ro.head, 0)) spine = ls->st.ro.head;
        if (!spine && prev && role > 0 && role < 3) {     /* mid-phrase: the line steps on over the bar line */
            int dir = ls->st.ro.step_dir ? ls->st.ro.step_dir : 1, tries[4], j;   /* the move the last bar's cell
                                                                                    planned */
            tries[0] = leads_coll_step(lg, prev, dir); tries[1] = leads_coll_step(lg, prev, 2 * dir);
            tries[2] = leads_coll_step(lg, prev, -dir); tries[3] = leads_coll_step(lg, prev, -2 * dir);
            for (j = 0; j < 4 && !spine; j++) {
                int n = tries[j], d = n > prev ? n - prev : prev - n;
                if (n < c->lo || n > c->hi || d > 4 || !leads_legal(c->r, n, c->pcs, c->np, c->bass)) continue;
                if (!leads_is_ct(n, c->pcs, c->np)) continue;    /* on 1 (or resolving a tie): a chord tone */
                spine = n;
            }
        }
        if (!spine) spine = ro_spine(c, c->pcs, c->np, c->bass, tgt, role ? prev : 0, c->lo, 1);   /* a head: free */
        if (from && spine == prev) spine = 0;              /* held in: it must move */
        if (from && !spine)
            spine = leads_coll_step(lg, prev, -1) >= c->lo ? leads_coll_step(lg, prev, -1)
                                                           : leads_coll_step(lg, prev, 1);
    }
    ls->st.ro.head = 0;
    if (!spine || spine > c->hi) { ls->last = prev; return; }
    if (role == 3) {   /* the cadence: an appoggiatura a step over the resting chord tone, or the rest alone; held,
                          then a breath (and before the answer, its pickup) */
        int a = leads_coll_step(lg, spine, 1), t0 = from;
        if (p.appog && !from && a <= c->hi && leads_is_ct(a, c->pcs, c->np) && a != prev &&
            (!prev || (a - prev <= 4 && prev - a <= 4))) {
            leads_put(ls, c, 0, a, 4, 70);
            t0 = 4;
        }
        leads_put(ls, c, t0 * 3, spine, half ? 12 : 11, half ? 60 : 66);
        prev = spine;
        if (!half && x && p.pick) {                         /* the answer's pickup: a rising 4th or a step */
            unsigned nlg = leads_legal_pcs(c->r, x->pcs, x->np, x->bass);
            int h = ro_spine(c, x->pcs, x->np, x->bass, leads_si_note(key, sc, asi + SPINE[p.line][1][0]), spine,
                             c->lo_x, 1), q = 0;
            if (h) {
                if (p.leap && leads_ok(c, h - 5, 0) && h - 5 >= c->lo) q = h - 5;
                if (!q) q = leads_coll_step(lg, h, -1);
                if (q && leads_ok(c, q, 0) && q != spine && (nlg >> (h % 12)) & 1u) {
                    leads_put(ls, c, 42, q, 16, 58);
                    ls->st.ro.head = h;
                    prev = q;
                }
            }
        }
        ls->last = prev;
        return;
    }
    /* the bar's onsets (thirds of a 16th): the cell after the spine; bar 2 may drop one; bar 3 may end in a triplet */
    t3[k] = from * 3; nt[k] = spine; vel[k] = 64 + 4 * role; k++;
    {
        int m = 0, cells[6], last8 = -1;
        for (i = 1; i < 6 && CELL[s.cell][i] >= 0; i++)
            if (CELL[s.cell][i] >= from + 2) cells[m++] = CELL[s.cell][i];
        if (role == 1 && p.thin && m > 2) { cells[m - 2] = cells[m - 1]; m--; }
        if (role == 2 && p.trip) {
            for (i = 0; i < m && cells[i] <= 8; i++) last8 = i;
            m = last8 + 1;
            if (m == 0 && from <= 8) cells[m++] = 8 > from + 2 ? 8 : from + 2;
            for (i = 0; i < m; i++) t3[k + i] = cells[i] * 3;
            k += m;
            t3[k++] = 40;
            t3[k++] = 44;
        } else {
            for (i = 0; i < m; i++) t3[k + i] = cells[i] * 3;
            k += m;
        }
    }
    {   /* the walk: a sequence (chains of sequences from one cell). The bar's k moves (its k-1 steps and the step over
           the bar line into the next spine note) all go the bar's way (the line's shape: up, down, an arch) but one,
           the song's back step (first, middle, or over the bar line), so the line climbs or falls a 2nd or a 3rd a
           bar in the legal notes. A head may first leap a 4th-6th up and fill back down by step. Where the next
           legal note is a 4th or more away, or a strong 16th finds no chord tone, the last note holds. */
        int way = SPINE[p.line][half][role + 1] > SPINE[p.line][half][role] ? 1 : -1, j, nm = k, mv[10], bp;
        bp = p.back == 0 ? 0 : p.back == 1 ? nm - 1 : nm / 2;
        for (j = 0; j < nm; j++) mv[j] = nm > 1 && j == bp ? -way : way;
        for (i = 1; i < k; i++) {
            int cur = nt[i - 1], dir = mv[i - 1], note;
            int lp = leads_coll_step(lg, cur, p.ld);
            if (lp - cur > 9) lp = leads_coll_step(lg, cur, p.ld - 1);
            if (i == 1 && role == 0 && p.leap && !from && lp <= c->hi && lp - cur >= 5 && lp - cur <= 9 &&
                (t3[i] % 24 || leads_is_ct(lp, c->pcs, c->np))) {
                note = lp;
                for (j = 1; j < nm; j++) mv[j] = j == nm - 1 ? way : -1;   /* the fill comes back down */
            } else {
                note = leads_coll_step(lg, cur, dir);
                if (note > c->hi || note < c->lo) {        /* the range's edge: the rest of the bar mirrored */
                    for (j = i - 1; j < nm; j++) mv[j] = -mv[j];
                    dir = mv[i - 1];
                    note = leads_coll_step(lg, cur, dir);
                }
                if (t3[i] % 24 == 0 && !leads_is_ct(note, c->pcs, c->np)) {   /* on 3: a chord tone, a step on */
                    int alt = leads_coll_step(lg, note, dir);
                    note = leads_is_ct(alt, c->pcs, c->np) && alt - cur <= 4 && cur - alt <= 4 ? alt : 0;
                }
                if (!note || note > c->hi || note < c->lo || ((note - cur >= 5 || cur - note >= 5) &&
                                                               !(i == 1 && (note - cur == 5 || cur - note == 5)))) {
                    for (j = i; j < k - 1; j++) t3[j] = t3[j + 1];      /* no step there: the last note holds */
                    for (j = i - 1; j < nm - 1; j++) mv[j] = mv[j + 1];
                    nm--;
                    k--;
                    i--;
                    continue;
                }
            }
            nt[i] = note;
            vel[i] = 64 + 4 * role + (note - anchor) / 2;
        }
        ls->st.ro.step_dir = nm > 0 ? mv[nm - 1] : way;      /* the step over the bar line */
    }
    for (i = 0; i < k; i++) {
        int end = i + 1 < k ? (t3[i + 1] + 2) / 3 : 16;
        if (i == k - 1 && p.tie[role] && x && leads_legal(c->r, nt[i], x->pcs, x->np, x->bass) && nt[i] >= c->lo_x) {
            end = 16 + p.te[role];
            ls->ring |= 1u << (t3[i] / 3);
            ls->tied = 1;
            ls->tie_end = p.te[role];
        }
        if (i > 0 && t3[i] <= t3[i - 1]) continue;
        leads_put(ls, c, t3[i], nt[i], end, vel[i]);
    }
    ls->acc |= 1u << (from);
    ls->last = nt[k - 1];
}

/* The song's motif: its rhythm cell, the walk's steps through it (all the line's way but the song's back step), as
   the question's first bar draws them before the legal notes and the range bend them; the last note held to the
   bar's end. Every bar restates it a step or two on (the spine). */
void romanticism_motif(const song_roll_t *r, lead_motif_t *m)
{
    ro_song_t s;
    int at[6], len[6], si[6], n, j, bp, way;
    ro_song(song_ov_phrase_seed(r->seed), r->v[RF_MOTION], &s);
    way = SPINE[s.line][0][1] > SPINE[s.line][0][0] ? 1 : -1;
    for (n = 0; n < 6 && CELL[s.cell][n] >= 0; n++) at[n] = CELL[s.cell][n];
    bp = s.back == 0 ? 0 : s.back == 1 ? n - 1 : n / 2;
    for (j = 0; j < n; j++) {
        len[j] = j + 1 < n ? at[j + 1] - at[j] : 16 - at[j];
        si[j] = j ? si[j - 1] + (j - 1 == bp ? -way : way) : 0;
    }
    lead_motif_from(m, n, at, len, si);
    m->scale = LM_MODE;
}

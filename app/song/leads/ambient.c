/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Ambient (lead-styles design, 2026-10-29; ported unchanged from its spike but for the shared register): a short
   pentatonic motif (3-5 notes, 8ths at the quickest, mostly quarters, in a high calm register) restated every 2 bars
   at the SAME pitches while the chords shift under it, so each chord re-colors it (the 9th over one chord is the 5th
   over the next). A 2-bar unit, four per 8 bars:
     bar 1  the motif: mostly entering after beat 1 (the chord sounds first), legato, the last note let ring
     bar 2  the answer: the last note ringing on into it (where it is legal over the next chord), or the motif's last
            two notes echoed softer at the same pitches (an ostinato's afterimage)
   The motif's level is chosen once per song, from the progression: the pentatonic level whose notes are legal over
   the most of the loop's chords, so the re-coloring is designed, not hoped for. A note that would rub over this
   chord moves a step (direction kept where it can) or drops. Units 1-3 of each 8 bars restate the motif; unit 4
   changes ONE thing: a passing note added, a note dropped, or the last note a step lower. After the first 8 bars,
   unit 2 sometimes takes the same change. */
#include <stdint.h>
#include "leads.h"
#include "library.h"
#include "harmony.h"
#include "rng.h"
#include "override.h"

#define AM_MAX 6
enum { AS_FALL, AS_ARCH, AS_PEND, AS_SKIP, AS_N };
enum { AV_SAME, AV_ADD, AV_DROP, AV_END };
enum { AA_RING, AA_ECHO };
static const int8_t AM_SHAPE[AS_N][5] = {{0, -1, -2, -1, -3}, {0, 1, 2, 1, 0}, {0, 1, 0, -1, 0}, {0, 2, 1, 0, -1}};
typedef struct { int n, on[AM_MAX], off[AM_MAX], ans; } am_motif_t;

static void am_song(uint32_t seed, int motion, am_motif_t *m)
{
    static const uint8_t SW[4] = {20, 25, 35, 20}, GW[4] = {15, 50, 25, 10}, KW[AS_N] = {30, 20, 20, 30};
    static const int8_t START[4] = {0, 2, 4, 6}, GAP[4] = {2, 4, 6, 8};
    int k, t, tries, kind;
    rng_t g;
    rng_seed(&g, seed, 500);
    m->n = motion < 40 ? 3 + (rng_int(&g, 10) < 3) : motion <= 65 ? 4 : 4 + rng_int(&g, 2);
    for (tries = 0; tries < 8; tries++) {
        t = START[rng_weighted(&g, SW, 4)];
        for (k = 0; k < m->n; k++) {
            if (k && t == 8 && rng_int(&g, 10) < 7) t = 10;   /* mostly off beat 3: a fixed pitch there would
                                                                    have to be a chord tone of every chord */
            m->on[k] = t;
            t += tries >= 4 ? (rng_int(&g, 2) ? 2 : 4) : GAP[rng_weighted(&g, GW, 4)];
        }
        if (m->on[m->n - 1] <= 12) break;
    }
    if (m->on[m->n - 1] > 12)
        for (k = 0; k < m->n; k++) m->on[k] = 2 + 3 * k;   /* fallback: dotted 8ths from the "and" of 1 */
    kind = rng_weighted(&g, KW, AS_N);
    for (k = 0; k < m->n; k++) m->off[k] = AM_SHAPE[kind][k];
    m->ans = rng_int(&g, 100) < (motion > 55 ? 40 : 65) ? AA_RING : AA_ECHO;
}

/* the motif's first note: the pentatonic level near center whose notes are legal over the most of the loop's
   chords (a strong-beat note a chord tone) */
static int am_level(const leads_ctx_t *c, const am_motif_t *m, unsigned pent, int center)
{
    const lib_prog_t *pr = lib_prog_by_id((uint8_t)c->r->v[RF_PROG]);
    int bpc = pr && pr->bars_per_chord ? pr->bars_per_chord : 1, loop = pr && pr->n ? pr->n * bpc : 4;
    int cand, best = 0, bs = -(1 << 30), bar, k, pcs[5], np;
    for (cand = center - 7; cand <= center + 7; cand++) {
        int s = 0, mean = 0;
        if (!((pent >> (cand % 12)) & 1u)) continue;
        for (k = 0; k < m->n; k++) {
            int note = leads_coll_step(pent, cand, m->off[k]);
            if (note > c->hi || note < c->lo) s -= 1000;
            mean += note;
        }
        for (bar = 0; bar < loop; bar += bpc) {
            np = harmony_chord_at(c->r, bar, 1, 0, pcs);
            for (k = 0; k < m->n; k++) {
                int note = leads_coll_step(pent, cand, m->off[k]);
                if (leads_legal(c->r, note, pcs, np, 0) && (m->on[k] % 8 || leads_is_ct(note, pcs, np))) s += 10;
            }
        }
        mean /= m->n;
        s -= (mean > center ? mean - center : center - mean) * 4;
        if (s > bs) { bs = s; best = cand; }
    }
    return best;
}

void ambient_plan(lead_state_t *ls, const leads_ctx_t *c, const plan_block_t *b, int bar, const leads_next_t *nx)
{
    static const uint8_t VW[3] = {40, 35, 25};
    int run, pass, bip, pbars, last, k, unit, v0, var, vary1, center, from = ls->tied ? ls->tie_end : 0;
    uint32_t seed = song_ov_phrase_seed(c->r->seed);
    unsigned scale = leads_scale_mask(c->r, c->pcs, c->np), pent = leads_pent_mask(c->r, c->pcs, c->np, 0);
    const lib_set_t *set = lib_set_by_id((uint8_t)c->r->v[RF_SET]);
    const leads_next_t *x = c->can_push && nx->np > 0 ? nx : 0;
    int sustained = set && lib_ins_sustained(set->lead) && !c->vibes;
    am_motif_t m;
    rng_t g;
    ls->tied = 0;
    ls->tie_end = 0;
    leads_phrase_at(b, bar, &run, &pass, &bip, &pbars, &last);
    unit = bip / 2;
    if (pbars < 8 && last && unit < 2) unit += 2;     /* a 4-bar remainder ends on the varied unit */
    am_song(seed, c->r->v[RF_MOTION], &m);
    rng_seed(&g, seed ^ ((uint32_t)run * 0x27D4EB2Fu) ^ ((uint32_t)pass * 0x165667B1u), 501);
    var = AV_ADD + rng_weighted(&g, VW, 3);
    vary1 = pass > 0 && rng_int(&g, 100) < 25;
    center = sustained ? 77 : 81;                     /* high, never the top */
    if (center > c->hi - 7) center = c->hi - 7;
    if (center < c->lo + 5) center = c->lo + 5;
    if (!ls->st.am.lvl) ls->st.am.lvl = am_level(c, &m, pent, center);
    v0 = sustained ? 58 : 72;
    if (bip % 2 == 0) {                               /* the motif, at the song's pitches */
        int v = unit == 3 || (unit == 1 && vary1) ? var : AV_SAME, n = m.n, on[AM_MAX], tgt[AM_MAX], pl[AM_MAX];
        int add = -1, prevn = 0, lastk = -1;
        for (k = 0; k < n; k++) { on[k] = m.on[k]; tgt[k] = leads_coll_step(pent, ls->st.am.lvl, m.off[k]); }
        if (v == AV_DROP && n < 3) v = AV_END;
        if (v == AV_ADD) {                            /* a passing (or neighbor) note an 8th before a later note */
            for (k = n - 1; k > 0 && add < 0; k--)
                if (on[k] - on[k - 1] >= 4) add = k;
            if (add < 0) v = AV_END;
        }
        if (v == AV_DROP) tgt[n - 2] = 0;             /* the note before the last dropped */
        if (v == AV_END) tgt[n - 1] = leads_coll_step(pent, tgt[n - 1], -1);   /* the last note a step lower */
        for (k = 0; k < n; k++) {
            int t = tgt[k], strong = on[k] % 8 == 0;
            pl[k] = 0;
            if (!t) continue;
            if (on[k] < from) continue;
            if (leads_ok(c, t, strong)) pl[k] = t;
            else {                                    /* the nearest step that sounds (a scale or pentatonic step),
                                                         its direction from the last note and to the next kept */
                int d, best = 0, bc = 1 << 30, cand[4];
                int pdir = k ? (m.off[k] > m.off[k - 1]) - (m.off[k] < m.off[k - 1]) : 0;
                int ndir = k + 1 < n ? (m.off[k + 1] > m.off[k]) - (m.off[k + 1] < m.off[k]) : 0;
                int nt = k + 1 < n ? tgt[k + 1] : 0;
                cand[0] = leads_coll_step(scale, t, 1); cand[1] = leads_coll_step(scale, t, -1);
                cand[2] = leads_coll_step(pent, t, 1); cand[3] = leads_coll_step(pent, t, -1);
                for (d = 0; d < 4; d++) {
                    int a = cand[d], cost;
                    if (!leads_ok(c, a, strong)) continue;
                    cost = a > t ? a - t : t - a;
                    if (prevn && (a > prevn) - (a < prevn) != pdir) cost += 20;
                    if (nt && (nt > a) - (nt < a) != ndir) cost += 10;
                    if (prevn && (a > prevn ? a - prevn : prevn - a) >= 5) cost += 6;
                    if (cost < bc) { bc = cost; best = a; }
                }
                pl[k] = best;
            }
            if (pl[k]) prevn = pl[k];
        }
        for (k = 0; k < n; k++) if (pl[k]) lastk = k;
        for (k = 0; k < n; k++) {                     /* legato, each to the next onset; the last let ring */
            int nxo = 16, j;
            if (!pl[k]) continue;
            for (j = k + 1; j < n; j++) if (pl[j]) { nxo = j == add ? on[j] - 2 : on[j]; break; }
            if (k == lastk) {
                int end = 16;
                if (m.ans == AA_RING && unit != 3 && x && leads_legal(c->r, pl[k], x->pcs, x->np, x->bass) &&
                    pl[k] >= c->lo_x) {
                    end = 16 + 8;
                    ls->ring |= 1u << on[k];
                    ls->tied = 1;
                    ls->tie_end = 8;
                }
                leads_put(ls, c, on[k] * 3, pl[k], end, v0 + 2);
            } else leads_put(ls, c, on[k] * 3, pl[k], nxo, v0 + (k == 0 ? 4 : 0));
            ls->last = pl[k];
        }
        if (add > 0 && pl[add] && pl[add - 1]) {      /* the added note: passing between its neighbors, else a
                                                         neighbor from the other side */
            int a = pl[add - 1], z = pl[add];
            int p = a == z ? leads_coll_step(pent, z, 1) : leads_coll_step(scale, z, a > z ? 1 : -1);
            if (p == a || !leads_ok(c, p, 0)) p = leads_coll_step(scale, z, a > z ? -1 : 1);
            if (p != a && leads_ok(c, p, 0)) leads_put(ls, c, (on[add] - 2) * 3, p, on[add], v0 - 6);
            else ls->len[on[add - 1]] = on[add] - on[add - 1];
        }
    } else if (m.ans == AA_ECHO && unit != 3 && m.n >= 2) {   /* the last two notes again, softer, same pitches */
        int a = leads_coll_step(pent, ls->st.am.lvl, m.off[m.n - 2]);
        int z = leads_coll_step(pent, ls->st.am.lvl, m.off[m.n - 1]);
        int t1 = 12 - (m.on[m.n - 1] - m.on[m.n - 2]);
        if (t1 >= from && t1 >= 4 && leads_ok(c, a, t1 % 8 == 0) && leads_ok(c, z, 0)) {
            leads_put(ls, c, t1 * 3, a, 12, v0 - 12);
            leads_put(ls, c, 12 * 3, z, 16, v0 - 14);
            ls->last = z;
        }
    }
}

/* The song's motif: its 3-5 notes as the song draws them (before the legal notes bend them), each legato to the next
   onset, the last ringing to the bar's end, in steps of the pentatonic from the first. */
void ambient_motif(const song_roll_t *r, lead_motif_t *m)
{
    am_motif_t a;
    int at[AM_MAX], len[AM_MAX], k;
    am_song(song_ov_phrase_seed(r->seed), r->v[RF_MOTION], &a);
    for (k = 0; k < a.n; k++) {
        at[k] = a.on[k] - a.on[0];
        len[k] = (k + 1 < a.n ? a.on[k + 1] : 16) - a.on[k];
    }
    lead_motif_from(m, a.n, at, len, a.off);
    m->scale = LM_PENT;
}

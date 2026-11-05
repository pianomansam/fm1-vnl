/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Modal Jazz (lead-styles design, 2026-10-29; ported unchanged from its spike but for the shared register): a modal,
   spare lead: one motif per song (2-4 notes into a long held tone), lines from the scale over the chord (walked by
   steps, not the chord arpeggiated), lots of space. A 4-bar unit, twice per 8 (call, answer):
     bar 1  the head: the motif, starting late (after 1), landing on a held chord or color tone, tied over the bar
     bar 2  the hold, then a fall-off (a short note a step or two down) or nothing
     bar 3  the answer: the motif again with ONE thing changed (a beat later, its tail only, a note added, or its
            last interval turned), landing lower
     bar 4  silence (or the hold's fall-off; at motion 40+ sometimes the motif's tail as an echo)
   Every head restates the motif's rhythm and contour exactly, fitted to the chord by pitch level only. Middle
   register, soft, understated. Every note passes leads_legal(). */
#include <stdint.h>
#include "leads.h"
#include "library.h"
#include "rng.h"
#include "override.h"

static const int8_t CONT[6][3] = {   /* scale steps from the held tone, earliest first (the last n used) */
    {+1, +2, +1}, {0, -1, +1}, {+2, +1, -1}, {+1, +3, +1}, {-3, -2, -1}, {+4, +2, +2}};
static const int8_t GAP[5][3] = {{2, 2, 2}, {4, 2, 2}, {3, 3, 2}, {2, 4, 4}, {4, 4, 4}};
enum { AV_SAME, AV_LATE, AV_TAIL, AV_EXT, AV_TURN, AV_N };

typedef struct { int cont, gap, npre, first, la, hold, echo; } mo_song_t;

static void mo_song(uint32_t seed, int motion, mo_song_t *s)
{
    static const uint8_t FW[4] = {25, 35, 25, 15}, CW[6] = {22, 16, 16, 16, 14, 16}, GW[5] = {25, 20, 20, 15, 20};
    static const int8_t FIRST[4] = {2, 4, 6, 8};
    int tot, k;
    rng_t g;
    rng_seed(&g, seed, 300);
    s->cont = rng_weighted(&g, CW, 6);
    s->gap = rng_weighted(&g, GW, 5);
    s->npre = (motion < 40 ? 1 : 2) + rng_int(&g, 2);
    s->first = FIRST[rng_weighted(&g, FW, 4)];
    s->hold = 4 + 2 * rng_int(&g, 4);                    /* how far the held tone rings into bar 2 (16ths) */
    s->echo = rng_int(&g, 100) < (motion >= 40 ? 25 : 0);
    for (tot = 0, k = 3 - s->npre; k < 3; k++) tot += GAP[s->gap][k];
    while (s->npre > 1 && tot > 8) tot -= GAP[s->gap][3 - s->npre--];   /* the held tone from 10 at the latest */
    if (s->first + tot > 10) s->first = 10 - tot > 0 ? 10 - tot : 0;
    s->la = s->first + tot;
}

/* the motif (n pre-notes, offsets in scale steps from the held tone, onsets and lengths) at landing la */
static int mo_motif(const mo_song_t *s, int var, int la, int *off, int *on, int *len)
{
    int n = s->npre, k, t = la, base = 3 - n;
    if (var == AV_TAIL && n > 1) { base = 2; n = 1; }
    for (k = n - 1; k >= 0; k--) {
        int g = GAP[s->gap][base + k];
        t -= g;
        on[k] = t;
        len[k] = g >= 4 ? g - 1 : g;
        off[k] = CONT[s->cont][base + k];
    }
    if (var == AV_TURN && n) off[n - 1] = off[n - 1] ? -off[n - 1] : -1;
    if (var == AV_EXT && on[0] >= 2) {                   /* one more note before, continuing the contour */
        for (k = n; k > 0; k--) { on[k] = on[k - 1]; len[k] = len[k - 1]; off[k] = off[k - 1]; }
        on[0] = on[1] - 2;
        len[0] = 2;
        off[0] = off[1] + (off[1] - (n > 1 ? off[2] : 0) >= 0 ? 1 : -1);
        n++;
    }
    return n;
}

/* the held tone and the motif before it, fitted to the chord by pitch level: the landing whose motif fits whole,
   nearest target; failing notes then move a scale step or two, or drop. Returns the landing (0 none). */
static int mo_fit(lead_state_t *ls, const leads_ctx_t *c, const int *off, const int *on, const int *len, int n, int la,
                  int target, int end, const leads_next_t *x, int prev, int v0, int answer, unsigned scale)
{
    int note, best = 0, bc = 1 << 30, k, lo = x ? c->lo_x : c->lo, minor = 0, j, notes[6];
    int colpc = leads_color_pc(c->r, c->pcs, c->np);
    for (j = 0; j < c->np; j++) minor |= (c->pcs[j] - c->pcs[0] + 12) % 12 == 3;
    for (note = lo; note <= c->hi; note++) {
        int iv = (note - c->pcs[0] + 120) % 12, cost;
        if (!leads_legal(c->r, note, c->pcs, c->np, c->bass)) continue;
        if (x && !leads_legal(c->r, note, x->pcs, x->np, x->bass)) continue;
        if (la % 8 == 0 && !leads_is_ct(note, c->pcs, c->np)) continue;
        cost = (note > target ? note - target : target - note) * 2;
        if (iv == 0 && !answer) cost += 6;
        if (iv == 7) cost += 2;
        if (iv == 2 || iv == 9 || (minor && iv == 5)) cost -= 2;
        if (note % 12 == colpc) cost += 5;               /* the chord's color note: a passing touch, not a held tone */
        if (prev && (note - prev > 9 || prev - note > 9)) cost += 20;
        for (k = 0; k < n; k++) {
            int m = leads_coll_step(scale, note, off[k]);
            if (!leads_ok(c, m, on[k] % 8 == 0)) cost += 30;
        }
        if (cost < bc) { bc = cost; best = note; }
    }
    if (!best) return 0;
    for (k = 0; k < n; k++) {
        int m = leads_coll_step(scale, best, off[k]), d, pl = k ? notes[k - 1] : 0;
        if (!leads_ok(c, m, on[k] % 8 == 0)) {
            static const int8_t TRY[4] = {1, -1, 2, -2};
            int alt = 0, any = 0, dp = k ? (off[k] > off[k - 1]) - (off[k] < off[k - 1]) : 0;
            int dn = (0 > off[k]) - (0 < off[k]);          /* the move on to the held tone */
            for (d = 0; d < 4 && !alt; d++) {               /* a step or two off, keeping both moves' directions */
                int a = leads_coll_step(scale, best, off[k] + TRY[d]);
                if (!leads_ok(c, a, on[k] % 8 == 0)) continue;
                if (!any) any = a;
                if ((!pl || (a > pl) - (a < pl) == dp) && (best > a) - (best < a) == dn &&
                    (a > m ? a - m : m - a) <= 3 && (!pl || (a > pl ? a - pl : pl - a) <= 5)) alt = a;   /* near the
                                                                    planned note: no leap for the contour's sake */
            }
            m = alt ? alt : any;
        }
        notes[k] = on[k] >= 0 ? m : 0;
    }
    for (k = 0; k < n; k++)
        if (notes[k]) leads_put(ls, c, on[k] * 3, notes[k], on[k] + len[k], v0 + 2 * k);
    leads_put(ls, c, la * 3, best, end, v0 + 8);
    ls->acc |= 1u << la;
    return best;
}

/* a fall-off: a short note one or two scale steps under the held tone, right as it ends at 16th t */
static void mo_fall(lead_state_t *ls, const leads_ctx_t *c, int t, int held, int vel, unsigned scale)
{
    int k;
    if (t > 14 || !held) return;
    for (k = 1; k <= 2; k++) {
        int m = leads_coll_step(scale, held, -k);
        if (leads_ok(c, m, t % 8 == 0)) { leads_put(ls, c, t * 3, m, t + 2, vel - 16 < 40 ? 40 : vel - 16); return; }
    }
}

void modal_plan(lead_state_t *ls, const leads_ctx_t *c, const plan_block_t *b, int bar, const leads_next_t *nx)
{
    int run, pass, bip, pbars, last, role, half, i, key = c->r->v[RF_KEY], center, prev = ls->last, anchor;
    int from = ls->tied ? ls->tie_end : 0, held = ls->tied ? ls->st.mo.note : 0, hv = ls->st.mo.vel, v0;
    uint32_t seed = song_ov_phrase_seed(c->r->seed);
    unsigned scale = leads_scale_mask(c->r, c->pcs, c->np);
    mo_song_t s;
    rng_t g;
    const leads_next_t *x = c->can_push && nx->np > 0 ? nx : 0;
    ls->tied = 0;
    ls->tie_end = 0;
    leads_phrase_at(b, bar, &run, &pass, &bip, &pbars, &last);
    role = bip % 4;
    half = bip / 4;
    if (pbars < 8 && last) half = 1;
    mo_song(seed, c->r->v[RF_MOTION], &s);
    rng_seed(&g, seed ^ ((uint32_t)run * 0x27D4EB2Fu) ^ ((uint32_t)(pass * 2 + half) * 0x165667B1u), 301 + role);
    center = c->lo + 3 > 72 ? c->lo + 3 : 72;           /* the middle register */
    if (center > c->hi - 8) center = c->hi - 8;
    anchor = leads_place(key % 12, center - 6, center + 5, center);
    if (!anchor) anchor = center;
    if (bip == 0) prev = 0;
    {
        const lib_set_t *set = lib_set_by_id((uint8_t)c->r->v[RF_SET]);
        v0 = set && !lib_ins_sustained(set->lead) && !c->vibes ? 80 : 58;   /* a struck lead sings over the keys */
    }
    if (role == 0 || role == 2) {
        int off[6], on[6], len[6], n, var = AV_SAME, la = s.la, tgt, end, land, tie;
        if (role == 2) {
            static const uint8_t AW[AV_N] = {25, 25, 15, 15, 20};
            var = rng_weighted(&g, AW, AV_N);
            if (var == AV_LATE) la = la + 4 <= 13 ? la + 4 : la + 2 <= 13 ? la + 2 : la;
        }
        n = mo_motif(&s, var, la, off, on, len);
        /* (the heads of every other 8 bars restated a 3rd higher: the pitch level only) */
        tgt = leads_coll_step(scale, anchor, role == 0 ? (half ? 2 : 1) + 2 * (pass & 1) : (half ? -2 : -1));
        tie = rng_int(&g, 10) < (role == 0 ? 8 : 5);
        end = 16 + s.hold;
        land = tie && x ? mo_fit(ls, c, off, on, len, n, la, tgt, end, x, prev, v0, role == 2, scale) : 0;
        if (land && land >= c->lo_x) {
            ls->ring |= 1u << la;
            ls->tied = 1;
            ls->tie_end = s.hold;
            ls->st.mo.note = land;
            ls->st.mo.vel = v0 + 8;
        } else {
            int fall = rng_int(&g, 10) < 4;
            for (i = 0; i < 16; i++) { ls->on[i] = 0; ls->dl[i] = 0; ls->vel[i] = 0; }
            ls->acc = 0;
            ls->ring = 0;
            end = fall ? 14 : 16;
            land = mo_fit(ls, c, off, on, len, n, la, tgt, end, 0, prev, v0, role == 2, scale);
            if (land && fall) mo_fall(ls, c, 14, land, v0 + 8, scale);
        }
        if (land) prev = land;
    } else {                                             /* the hold, then a fall-off, an echo, or silence */
        if (held && rng_int(&g, 10) < 5) mo_fall(ls, c, from, held, hv, scale);
        else if (role == 3 && s.echo && !half && from <= 6) {   /* the motif's tail, late, as an echo */
            int off[6], on[6], len[6], n;
            n = mo_motif(&s, AV_TAIL, 14, off, on, len);
            if (on[0] >= from + 2)
                mo_fit(ls, c, off, on, len, n, 14, leads_coll_step(scale, anchor, -1), 16, 0, prev, v0 - 8, 1, scale);
        }
    }
    ls->last = prev;
}

/* The song's motif: its phrase heads' notes as the song draws them (before the legal notes and the range bend them):
   its 1-3 pre-notes (the contour's last n, in scale steps from the held tone) into the held tone, which rings to the
   bar's end and over it. */
void modal_motif(const song_roll_t *r, lead_motif_t *m)
{
    mo_song_t s;
    int off[6], on[6], len[6], si[6], at[6], n, k;
    mo_song(song_ov_phrase_seed(r->seed), r->v[RF_MOTION], &s);
    n = mo_motif(&s, AV_SAME, s.la, off, on, len);
    for (k = 0; k < n; k++) {
        at[k] = on[k] - on[0];
        si[k] = off[k];
    }
    at[n] = s.la - on[0];
    len[n] = 16 - s.la;
    si[n] = 0;
    lead_motif_from(m, n + 1, at, len, si);
    m->scale = LM_MODE;
}

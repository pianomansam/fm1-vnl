/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Jazz-Hop (lead-styles design, 2026-10-29; ported unchanged from its spike but for the shared register): a
   sampled-loop hook: one short hook per song (2-4 quick notes into a held tone, all in one bar), restated every
   2 bars like a looped record, the drums answering in the bar between. A 2-bar unit, four per 8 bars:
     bar 1  the hook: syncopated, mostly off beat 1, its onsets stepped off the snare (the line speaks between the
            backbeats), landing on a held tone by step 11 at the latest
     bar 2  the answer: the drums (the held tone ringing on, or silence), or the hook's tail echoed, softer, lower
   Units 1-3 of each 8 bars restate the hook exactly (rhythm and contour; the same target pitch every time, so over
   the same chord it is the same notes: the loop). Unit 4 is the turnaround, with ONE change: the held tone a step
   up, the first note stuttered into two 16ths, the first note chopped off, or the whole hook an 8th late. After
   the first 8 bars, unit 2 sometimes takes the same change (sparse variation over a long loop). Notes step through
   the mode's pentatonic (2nds and 3rds) in a narrow band, a 6th at most. Every note passes leads_legal().
   The hook floor (Sam 2026-11-03): every hook statement plays 3 pitches or more, a perfect 4th or wider: the
   hook is drawn with 3 pitch levels, and where the band and the chord fold it, a note moves to another chord tone
   (jh_spread). */
#include <stdint.h>
#include "leads.h"
#include "library.h"
#include "rng.h"
#include "override.h"

#define JH_MAX 7
enum { JS_FALL, JS_RISE, JS_ARCH, JS_NEIGH, JS_N };
enum { JV_SAME, JV_UP, JV_STUT, JV_DROP, JV_LATE, JV_N };
enum { JA_REST, JA_ECHO, JA_HOLD, JA_N };
typedef struct { int n, on[JH_MAX], len[JH_MAX], off[JH_MAX], la, ans; } jh_hook_t;

static void jh_song(uint32_t seed, int motion, unsigned snare, jh_hook_t *h)
{
    static const uint8_t SW[5] = {15, 35, 15, 25, 10}, GW[4] = {15, 45, 25, 15}, KW[JS_N] = {35, 15, 30, 20},
                         AW[JA_N] = {40, 35, 25};
    static const int8_t START[5] = {0, 2, 3, 6, 1};
    int npre, k, t = 0, tries, kind, cur;
    rng_t g;
    rng_seed(&g, seed, 400);
    npre = motion < 40 ? 2 + rng_int(&g, 2) : motion <= 65 ? 3 : 3 + rng_int(&g, 2);
    for (tries = 0; tries < 8; tries++) {             /* the rhythm: a start, gaps of a 16th to a quarter, onsets
                                                         stepped off the snare (just after it: answering it) */
        int st = START[rng_weighted(&g, SW, 5)];
        if (tries >= 4 && st > 2) st = 2;
        t = st;
        for (k = 0; k < npre; k++) {
            int gap;
            if ((snare >> t) & 1u && rng_int(&g, 10) < 8) t++;
            h->on[k] = t;
            gap = 1 + rng_weighted(&g, GW, 4);
            if (gap == 1 && k == npre - 1) gap = 2;   /* a 16th only inside the hook, never into the held tone */
            if (tries >= 4 && gap > 2) gap = 2;
            h->len[k] = gap >= 4 ? 3 : gap;
            t += gap;
        }
        if ((snare >> t) & 1u) t++;
        if (t <= 11) break;
    }
    if (t > 11) {                                     /* fallback: 8ths from the "and" of 1 */
        for (k = 0; k < npre; k++) { h->on[k] = 2 + 2 * k; h->len[k] = 2; }
        t = 2 + 2 * npre;
    }
    h->la = t;
    h->n = npre;
    kind = rng_weighted(&g, KW, JS_N);                /* the contour, back from the held tone (offset 0), in steps
                                                         of the mode's pentatonic */
    cur = 0;
    for (k = npre - 1; k >= 0; k--) {
        int d, x = rng_int(&g, 100);
        switch (kind) {
        case JS_FALL: d = x < 70 ? 1 : x < 85 ? 2 : 0; break;              /* falling into the held tone */
        case JS_RISE: d = x < 70 ? -1 : x < 85 ? -2 : 0; break;            /* rising into it */
        case JS_ARCH: d = k == npre - 1 ? 1 : x < 75 ? -1 : 0; break;      /* up to a peak, a step down onto it */
        default: d = (npre - 1 - k) % 2 ? -1 : x < 50 ? 0 : 1; break;      /* a repeated note and its neighbor */
        }
        cur += d;
        if (cur > 3) cur = 3;
        if (cur < -3) cur = -3;
        h->off[k] = cur;
    }
    if (h->off[npre - 1] == 0 && h->off[0] == 0) h->off[0] = 1;            /* never one pitch throughout */
    {   /* the hook floor: 3 pitch levels at least (the held tone's among them): the earliest note sharing its
           level moved a step outward, past the hook's top or bottom (the rhythm kept; no draw, so the song's rolls
           are the same) */
        int mx = 0, mn = 0, d = 0, lv;
        for (k = 0; k < npre; k++) {
            if (h->off[k] > mx) mx = h->off[k];
            if (h->off[k] < mn) mn = h->off[k];
        }
        for (lv = mn; lv <= mx; lv++) {
            int has = lv == 0;
            for (k = 0; k < npre; k++) has |= h->off[k] == lv;
            d += has;
        }
        for (k = 0; k < npre && d < 3; k++) {
            int j, was = h->off[k], shared = was == 0, up = 0, dn = 0;
            for (j = 0; j < npre; j++) shared |= j != k && h->off[j] == was;
            if (!shared) continue;
            for (j = k - 1; j <= k + 1; j += 2) {     /* the side that turns fewer rises and falls round */
                int o, sw;
                if (j < 0) continue;
                o = j < npre ? h->off[j] : 0;         /* (past the last: the held tone) */
                sw = (was > o) - (was < o);
                up += sw != 1;
                dn += sw != -1;
            }
            h->off[k] = up < dn || (up == dn && was >= 0) ? mx + 1 : mn - 1;
            d++;
        }
    }
    h->ans = rng_weighted(&g, AW, JA_N);
}

/* the hook with one change (var), into on/len/off; returns its notes before the held tone, *la the held tone's
   16th; *var the change made (one that can't fit falls back) */
static int jh_vary(const jh_hook_t *h, int *var, int *on, int *len, int *off, int *la)
{
    int n = h->n, k;
    for (k = 0; k < n; k++) { on[k] = h->on[k]; len[k] = h->len[k]; off[k] = h->off[k]; }
    *la = h->la;
    if (*var == JV_STUT && h->len[0] < 2) *var = JV_LATE;
    if (*var == JV_DROP && n < 2) *var = JV_UP;
    if (*var == JV_LATE && h->la + 2 > 13) *var = JV_UP;
    if (*var == JV_STUT) {                            /* the first note retriggered: two 16ths, a sampler's chop */
        for (k = n; k > 0; k--) { on[k] = on[k - 1]; len[k] = len[k - 1]; off[k] = off[k - 1]; }
        len[0] = 1;
        on[1] = on[0] + 1;
        len[1] = h->len[0] - 1;
        n++;
    } else if (*var == JV_DROP) {                     /* the first note chopped off */
        for (k = 0; k + 1 < n; k++) { on[k] = on[k + 1]; len[k] = len[k + 1]; off[k] = off[k + 1]; }
        n--;
    } else if (*var == JV_LATE) {                     /* the whole hook an 8th late */
        for (k = 0; k < n; k++) on[k] += 2;
        *la += 2;
    }
    return n;
}

/* the hook floor (Sam 2026-11-03): the floor's measure of a hook's pitches p[0..n] (0: a dropped note): its
   distinct pitches (3 counted) and its span (5 counted), 8 a pitch and 1 a semitone; JH_FLOOR met */
#define JH_FLOOR (3 * 8 + 5)
static int jh_floor(const int *p, int n)
{
    int j, q, d = 0, a = 128, b = 0;
    for (j = 0; j <= n; j++) {
        if (!p[j]) continue;
        for (q = 0; q < j && p[q] != p[j]; q++) {}
        d += q == j;
        if (p[j] < a) a = p[j];
        if (p[j] > b) b = p[j];
    }
    return (d < 3 ? d : 3) * 8 + (b - a < 5 ? (b > a ? b - a : 0) : 5);
}

/* the hook's pitches p[0..n] (p[n] the held tone) to at least 3 distinct, a perfect 4th or more from lowest to
   highest: one note moved to another chord tone in the band (two where one can't), each move the one that gets
   nearest the floor, then the smallest; the held tone's move dearer, one that turns a rise or fall round dearer
   still (the contour kept where it can). Where chord tones can't, a note off the beat may take a pentatonic tone.
   lo: the held tone's floor (it may ring on: x, the next bar's chord, must pass too). Returns 0 where it can't (the
   hook as it was). */
static int jh_spread(const leads_ctx_t *c, int *p, int n, const int *on, int la, int lo, const leads_next_t *x,
                     unsigned pent)
{
    int q[JH_MAX + 1], pass, mv, k, m, j;
    for (pass = 0; pass < 2; pass++) {
        for (k = 0; k <= n; k++) q[k] = p[k];
        for (mv = 0; mv < 2; mv++) {
            int best = -1, bm = 0, bs = jh_floor(q, n), bc = 1 << 30;
            for (k = 0; k <= n; k++) {
                int strong = (k < n ? on[k] : la) % 8 == 0, was = q[k];
                if (!was) continue;
                for (m = k < n ? c->lo : lo; m <= c->hi; m++) {
                    int sc, cost;
                    if (m == was || !leads_ok(c, m, strong)) continue;
                    if (!leads_is_ct(m, c->pcs, c->np) && (!pass || strong || !((pent >> (m % 12)) & 1u))) continue;
                    if (k == n && x && !leads_legal(c->r, m, x->pcs, x->np, x->bass)) continue;
                    q[k] = m;
                    sc = jh_floor(q, n);
                    q[k] = was;
                    cost = (m > was ? m - was : was - m) + (k == n ? 4 : 0);
                    for (j = k - 1; j <= k + 1; j += 2)   /* a rise or fall into or out of it turned round */
                        if (j >= 0 && j <= n && q[j] && (was > q[j]) - (was < q[j]) != (m > q[j]) - (m < q[j]))
                            cost += 20;
                    if (sc > bs || (sc == bs && best >= 0 && cost < bc)) { bs = sc; bc = cost; best = k; bm = m; }
                }
            }
            if (best < 0) break;
            q[best] = bm;
            if (bs == JH_FLOOR) {
                for (k = 0; k <= n; k++) p[k] = q[k];
                return 1;
            }
        }
    }
    return 0;
}

/* the held tone and the notes before it, fitted to the chord by pitch level only: the landing whose hook fits
   whole, nearest target (the same target each time: the loop's same notes over the same chord); a note that
   can't sound moves a step or two (direction kept where it can), or drops. up: the held tone a step higher (the
   turnaround). floor: a hook (held to the hook floor, jh_spread). Returns the held tone (0 none). */
static int jh_fit(lead_state_t *ls, const leads_ctx_t *c, const int *off, const int *on, const int *len, int n,
                  int la, int target, int end, const leads_next_t *x, int prev, int v0, int up, unsigned pent,
                  unsigned scale, int floor)
{
    int note, best = 0, bc = 1 << 30, k, lo = x ? c->lo_x : c->lo, j, notes[JH_MAX];
    int minor = 0, held, hoff = 0;
    for (j = 0; j < c->np; j++) minor |= (c->pcs[j] - c->pcs[0] + 12) % 12 == 3;
    for (note = lo; note <= c->hi; note++) {
        int iv = (note - c->pcs[0] + 120) % 12, cost;
        if (!leads_legal(c->r, note, c->pcs, c->np, c->bass)) continue;
        if (x && !leads_legal(c->r, note, x->pcs, x->np, x->bass)) continue;
        if (la % 8 == 0 && !leads_is_ct(note, c->pcs, c->np)) continue;
        cost = (note > target ? note - target : target - note) * 2;
        if (iv == 0) cost += 3;
        if (iv == 2 || iv == 9 || (minor && iv == 5)) cost -= 2;   /* the 9th, 13th, 11th: the sampled-jazz color */
        if (!leads_is_ct(note, c->pcs, c->np) && !((pent >> (note % 12)) & 1u)) cost += 3;
        if (prev && (note - prev > 9 || prev - note > 9)) cost += 20;
        if (c->keys[3] && note == c->keys[3] + 12) cost += 4;
        for (k = 0; k < n; k++)                       /* a moved note costs less than a register jump */
            if (!leads_ok(c, leads_coll_step(pent, note, off[k]), on[k] % 8 == 0)) cost += 15;
        if (cost < bc) { bc = cost; best = note; }
    }
    if (!best) return 0;
    held = best;
    if (up) {                                         /* the turnaround: the held tone a step up, the rest kept */
        int a = leads_coll_step(pent, best, 1), s = leads_coll_step(scale, best, 1);
        if (leads_ok(c, a, la % 8 == 0)) { held = a; hoff = 1; }
        else if (leads_ok(c, s, la % 8 == 0)) { held = s; hoff = 1; }
    }
    for (k = 0; k < n; k++) {
        int m = leads_coll_step(pent, best, off[k]), d, pl = k ? notes[k - 1] : 0;
        if (!leads_ok(c, m, on[k] % 8 == 0)) {
            static const int8_t TRY[4] = {1, -1, 2, -2};
            int alt = 0, any = 0, dp = k ? (off[k] > off[k - 1]) - (off[k] < off[k - 1]) : 0;
            int dn = (hoff > off[k]) - (hoff < off[k]);
            for (d = 0; d < 4 && !alt; d++) {
                int a = leads_coll_step(scale, m, TRY[d]);
                int ref = pl ? pl : held;
                if (!leads_ok(c, a, on[k] % 8 == 0)) continue;
                if ((!pl || (a > pl) - (a < pl) == dp) && (held > a) - (held < a) == dn &&
                    (!pl || (a > pl ? a - pl : pl - a) <= 5) && (a > held ? a - held : held - a) <= 9)
                    alt = a;                          /* near: no leap for the contour's sake */
                else if (!any || (a > ref ? a - ref : ref - a) < (any > ref ? any - ref : ref - any)) any = a;
            }
            m = alt ? alt : any;
        }
        notes[k] = m;
    }
    if (floor) {
        notes[n] = held;
        if (jh_floor(notes, n) < JH_FLOOR && jh_spread(c, notes, n, on, la, lo, x, pent)) held = notes[n];
    }
    for (k = 0; k < n; k++)
        if (notes[k])
            leads_put(ls, c, on[k] * 3, notes[k], on[k] + len[k], v0 + (k == 0 ? 6 : 0) - (len[k] == 1 ? 10 : 0));
    leads_put(ls, c, la * 3, held, end, v0 + 4);
    ls->acc |= 1u << la;
    return held;
}

/* the drum pattern's snare hits (bits by 16th; a backbeat where none) */
static unsigned jh_snare(const song_roll_t *r)
{
    const lib_pattern_t *dp = lib_pattern_by_id((uint8_t)r->v[RF_PATTERN]);
    return (dp ? lib_lane_hits(dp, D_SNARE, 0) : 0x1010u) & 0xFFFFu;
}

void jazzhop_plan(lead_state_t *ls, const leads_ctx_t *c, const plan_block_t *b, int bar, const leads_next_t *nx)
{
    static const uint8_t VW[4] = {30, 25, 20, 25};
    int run, pass, bip, pbars, last, i, key = c->r->v[RF_KEY], center, prev = ls->last, anchor, unit, v0, var, vary1;
    int from = ls->tied ? ls->tie_end : 0;
    uint32_t seed = song_ov_phrase_seed(c->r->seed);
    unsigned scale = leads_scale_mask(c->r), pent = leads_pent_mask(c->r, 0);
    const lib_set_t *set = lib_set_by_id((uint8_t)c->r->v[RF_SET]);
    unsigned snare = jh_snare(c->r);
    jh_hook_t h;
    rng_t g;
    const leads_next_t *x = c->can_push && nx->np > 0 ? nx : 0;
    ls->tied = 0;
    ls->tie_end = 0;
    leads_phrase_at(b, bar, &run, &pass, &bip, &pbars, &last);
    unit = bip / 2;
    if (pbars < 8 && last && unit < 2) unit += 2;     /* a 4-bar remainder ends on the turnaround */
    jh_song(seed, c->r->v[RF_MOTION], snare, &h);
    rng_seed(&g, seed ^ ((uint32_t)run * 0x27D4EB2Fu) ^ ((uint32_t)pass * 0x165667B1u), 401);
    var = JV_UP + rng_weighted(&g, VW, 4);
    vary1 = pass > 0 && rng_int(&g, 100) < 30;
    center = c->lo + 3 > 72 ? c->lo + 3 : 72;
    if (center > c->hi - 8) center = c->hi - 8;
    anchor = leads_place(key % 12, center - 6, center + 5, center);
    if (!anchor) anchor = center;
    v0 = set && !lib_ins_sustained(set->lead) && !c->vibes ? 80 : 62;   /* a struck lead sings over the keys */
    if (bip % 2 == 0) {                               /* the hook */
        int on[JH_MAX], len[JH_MAX], off[JH_MAX], la, n, v = unit == 3 || (unit == 1 && vary1) ? var : JV_SAME;
        int land = 0, tie;
        n = jh_vary(&h, &v, on, len, off, &la);
        tie = h.ans == JA_HOLD && unit != 3 && v != JV_UP;
        if (tie && x) land = jh_fit(ls, c, off, on, len, n, la, anchor, 16 + 6, x, prev, v0, 0, pent, scale, 1);
        if (land && land >= c->lo_x) {
            ls->ring |= 1u << la;
            ls->tied = 1;
            ls->tie_end = 6;
        } else {
            for (i = 0; i < 16; i++) { ls->on[i] = 0; ls->dl[i] = 0; ls->vel[i] = 0; }
            ls->acc = 0;
            ls->ring = 0;
            land = jh_fit(ls, c, off, on, len, n, la, anchor, 15, 0, prev, v0, v == JV_UP, pent, scale, 1);
        }
        if (land) prev = land;
    } else if (h.ans == JA_ECHO && unit != 3 && !from) {   /* the hook's tail echoed, late, softer, a step lower */
        int eon[1], elen[1], eoff[1], ela = 10;
        eon[0] = ela - (h.la - h.on[h.n - 1]);
        if ((snare >> ela) & 1u) { ela++; eon[0]++; }
        elen[0] = h.len[h.n - 1];
        eoff[0] = h.off[h.n - 1];
        i = jh_fit(ls, c, eoff, eon, elen, 1, ela, leads_coll_step(pent, anchor, -1), 15, 0, prev, v0 - 12, 0, pent,
                   scale, 0);
        if (i) prev = i;
    }
    ls->last = prev;
}

/* The song's motif: its hook as the song draws it (before the legal notes and the range bend it): its 2-4 quick
   notes (in the mode's pentatonic, steps from the held tone) into the held tone, which rings to the bar's last 16th
   (or over the bar line). */
void jazzhop_motif(const song_roll_t *r, lead_motif_t *m)
{
    jh_hook_t h;
    int at[JH_MAX + 1], len[JH_MAX + 1], si[JH_MAX + 1], k;
    jh_song(song_ov_phrase_seed(r->seed), r->v[RF_MOTION], jh_snare(r), &h);
    for (k = 0; k < h.n; k++) {
        at[k] = h.on[k] - h.on[0];
        len[k] = h.len[k];
        si[k] = h.off[k];
    }
    at[h.n] = h.la - h.on[0];
    len[h.n] = 15 - h.la;
    si[h.n] = 0;
    lead_motif_from(m, h.n + 1, at, len, si);
    m->scale = LM_PENT;
}

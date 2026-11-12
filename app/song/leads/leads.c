/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdint.h>
#include "leads.h"
#include "library.h"
#include "override.h"
#include "harmony.h"
#include "theory.h"
#include "groove.h"
#include "rng.h"

int lead_motif_from(lead_motif_t *m, int n, const int *at, const int *len, const int *si)
{
    lead_motif_t t;
    int i;
    if (n < 1 || n > 8) return 0;
    for (i = 0; i < n; i++) {
        int st = si[i] - si[0];
        if (at[i] < 0 || at[i] > 255 || len[i] < 1 || len[i] > 255 || st < -128 || st > 127) return 0;
        if (i && at[i] <= at[i - 1]) return 0;
    }
    t.n = (uint8_t)n;
    t.contour = 0;
    t.scale = LM_MODE;
    for (i = 0; i < 8; i++) {
        t.at[i] = (uint8_t)(i < n ? at[i] : 0);
        t.len[i] = (uint8_t)(i < n ? len[i] : 0);
        t.step[i] = (int8_t)(i < n ? si[i] - si[0] : 0);
        if (i && i < n && si[i] > si[i - 1]) t.contour |= (uint8_t)(1u << (i - 1));
    }
    *m = t;
    return 1;
}

int lead_motif_to(const lead_motif_t *m, int si0, int *at, int *len, int *si)
{
    int i;
    for (i = 0; i < m->n && i < 8; i++) {
        at[i] = m->at[i];
        len[i] = m->len[i];
        si[i] = si0 + m->step[i];
    }
    return i;
}

/* The styles each lead instrument plays (the spec's fits, bit LS_*): the flute any of the eight; the felt piano '60s
   Jazz, Jazz-Hop, Film Score, Ambient; the vibes Impressionism, '60s Jazz, Jazz-Hop, Bossa Nova, Ambient; the Knight
   '60s Jazz, Film Score, Ambient; the jazz guitar '60s Jazz, Modal Jazz, Jazz-Hop, Bossa Nova; the
   sampled cello and the harp Impressionism, Romanticism, Film Score, Ambient; the whistle all but '60s Jazz and Modal
   Jazz. Any other instrument on the lead (a workbench pick; the soft synth lead, a candidate) takes the flute's. */
#define LSB(s) (1u << (s))
static unsigned style_fit(int ins)
{
    if (ins == INS_FELT_PIANO) return LSB(LS_JAZZ60S) | LSB(LS_JAZZHOP) | LSB(LS_FILMSCORE) | LSB(LS_AMBIENT);
    if (ins == INS_VIBES)
        return LSB(LS_IMPRESSIONISM) | LSB(LS_JAZZ60S) | LSB(LS_JAZZHOP) | LSB(LS_BOSSA) | LSB(LS_AMBIENT);
    if (ins == INS_KNIGHT) return LSB(LS_JAZZ60S) | LSB(LS_FILMSCORE) | LSB(LS_AMBIENT);
    if (ins == INS_JAZZ_GTR) return LSB(LS_JAZZ60S) | LSB(LS_MODAL) | LSB(LS_JAZZHOP) | LSB(LS_BOSSA);
    if (ins == INS_SAMP_CELLO || ins == INS_HARP)
        return LSB(LS_IMPRESSIONISM) | LSB(LS_ROMANTICISM) | LSB(LS_FILMSCORE) | LSB(LS_AMBIENT);
    if (ins == INS_WHISTLE) /* (not the jazz styles' runs: a whistled line is a tune) */
        return (LSB(LS_N) - 1u) & ~(LSB(LS_JAZZ60S) | LSB(LS_MODAL));
    return LSB(LS_N) - 1u;
}

/* The set's flavor's weight on each style, in quarters (the spec's "Style roll", Sam tunes them by ear): lofi Jazz-Hop
   x3; bossa Bossa Nova x4; city pop Film Score and Jazz-Hop x2; neo-soul '60s Jazz, Modal Jazz and Jazz-Hop x2;
   chillwave Ambient x3 and Impressionism x2; the rest x1, but Bossa Nova x0.25 outside the bossa sets.
                  Impr  Rom  '60s  Modal  J-Hop  Film  Bossa  Amb */
static const uint8_t FLAVOR_W[5][LS_N] = {
    /* lofi */      {4,  4,   4,    4,    12,    4,    1,    4},
    /* bossa */     {4,  4,   4,    4,     4,    4,   16,    4},
    /* city pop */  {4,  4,   4,    4,     8,    8,    1,    4},
    /* neo-soul */  {4,  4,   8,    8,     8,    4,    1,    4},
    /* chillwave */ {8,  4,   4,    4,     4,    4,    1,   12},
};

int leads_weight(int ins, int flavor, int style)
{
    if (style < 0 || style >= LS_N || !((style_fit(ins) >> style) & 1u)) return 0;
    return FLAVOR_W[flavor >= 0 && flavor < 5 ? flavor : FL_LOFI][style];
}

/* The roll: weight = fit x flavor (leads_weight), drawn from the song's seed (hashed apart, its own stream: a lead
   re-roll, a salt, keeps it) */
int leads_style_roll(const song_roll_t *r)
{
    const lib_set_t *set = lib_set_by_id((uint8_t)r->v[RF_SET]);
    int ins = set ? set->lead : INS_FLUTE, fl = set ? set->flavor : FL_LOFI, tot = 0, k, i;
    rng_t g;
    for (i = 0; i < LS_N; i++) tot += leads_weight(ins, fl, i);
    rng_seed(&g, roll_mix(r->seed, 0x57E1u), 144);
    k = rng_int(&g, tot);
    for (i = 0; i < LS_N - 1; i++)
        if ((k -= leads_weight(ins, fl, i)) < 0) break;
    return i;
}

int leads_style(const song_roll_t *r)
{
    int s = song_ov()->lead_style;
    return s >= 2 && s < 2 + LS_N ? s - 2 : leads_style_roll(r);   /* forced; else (0 and 1, the retired engines,
                                                                        too) rolled */
}

int leads_fit(int ins, int style) { return style >= 0 && style < LS_N && (style_fit(ins) >> style) & 1u; }

int leads_style_keep(const song_roll_t *r, int prev)
{
    const lib_set_t *set = lib_set_by_id((uint8_t)r->v[RF_SET]);
    return leads_fit(set ? set->lead : INS_FLUTE, prev) ? prev : leads_style_roll(r);
}

/* The centers: each style's median lead pitch in its spike's five A/B songs (seeds 101-105: 1 flute G Aeolian,
   2 flute Db Lydian, 3 vibes B Dorian, 4 felt piano Ab Mixolydian, 5 flute A Ionian; the renders' json "lead" notes,
   app/build/new, 2026-10-29), per lead instrument; a half rounded down ('60s Jazz on the flute, 80.5). No spike put
   the Knight on the lead: it takes the style's flute center, as does any other lead instrument; a low lead
   (lib_lead_drop: the jazz guitar) the flute's an octave down, the soft synth lead's a 5th; the cello (a drop of 24)
   its own, CELLO_C (Bb3).

                      flute  felt   vibes  Knight  other   (notes measured: flute / felt / vibes)
     Impressionism     79     80     76     79      79      119 /  80 / 49
     Romanticism       80     78     78     80      80      164 / 104 / 47
     '60s Jazz         80     77     78     80      80      112 /  55 / 45
     Modal Jazz        77     82     76     77      77       89 /  51 / 38
     Jazz-Hop          76     80     76     76      76      105 /  63 / 40
     Film Score        78     80     76     78      78      139 /  79 / 46
     Bossa Nova        82     80     79     82      82      129 /  81 / 48
     Ambient           77     82     80     77      77      110 /  85 / 38                                    */
static const uint8_t CENTER[LS_N][5] = {
    {79, 80, 76, 79, 79}, {80, 78, 78, 80, 80}, {80, 77, 78, 80, 80}, {77, 82, 76, 77, 77},
    {76, 80, 76, 76, 76}, {78, 80, 76, 78, 78}, {82, 80, 79, 82, 82}, {77, 82, 80, 77, 77},
};

/* The cello's register, whatever the style (Sam, 2026-11-11: "the cello is an octave too high!" on seed 105, its line
   Db4-Ab4 where two octaves under the flute's style center floated it; then G3's center too low, under G3 a cello
   reads as bass): centered on Bb3, its band E3-E4, its ceiling Gb4 (range_bar; Sam: "the correct register", A4 let
   seed 105 climb to Db4-Ab4), a hard floor at D3 (CELLO_LO: the
   range's and, for any note a style still puts under it, an octave up in plan_bar), a cello's melody D3-A4 */
#define CELLO_C 58
#define CELLO_LO 50

int leads_center(int style, int ins)
{
    int col = ins == INS_FLUTE ? 0 : ins == INS_FELT_PIANO ? 1 : ins == INS_VIBES ? 2 : ins == INS_KNIGHT ? 3 : 4;
    int c = lib_lead_drop(ins) > 12 ? CELLO_C
                                    : CENTER[style >= 0 && style < LS_N ? style : 0][col] - lib_lead_drop(ins);
    int lo, hi;
    song_ov_range(ins, &lo, &hi);
    return c < lo + 6 ? lo + 6 : c > hi - 6 ? hi - 6 : c;
}

void leads_band_for(const song_roll_t *r, int style, int *lo, int *hi)
{
    const lib_set_t *set = lib_set_by_id((uint8_t)r->v[RF_SET]);
    int ins = set ? set->lead : INS_FLUTE, c = leads_center(style, ins), il, ih;
    song_ov_range(ins, &il, &ih);
    c += 12 * song_ov()->lead_oct;                       /* the workbench's octave, in the instrument's range */
    if (c < il + 6) c = il + 6;
    if (c > ih - 6) c = ih - 6;
    *lo = c - 6;
    *hi = c + 6;
}

static void range_bar(const song_roll_t *r, int style, const int keys[4], const int nkeys[4], int *lo, int *lo_x,
                      int *hi)
{
    const lib_set_t *set = lib_set_by_id((uint8_t)r->v[RF_SET]);
    int top = 0, ntop = 0, i, blo, bhi, il, ih, ko = harmony_keys_oct(r);
    int drop = set ? lib_lead_drop(set->lead) : 0, e6 = drop > 12 ? 66 : drop ? 76 : 88;   /* the ceiling: E6; a low
                                                                                            lead's E5, a cello's Gb4 */
    int lift = set && lib_lead_lift(set->lead);          /* the keys lift over it (the jazz guitar, the cello) */
    for (i = 0; i < 4; i++) {
        if (keys[i] > top) top = keys[i];
        if (nkeys[i] > ntop) ntop = nkeys[i];
    }
    song_ov_range(set ? set->lead : INS_FLUTE, &il, &ih);
    leads_band_for(r, style, &blo, &bhi);
    *lo = top >= blo && !lift ? top + 5 : blo;           /* the keys reach in: a 4th over them (a lead they lift
                                                            over: its band, the keys raised over it) */
    *lo_x = ntop >= blo && ntop + 5 > *lo && !lift ? ntop + 5 : *lo;
    *hi = e6 + 12 * song_ov()->lead_oct;                 /* E6, or the workbench's octave off it */
    if (ko > 0 && e6 + ko > *hi && !lift) *hi = e6 + ko; /* the workbench's keys raised: the ceiling with them */
    if (*hi > ih) *hi = ih > e6 ? ih : e6;               /* in the instrument's range (or E6 at least: as before) */
    if (*hi < bhi) *hi = bhi;                            /* an octave down: the band's top at least */
    if (lift && !song_ov()->keys_with) {                 /* the keys lifted over it (the jazz guitar): its top at
                                                            their lowest note (Sam: the keys' lowest never under the
                                                            guitar's top), a 6th of room under it in its range */
        int bot = 999;
        for (i = 0; i < 4; i++)
            if (keys[i] && keys[i] < bot) bot = keys[i];
        if (bot < *hi && bot >= il + 5) {               /* (keys left low, the lift refused: as before) */
            *hi = bot;
            if (*lo > *hi - 9) *lo = *hi - 9 > il ? *hi - 9 : il;
            if (*lo_x < *lo || *lo_x > *hi) *lo_x = *lo;
        }
    }                                                    /* (the keys with it, the workbench's keys_with: its band
                                                            as it is, the keys thinned where it plays: harmony.c) */
    if (drop > 12 && *hi >= CELLO_LO + 5) {               /* the cello's floor, D3 */
        if (*lo < CELLO_LO) *lo = CELLO_LO;
        if (*lo_x < *lo) *lo_x = *lo;
    }
}

void leads_range_bar(const song_roll_t *r, int style, const int keys[4], const int nkeys[4], int *lo, int *lo_x,
                     int *hi)
{
    range_bar(r, style, keys, nkeys, lo, lo_x, hi);
}

/* ---- the helpers the styles share ---- */

int leads_is_ct(int note, const int *pcs, int np)
{
    int j;
    for (j = 0; j < np; j++)
        if ((note - pcs[j] + 120) % 12 == 0) return 1;
    return 0;
}

int leads_legal(const song_roll_t *r, int note, const int *pcs, int np, unsigned bass)
{
    int j, pc = note % 12;
    if (th_is_avoid(r->v[RF_KEY], r->v[RF_MODE], pcs, np, note)) return 0;
    if (bass & (1u << ((pc + 1) % 12) | 1u << ((pc + 11) % 12))) return 0;   /* (R25: the maj7 over the root rubs) */
    for (j = 0; j < np; j++) {
        int d = (note - pcs[j] + 120) % 12;
        if (d == 1 || d == 11) return 0;
    }
    return leads_is_ct(note, pcs, np) || th_in_scale(r->v[RF_KEY], r->v[RF_MODE], note);
}

unsigned leads_legal_pcs(const song_roll_t *r, const int *pcs, int np, unsigned bass)
{
    unsigned m = 0;
    int pc;
    for (pc = 0; pc < 12; pc++)
        if (leads_legal(r, 60 + pc, pcs, np, bass)) m |= 1u << pc;
    return m;
}

int leads_place(int pc, int lo, int hi, int near)
{
    int best = 0, note;
    for (note = lo; note <= hi; note++)
        if (note % 12 == pc &&
            (!best || (note > near ? note - near : near - note) < (best > near ? best - near : near - best)))
            best = note;
    return best;
}

/* Where bar sits in its phrase (the sustained-lead design's call 1): a run of contiguous lead blocks (so a request
   that splits a block, or a 4-bar lead block after another, resumes the phrase rather than restarting it); 8-bar
   phrases from the run's start, a 4-bar remainder half a phrase (the answer when no lead block follows in the song).
   b is the plan's own block: its neighbors are read from the plan (blocks are contiguous from bar 0, and the plan's
   unused blocks are zero). */
void leads_phrase_at(const plan_block_t *b, int bar, int *run, int *pass, int *bip, int *pbars, int *last)
{
    const plan_block_t *f = b, *e = b, *x;
    int end;
    while (f->start > 0 && lead_plays(f - 1)) f--;
    while ((e + 1)->bars && (e + 1)->start == e->start + e->bars && lead_plays(e + 1)) e++;
    end = e->start + e->bars;
    *last = 1;
    for (x = e + 1; x->bars && x->start == (x - 1)->start + (x - 1)->bars; x++)
        if (lead_plays(x)) *last = 0;
    *run = f->start;
    *pass = (bar - f->start) / 8;
    *bip = (bar - f->start) % 8;
    *pbars = end - (f->start + *pass * 8) >= 8 ? 8 : 4;
}

int leads_flip(uint32_t seed, int run, int pass, int bip)
{
    rng_t g;
    rng_seed(&g, seed ^ ((uint32_t)run * 0x27D4EB2Fu) ^ ((uint32_t)(pass * 8 + bip) * 0x165667B1u), 700);
    return rng_int(&g, 100) < LEADS_FLIP_PCT;
}

void leads_scale(const song_roll_t *r, int *sc)
{
    unsigned pool = th_mode_pool(r->v[RF_MODE]);
    int i, k = 0;
    for (i = 0; i < 12 && k < 7; i++)
        if ((pool >> i) & 1u) sc[k++] = i;
    for (; k < 7; k++) sc[k] = sc[k - 1] + 1;
}

unsigned leads_scale_mask(const song_roll_t *r)
{
    int sc[7], i;
    unsigned m = 0;
    leads_scale(r, sc);
    for (i = 0; i < 7; i++) m |= 1u << ((r->v[RF_KEY] + sc[i]) % 12);
    return m;
}

unsigned leads_pent_mask(const song_roll_t *r, int color)
{
    /* the mode's pentatonic, as scale indexes (Ionian, Lydian, Mixolydian: 1 2 3 5 6; Dorian, Phrygian, Aeolian:
       1 b3 4 5 b7) */
    static const int8_t PENT[MODE_N][5] = {{0, 1, 2, 4, 5}, {0, 2, 3, 4, 6}, {0, 2, 3, 4, 6},
                                          {0, 1, 2, 4, 5}, {0, 1, 2, 4, 5}, {0, 2, 3, 4, 6}};
    int sc[7], i, key = r->v[RF_KEY], mode = r->v[RF_MODE], col = color ? leads_color_pc(r) : -1;
    unsigned m = 0;
    leads_scale(r, sc);
    for (i = 0; i < 5; i++) m |= 1u << ((key + sc[PENT[mode][i]]) % 12);
    if (col >= 0) m |= 1u << col;
    return m;
}

int leads_color_pc(const song_roll_t *r)
{
    static const int8_t COLOR[MODE_N] = {-1, 5, 1, 3, 6, 5};   /* scale indexes: Ionian none, Dorian 6, Phrygian b2,
                                                                  Lydian #4, Mixolydian b7, Aeolian b6 */
    int sc[7], c = r->v[RF_MODE] >= 0 && r->v[RF_MODE] < MODE_N ? COLOR[r->v[RF_MODE]] : -1;
    if (c < 0) return -1;
    leads_scale(r, sc);
    return (r->v[RF_KEY] + sc[c]) % 12;
}

int leads_si_note(int key, const int *sc, int si)
{
    int o = si >= 0 ? si / 7 : -((6 - si) / 7);
    return key + 12 * o + sc[si - 7 * o];
}

int leads_note_si(int key, const int *sc, int note)
{
    int d = note - key, o = d >= 0 ? d / 12 : -((11 - d) / 12), j = 6;
    d -= 12 * o;
    while (j > 0 && sc[j] > d) j--;
    return 7 * o + j;
}

int leads_coll_step(unsigned mask, int note, int k)
{
    int d = k > 0 ? 1 : -1, guard = 0;
    while (k && guard++ < 48) {
        note += d;
        if ((mask >> (note % 12)) & 1u) k -= d;
    }
    return note;
}

int leads_ok(const leads_ctx_t *c, int note, int strong)
{
    return note >= c->lo && note <= c->hi && leads_legal(c->r, note, c->pcs, c->np, c->bass) &&
           (!strong || leads_is_ct(note, c->pcs, c->np));
}

void leads_put(lead_state_t *ls, const leads_ctx_t *c, int p, int note, int end, int v)
{
    int t = p / 3;
    if (t < 0 || t > 15 || note <= 0 || end <= t) return;
    if (!c->pad && note < 76) v += (76 - note) / 2;     /* low: among percussive keys */
    if (c->vibes) v = 88 + (v - 50) / 3;                 /* struck, not swelled */
    ls->on[t] = note;
    ls->len[t] = end - t;
    ls->dl[t] = (uint8_t)(p % 3);
    ls->vel[t] = (uint8_t)(v < 25 ? 25 : v > 110 ? 110 : v);
}

/* ---- the dispatch ---- */

/* the styles' planners and motifs, by LS_* (each style's file) */
typedef void leads_plan_fn(lead_state_t *ls, const leads_ctx_t *c, const plan_block_t *b, int bar,
                           const leads_next_t *nx);
typedef void leads_motif_fn(const song_roll_t *r, lead_motif_t *m);
static leads_plan_fn *const PLAN[LS_N] = {impressionism_plan, romanticism_plan, jazz60s_plan, modal_plan,
                                          jazzhop_plan, filmscore_plan, bossa_plan, ambient_plan};
static leads_motif_fn *const MOTIF[LS_N] = {impressionism_motif, romanticism_motif, jazz60s_motif, modal_motif,
                                            jazzhop_motif, filmscore_motif, bossa_motif, ambient_motif};

int leads_motif(const song_roll_t *r, int style, lead_motif_t *m)
{
    MOTIF[style >= 0 && style < LS_N ? style : 0](r, m);
    return 1;
}

/* ---- a phrase never ends on a dominant's 7th (Sam, 2026-11-05: "the lead motif can end on the 7th of a dominant.
   That sounds bad") ---- */

static void plan_bar(lead_state_t *ls, const song_roll_t *r, const plan_block_t *b, int bar, const int keys[4],
                     const int nkeys[4], const int *pcs, int np, int can_push, int settle);

/* the chord's root, 3rd and 5th as pitch classes (its 5th: a perfect one, a m7b5's diminished one) */
static int triad(const int *pcs, int np, int *t)
{
    int i, n = 0, iv = 0;
    for (i = 1; i < np; i++) iv |= 1 << (pcs[i] - pcs[0] + 120) % 12;
    t[n++] = pcs[0] % 12;
    if (iv & (1 << 3 | 1 << 4)) t[n++] = (pcs[0] + (iv & 1 << 4 ? 4 : 3)) % 12;
    t[n++] = (pcs[0] + (!(iv & 1 << 7) && (iv & 1 << 3) && (iv & 1 << 6) ? 6 : 7)) % 12;
    return n;
}

/* 1 where the bar's notes (with step t's note q) meet the hook floor: 3 pitches or more spanning a 4th or wider (a bar
   of under 3 notes always does; loop.c's bar_floor) */
static int hook_floor(const int *on, int t, int q)
{
    int i, j, n = 0, d = 0, lo = 128, hi = 0;
    for (i = 0; i < 16; i++) {
        int x = i == t ? q : on[i];
        if (x <= 0) continue;
        n++;
        for (j = 0; j < i && (j == t ? q : on[j]) != x; j++) {}
        d += j == i;
        if (x < lo) lo = x;
        if (x > hi) hi = x;
    }
    return n < 3 || (d >= 3 && hi - lo >= 5);
}

/* The next bar's lead onsets (bit i: an onset at 16th i), planned ahead on a copy of the lead's state as the next
   bar will plan them (the style, or the loop's replay; its keys taken for the bar after too) */
static lead_state_t ahead;
static unsigned next_onsets(const lead_state_t *ls, const song_roll_t *r, const plan_block_t *nb, int bar,
                            const int nkeys[4], const int *npcs, int nnp)
{
    const plan_block_t *n2 = bar + 2 < nb->start + nb->bars ? nb
                           : (nb + 1)->bars && (nb + 1)->start == nb->start + nb->bars ? nb + 1 : 0;
    unsigned m = 0;
    int i;
    ahead = *ls;
    for (i = 0; i < 16; i++) { ahead.on[i] = 0; ahead.vel[i] = 0; ahead.dl[i] = 0; }
    ahead.acc = 0;
    ahead.ring = 0;
    plan_bar(&ahead, r, nb, bar + 1, nkeys, nkeys, npcs, nnp, n2 != 0, 0);
    for (i = 0; i < 16; i++)
        if (ahead.on[i] > 0) m |= 1u << i;
    return m;
}

/* The bar's settled notes (after the style or the loop's replay and stutter): a phrase-ending note (a beat's rest or
   more after it, or the lead's end; the next bar's onsets planned ahead) on the b7 of the dominant it ends over (the
   next bar's where it rings over the bar line) moves to the nearest root, 3rd or 5th of that chord in the bar's range,
   legal where it sounds (a chord tone on beats 1 and 3), on the same side of the note before it and keeping the bar's
   hook floor where it can, and 2 scale steps away at most (the loop's replay: always, as its nudges; else where it
   can). A ringing note with none is cut at the bar line (then checked over this bar's chord); a note with none at
   all drops. prev: the last note sung before the bar; nkeys: the next bar's keys. */
static void end_off_b7(lead_state_t *ls, const leads_ctx_t *c, const leads_next_t *nx, const plan_block_t *nb,
                       int bar, int style, int prev, const int nkeys[4])
{
    int b7 = th_dom_b7(c->pcs, c->np), nb7 = c->can_push && nx->np > 0 ? th_dom_b7(nx->pcs, nx->np) : -1, i, j;
    int moved = 0, known = 0, loop = leads_loop_plays(style);
    unsigned nxt = 0, scale = leads_scale_mask(c->r);
    if (b7 < 0 && nb7 < 0) return;
    for (i = 15; i >= 0; i--) {
        int note = ls->on[i], e = i + ls->len[i], ext = e > 16, tb7 = ext ? nb7 : b7, best = 0, bc = 1 << 20;
        int tri[3], nt, k, p, pp, floor, s2lo, s2hi;
        if (note <= 0 || tb7 < 0 || note % 12 != tb7) continue;
        s2lo = leads_coll_step(scale, note, -2);
        s2hi = leads_coll_step(scale, note, 2);
        for (j = i + 1; j < 16 && ls->on[j] <= 0; j++) {}
        if (j < 16) {
            if (j - e < 4) continue;                     /* the phrase goes on in the bar */
        } else if (nb && lead_plays(nb) && nx->np > 0) {
            if (!known) { nxt = next_onsets(ls, c->r, nb, bar, nkeys, nx->pcs, nx->np); known = 1; }
            for (j = ext ? e - 16 : 0; j < 16 && !((nxt >> j) & 1u); j++) {}
            if (j < 16 && 16 + j - e < 4) continue;      /* it goes on into the next bar */
        }
        for (pp = 0, j = i - 1; j >= 0 && !pp; j--) pp = ls->on[j];
        if (!pp) pp = prev;
        floor = hook_floor(ls->on, i, note);
        nt = ext ? triad(nx->pcs, nx->np, tri) : triad(c->pcs, c->np, tri);
        for (p = ext ? c->lo_x : c->lo; p <= c->hi; p++) {
            int cost = p > note ? p - note : note - p;
            for (k = 0; k < nt && p % 12 != tri[k]; k++) {}
            if (k == nt || !leads_legal(c->r, p, c->pcs, c->np, c->bass) ||
                (i % 8 == 0 && !leads_is_ct(p, c->pcs, c->np)) ||
                (ext && !leads_legal(c->r, p, nx->pcs, nx->np, nx->bass)))
                continue;
            if (pp && pp != note && (p == pp || (p > pp) != (note > pp))) cost += 100;   /* the contour turned */
            if (floor && !hook_floor(ls->on, i, p)) cost += 2000;                        /* the hook folded */
            if (p < s2lo || p > s2hi) {                  /* over 2 scale steps: the loop's replay never moves so far */
                if (loop) continue;
                cost += 1000;
            }
            if (cost < bc) { bc = cost; best = p; }
        }
        moved = 1;
        if (!best && ext) {                              /* cut at the bar line: over this bar's chord */
            ls->len[i] = 16 - i;
            ls->ring &= ~(1u << i);
            for (j = 0; j < 16 && !(ls->on[j] > 0 && ((ls->ring >> j) & 1u) && j + ls->len[j] > 16); j++) {}
            if (j == 16) { ls->tied = 0; ls->tie_end = 0; }
            i++;
            continue;
        }
        if (!best) {
            ls->on[i] = 0; ls->len[i] = 0; ls->vel[i] = 0; ls->dl[i] = 0;
            ls->ring &= ~(1u << i);
            ls->acc &= ~(1u << i);
            continue;
        }
        if (style == LS_MODAL && ext && ls->st.mo.note == note) ls->st.mo.note = best;   /* its held tone */
        ls->on[i] = best;
    }
    if (!moved) return;
    for (i = 15; i >= 0 && ls->on[i] <= 0; i--) {}
    if (i >= 0) ls->last = ls->on[i];
}

/* the bar's context: its range (over percussive keys the band, leads_range_bar; over a pad its place, the
   workbench's lead_oct moving it), the bass's pitch classes, and the next bar's chord; settle: the phrase's end off a
   dominant's 7th (end_off_b7; 0 for the bar planned ahead) */
static void plan_bar(lead_state_t *ls, const song_roll_t *r, const plan_block_t *b, int bar, const int keys[4],
                     const int nkeys[4], const int *pcs, int np, int can_push, int settle)
{
    const lib_prog_t *pr = lib_prog_by_id((uint8_t)r->v[RF_PROG]);
    const lib_set_t *set = lib_set_by_id((uint8_t)r->v[RF_SET]);
    const plan_block_t *nb = bar + 1 < b->start + b->bars ? b
                           : (b + 1)->bars && (b + 1)->start == b->start + b->bars ? b + 1 : 0;
    int s = ls->style ? ls->style - 1 : leads_style(r), bpc = pr && pr->bars_per_chord ? pr->bars_per_chord : 1;
    int loop = pr && pr->n ? pr->n * bpc : 4, npcs[5], i, prev;
    leads_ctx_t c;
    leads_next_t nx;
    c.r = r; c.keys = keys; c.pcs = pcs; c.np = np; c.bass = 0; c.can_push = can_push;
    c.pad = set && lib_ins_sustained(set->chords);
    c.vibes = set && set->lead == INS_VIBES;
    if (c.pad) {                                         /* a pad: the octave doesn't matter (Sam), its place does */
        int d = 12 * song_ov()->lead_oct - (set ? lib_lead_drop(set->lead) : 0);   /* (a low lead: down one or two) */
        c.lo = c.lo_x = 60;
        c.hi = harmony_pad_place(r) == PL_ABOVE ? 72 : 83;
        if (d && set) {                                  /* the workbench's octave, in the instrument's range; left
                                                            where under an octave would remain */
            int lo, hi, nlo = c.lo + d, nhi = c.hi + d;
            song_ov_range(set->lead, &lo, &hi);
            if (nlo < lo) nlo = lo;
            if (nhi > hi) nhi = hi;
            if (nhi - nlo >= 12) { c.lo = c.lo_x = nlo; c.hi = nhi; }
        }
    } else {
        range_bar(r, s, keys, nkeys, &c.lo, &c.lo_x, &c.hi);
    }
    for (i = 0; i < 16; i++) {
        int bn = groove_bass_note(r, b, bar, i, pcs, np);
        if (bn) c.bass |= 1u << (bn % 12);
    }
    nx.np = harmony_chord_at(r, bar + 1, (bar + 1) / loop + 1, 0, npcs);
    nx.pcs = npcs;
    nx.bass = 0;
    if (!nb) nx.np = 0;
    for (i = 0; nb && i < 16; i++) {
        int bn = groove_bass_note(r, nb, bar + 1, i, npcs, nx.np);
        if (bn) nx.bass |= 1u << (bn % 12);
    }
    prev = ls->last;
    if (leads_loop_plays(s)) leads_loop_bar(ls, &c, b, bar, &nx, s, PLAN[s]);
    else PLAN[s](ls, &c, b, bar, &nx);
    if (settle) end_off_b7(ls, &c, &nx, nb, bar, s, prev, nkeys);
    if (set && lib_lead_drop(set->lead) > 12)            /* the cello: never under D3 (an octave up) */
        for (i = 0; i < 16; i++)
            if (ls->on[i] > 0 && ls->on[i] < CELLO_LO) ls->on[i] += 12;
}

void leads_plan_bar(lead_state_t *ls, const song_roll_t *r, const plan_block_t *b, int bar, const int keys[4],
                    const int nkeys[4], const int *pcs, int np, int can_push)
{
    plan_bar(ls, r, b, bar, keys, nkeys, pcs, np, can_push, 1);
}

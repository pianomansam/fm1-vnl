/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdint.h>
#include "leads.h"
#include "library.h"
#include "override.h"
#include "harmony.h"

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

int leads_style(const song_roll_t *r)
{
    int s = song_ov()->lead_style;
    (void)r;                                             /* (the style roll comes with the sets: rolled, none yet) */
    return s >= 2 && s < 2 + LS_N ? s - 2 : -1;
}

/* The centers: each style's median lead pitch in its spike's five A/B songs (seeds 101-105: 1 flute G Aeolian,
   2 flute Db Lydian, 3 vibes B Dorian, 4 felt piano Ab Mixolydian, 5 flute A Ionian; the renders' json "lead" notes,
   app/build/new, 2026-10-29), per lead instrument; a half rounded down ('60s Jazz on the flute, 80.5). No spike put
   the Knight on the lead: it takes the style's flute center, as does any other lead instrument.
   An older engine (no style; Task 6 removes them) plans over its own floor (a 4th over the keys' top); its center
   sets only the keys' window: its median lead pitch over percussive keys before the band (1500 songs, lead styles
   Task 1: flute 77, felt piano 75, vibes 77, Knight 75; the synth pluck and Emilyguitar 75, the saw lead 77: another
   instrument 76).

                      flute  felt   vibes  Knight  other   (notes measured: flute / felt / vibes)
     Impressionism     79     80     76     79      79      119 /  80 / 49
     Romanticism       80     78     78     80      80      164 / 104 / 47
     '60s Jazz         80     77     78     80      80      112 /  55 / 45
     Modal Jazz        77     82     76     77      77       89 /  51 / 38
     Jazz-Hop          76     80     76     76      76      105 /  63 / 40
     Film Score        78     80     76     78      78      139 /  79 / 46
     Bossa Nova        82     80     79     82      82      129 /  81 / 48
     Ambient           77     82     80     77      77      110 /  85 / 38
     (no style)        77     75     77     75      76                                                        */
static const uint8_t CENTER[LS_N + 1][5] = {
    {79, 80, 76, 79, 79}, {80, 78, 78, 80, 80}, {80, 77, 78, 80, 80}, {77, 82, 76, 77, 77},
    {76, 80, 76, 76, 76}, {78, 80, 76, 78, 78}, {82, 80, 79, 82, 82}, {77, 82, 80, 77, 77},
    {77, 75, 77, 75, 76},
};

int leads_center(int style, int ins)
{
    int col = ins == INS_FLUTE ? 0 : ins == INS_FELT_PIANO ? 1 : ins == INS_VIBES ? 2 : ins == INS_KNIGHT ? 3 : 4;
    int c = CENTER[style >= 0 && style < LS_N ? style : LS_N][col], lo, hi;
    song_ov_range(ins, &lo, &hi);
    return c < lo + 6 ? lo + 6 : c > hi - 6 ? hi - 6 : c;
}

void leads_band(const song_roll_t *r, int *lo, int *hi)
{
    const lib_set_t *set = lib_set_by_id((uint8_t)r->v[RF_SET]);
    int ins = set ? set->lead : INS_FLUTE, c = leads_center(leads_style(r), ins), il, ih;
    song_ov_range(ins, &il, &ih);
    c += 12 * song_ov()->lead_oct;                       /* the workbench's octave, in the instrument's range */
    if (c < il + 6) c = il + 6;
    if (c > ih - 6) c = ih - 6;
    *lo = c - 6;
    *hi = c + 6;
}

void leads_range_bar(const song_roll_t *r, const int keys[4], const int nkeys[4], int *lo, int *lo_x, int *hi)
{
    const lib_set_t *set = lib_set_by_id((uint8_t)r->v[RF_SET]);
    int top = 0, ntop = 0, i, blo, bhi, il, ih, ko = harmony_keys_oct(r);
    for (i = 0; i < 4; i++) {
        if (keys[i] > top) top = keys[i];
        if (nkeys[i] > ntop) ntop = nkeys[i];
    }
    song_ov_range(set ? set->lead : INS_FLUTE, &il, &ih);
    leads_band(r, &blo, &bhi);
    *lo = top >= blo ? top + 5 : blo;                    /* the keys reach in: a 4th over them */
    *lo_x = ntop >= blo && ntop + 5 > *lo ? ntop + 5 : *lo;
    *hi = 88 + 12 * song_ov()->lead_oct;                 /* E6, or the workbench's octave off it */
    if (ko > 0 && 88 + ko > *hi) *hi = 88 + ko;          /* the workbench's keys raised: the ceiling with them */
    if (*hi > ih) *hi = ih > 88 ? ih : 88;               /* in the instrument's range (or E6 at least: as before) */
    if (*hi < bhi) *hi = bhi;                            /* an octave down: the band's top at least */
}

/* the styles' planners, by LS_* (each style's file registers its own; none yet: the older engines plan) */
typedef void leads_plan_fn(lead_state_t *ls, const song_roll_t *r, const plan_block_t *b, int bar, const int keys[4],
                           const int nkeys[4], const int *pcs, int np, int can_push);
static leads_plan_fn *const PLAN[LS_N] = {0};

int leads_plan_bar(lead_state_t *ls, const song_roll_t *r, const plan_block_t *b, int bar, const int keys[4],
                   const int nkeys[4], const int *pcs, int np, int can_push)
{
    int s = leads_style(r);
    if (s < 0 || !PLAN[s]) return 0;
    PLAN[s](ls, r, b, bar, keys, nkeys, pcs, np, can_push);
    return 1;
}

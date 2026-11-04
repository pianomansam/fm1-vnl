/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <string.h>
#include "pads.h"
#include "library.h"
#include "plan.h"
#include "rng.h"
#include "theory.h"
#include "override.h"

enum { LO = 55, HI = 72 };

int pads_solo_voice(const song_roll_t *r)
{
    static const int VOICE[4] = {INS_CHOIR, INS_FLUTE, INS_ORGAN, INS_STRINGS};
    const lib_set_t *set = lib_set_by_id((uint8_t)r->v[RF_SET]);
    int ok[4], n = 0, i;
    rng_t g;
    rng_seed(&g, song_ov_salted(r->seed, song_ov()->pad_salt), 191);
    if (rng_int(&g, 100) < 40) return INS_NONE;           /* a song with no solo note */
    for (i = 0; i < 4; i++)                              /* the flute over a flute lead, the organ over organ keys */
        if (!set || !((VOICE[i] == INS_FLUTE && set->lead == INS_FLUTE) ||
                      (VOICE[i] == INS_ORGAN && set->chords == INS_ORGAN)))
            ok[n++] = VOICE[i];
    return ok[rng_int(&g, n)];
}

int pads_section_part(const song_roll_t *r, int block, int section, int sustained_keys)
{
    /* the solo note's share by SEC_INTRO, GROOVE, LEAD, BREAK, OUTRO: over struck keys (the chord part otherwise),
       over sustained keys (nothing otherwise: the first design's shares) */
    static const uint8_t STRUCK[5] = {35, 25, 15, 35, 30}, SUSTAINED[5] = {50, 60, 30, 50, 50};
    int other = sustained_keys ? 0 : 1;
    rng_t g;
    if (section < 0 || section > SEC_OUTRO || pads_solo_voice(r) == INS_NONE) return other;
    rng_seed(&g, song_ov_salted(r->seed, song_ov()->pad_salt) ^ (uint32_t)(block + 1) * 0x2C1B3C6Du, 190);
    return rng_int(&g, 100) < (sustained_keys ? SUSTAINED : STRUCK)[section] ? 2 : other;
}

int song_pad_in(const song_roll_t *r, const song_plan_t *p, int block)
{
    /* the pad's chance in a section by SEC_INTRO, GROOVE, LEAD, BREAK, OUTRO: likelier out in intros and breaks */
    static const uint8_t IN[5] = {45, 75, 85, 40, 60};
    uint32_t seed = song_ov_salted(r->seed, song_ov()->pad_salt);
    int i, in = 1, out = 0;
    if (block < 0 || block >= p->n) return 0;
    for (i = 0; i <= block; i++) {                       /* the run of sections without it so far */
        int sec = p->b[i].section;
        rng_t g;
        rng_seed(&g, song_ov_salted(seed, (uint32_t)i + 1u), 196);   /* hashed apart from stream 190's blocks */
        in = out >= 2 || rng_int(&g, 100) < IN[sec <= SEC_OUTRO ? sec : SEC_GROOVE];
        out = in ? 0 : out + 1;
    }
    return in;
}

int pads_solo_lo(int voice) { return voice == INS_STRINGS ? 84 : 72; }

int pads_rub(int a, int b)
{
    int d = a > b ? a - b : b - a;
    return d % 12 == 1;
}

static int mod12(int v) { return (v % 12 + 12) % 12; }
static int absi(int v) { return v < 0 ? -v : v; }

/* a note the others leave room for: no rub with the keys, the lead, or a minor 9th over the bass */
static int clear_of(int n, const int keys[4], const int *lead, int nlead, int bass_pc)
{
    int i;
    for (i = 0; i < 4; i++)
        if (keys[i] && pads_rub(n, keys[i])) return 0;
    for (i = 0; i < nlead; i++)
        if (lead[i] && pads_rub(n, lead[i])) return 0;
    return bass_pc < 0 || mod12(n - bass_pc) != 1;
}

/* the cost of moving from prev (by lane) to the notes t (ascending, m of them) on lanes ln: each lane's move, the top
   voice's 8 a semitone (and 6 for a leap), 6 for a common tone that moved, and 1000 for a common pitch on another lane
   (it would strike again rather than ring on: never chosen, as a lane order that keeps it always exists). A step
   glides on its own lane: 100 for a glide over 4 semitones (a leap strikes fresh on the free lane instead), 400 for
   each new note within 4 semitones of a lane left releasing, or rubbing it (its tail would smear under the swell), 3
   for any lane left releasing. With no prev, the top near G4 */
static int move_cost(const int prev[3], const int *t, const int *ln, int m)
{
    int c = 0, k, j, ptop = 0, have = 0;
    for (k = 0; k < 3; k++) {
        if (prev[k] > ptop) ptop = prev[k];
        have |= prev[k] != 0;
    }
    if (!have) return 2 * absi(t[m - 1] - 67) + (t[m - 1] - t[0] > 12 ? 2 * (t[m - 1] - t[0] - 12) : 0);
    for (k = 0; k < m; k++)
        if (prev[ln[k]]) c += absi(t[k] - prev[ln[k]]);
    k = absi(t[m - 1] - ptop);
    c += 8 * k + (k > 2 ? 6 : 0);
    for (j = 0; j < 3; j++) {
        int in = 0, at = 0;
        if (!prev[j]) continue;
        for (k = 0; k < m; k++) {
            in |= mod12(t[k] - prev[j]) == 0;
            at |= t[k] == prev[j];
        }
        if (in && !at) c += 6;
        for (k = 0; k < m; k++)
            if (t[k] == prev[j] && ln[k] != j) c += 1000;
    }
    for (k = 0; k < m; k++)                              /* a glide over 4 semitones: a fresh strike instead */
        if (prev[ln[k]] && absi(t[k] - prev[ln[k]]) > 4) c += 100;
    for (j = 0; j < 3; j++) {                            /* a lane left to release under the new chord */
        int held = 0;
        if (!prev[j]) continue;
        for (k = 0; k < m; k++) held |= ln[k] == j;
        if (held) continue;
        for (k = 0; k < m; k++)                          /* a step taken there swells beside the old tail: glide it */
            if (t[k] != prev[ln[k]] && (absi(t[k] - prev[j]) <= 4 || pads_rub(t[k], prev[j]))) c += 400;
        c += 3;
    }
    return c;
}

/* the best chord part so far: its cost and its notes by lane */
typedef struct { int cost, lane[3]; } pick_t;

/* notes v (ascending, m of them; penalties pen) as a chord part: legal among themselves, scored, kept if best */
static void consider(pick_t *b, const int *v, const int *pen, int m, int root, int have3, int have7,
                     const int prev[3])
{
    /* the lanes each note may take (notes ascending): any order, so common pitches keep their lanes */
    static const int LN2[6][2] = {{0, 1}, {0, 2}, {1, 2}, {1, 0}, {2, 0}, {2, 1}};
    static const int LN3[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    int i, j, c = 0, g3 = 0, g7 = 0;
    for (i = 0; i < m; i++) {
        int iv = mod12(v[i] - root);
        for (j = 0; j < i; j++)
            if (mod12(v[i] - v[j]) == 0 || pads_rub(v[i], v[j])) return;
        g3 |= iv == 3 || iv == 4;
        g7 |= iv == 10 || iv == 11;
        c += 20 * pen[i];
    }
    c += (have3 && !g3 ? 60 : 0) + (have7 && !g7 ? 40 : 0);   /* the guide tones first */
    for (i = 0; i < 6; i++) {
        const int *ln = m == 3 ? LN3[i] : LN2[i];
        int cc = c + move_cost(prev, v, ln, m);
        if (cc < b->cost) {
            b->cost = cc;
            b->lane[0] = b->lane[1] = b->lane[2] = 0;
            for (j = 0; j < m; j++) b->lane[ln[j]] = v[j];
        }
    }
}

int pads_chord(const song_roll_t *r, const int keys[4], const int *pcs, int np, unsigned bass_pc,
               const int *lead_notes, int nlead, const int prev[3], int out[3])
{
    int cand[24], pen[24], nc = 0, have3 = 0, have7 = 0, p[3], i, j, k, m;
    pick_t b;
    (void)r;
    memcpy(p, prev, sizeof p);
    b.cost = 1 << 30;
    for (k = LO; k <= HI; k++)                           /* the chord's tones but the root (the bass's) */
        for (i = 1; i < np && nc < 24; i++) {
            int iv = mod12(pcs[i] - pcs[0]);
            have3 |= k == LO && (iv == 3 || iv == 4);
            have7 |= k == LO && (iv == 10 || iv == 11);
            if (mod12(k) != mod12(pcs[i]) || !clear_of(k, keys, lead_notes, nlead, (int)bass_pc)) continue;
            /* 0 a guide tone, 1 the 5th or 9th, 2 another color (an 11th, a 13th) */
            pen[nc] = iv == 3 || iv == 4 || iv == 10 || iv == 11 ? 0 : iv == 2 || (iv >= 6 && iv <= 8) ? 1 : 2;
            cand[nc++] = k;
        }
    for (i = 0; i < nc; i++)                             /* 2 notes (the budget's cut (c)): the guide tones first */
        for (j = i + 1; j < nc; j++) {
            int v[2], w[2];
            v[0] = cand[i]; v[1] = cand[j];
            w[0] = pen[i]; w[1] = pen[j];
            consider(&b, v, w, 2, pcs[0], have3, have7, p);
        }
    if (b.cost == 1 << 30) {
        out[0] = out[1] = out[2] = 0;
        return 0;
    }
    for (i = m = 0; i < 3; i++) {
        out[i] = b.lane[i];
        m += out[i] != 0;
    }
    return m;
}

int pads_double_keys(const int keys[4], int out[3])
{
    int v[4], n = 0, i, j, t, nl[3] = {0, 0, 0}, used[3] = {0, 0, 0};
    for (i = 0; i < 4; i++)
        if (keys[i] >= LO && keys[i] <= HI) v[n++] = keys[i];
    for (i = 1; i < n; i++)                              /* ascending */
        for (j = i; j > 0 && v[j - 1] > v[j]; j--) { t = v[j]; v[j] = v[j - 1]; v[j - 1] = t; }
    if (n > 3) { v[0] = v[1]; v[1] = v[2]; v[2] = v[3]; n = 3; }
    for (i = 0; i < n; i++)                              /* a common pitch rings on in its lane */
        for (j = 0; j < 3; j++)
            if (!nl[j] && out[j] == v[i]) { nl[j] = v[i]; used[i] = 1; break; }
    for (i = 0; i < n; i++)                              /* the rest by voice position, into the free lanes */
        if (!used[i])
            for (j = i < 3 ? i : 0; ; j = (j + 1) % 3)
                if (!nl[j]) { nl[j] = v[i]; break; }
    for (i = 0; i < 3; i++) out[i] = nl[i];
    return n;
}

int pads_solo(const song_roll_t *r, int key, int mode, const int *pcs, int np, const int keys[4], const int *bass,
              int nbass, int chord_top, int lo, int prev)
{
    static const int ORDER[4] = {0, 2, 1, 6};           /* the tonic, the 3rd, the 2nd, the 7th (scale indices) */
    uint16_t pool = th_mode_pool(mode);
    int off[7], n = 0, i, o, best = 0, bc = 1 << 30, ref, b7 = th_dom_b7(pcs, np);
    (void)r;
    for (i = 0; i < 12 && n < 7; i++)
        if ((pool >> i) & 1) off[n++] = i;
    if (n < 7) return 0;
    ref = prev ? prev : lo + mod12(key - lo);           /* no note yet: from the tonic */
    for (o = 0; o < 4; o++)
        for (i = lo + mod12(key + off[ORDER[o]] - lo); i <= lo + 12; i += 12) {
            int k, c;
            if ((chord_top && i < chord_top + 3) || th_is_avoid(key, mode, pcs, np, i)) continue;
            if (mod12(i) == b7) continue;               /* never a dominant's 7th (Sam: it sounds cheesy) */
            for (k = 0; k < 4; k++)
                if (keys[k] && pads_rub(i, keys[k])) break;
            if (k < 4) continue;
            for (k = 0; k < nbass; k++) {               /* the bass rule: a major 7th over the root is a maj7 chord */
                int d = mod12(i - bass[k]);
                if (d == 1 || (d == 11 && (np <= 0 || mod12(bass[k]) != mod12(pcs[0])))) break;
            }
            if (k < nbass) continue;
            c = 4 * absi(i - ref) + o;
            if (c < bc) { bc = c; best = i; }
        }
    return best;
}

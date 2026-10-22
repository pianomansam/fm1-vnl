/* SPDX-License-Identifier: MPL-2.0 */
/* The melody. Development after the idea of Tuna's motif handling (stated, moved onto the next chord with the same
   shape, given a new ending, restated), reimplemented from the spec; no code from it.

   Legality is held as pitch-class masks per bar of a phrase: `weak` (in scale, not an avoid note under the bar's
   chord) for any step, `safe` (no semitone from any chord pitch class: the voicing is 4 of them, chosen elsewhere, so
   this covers whatever sounds, the root under a rootless maj7 included), `strong` (weak, safe and a chord tone) for
   steps 0 and 8, and `bound`, the step at which the next chord change (or the region's end) falls. A note held into
   step 0 or 8 must be safe there, or it ends before it. Every note is picked from those masks, so no rule can be broken; the
   motif uses masks intersected over every lead phrase of the song, so an exact restatement is legal anywhere. */
#include <stdint.h>
#include <string.h>
#include "melody.h"
#include "rng.h"
#include "theory.h"
#include "library.h"
#include "harmony.h"

#define MAXN 24                                    /* notes in one realized phrase */
#define MAXP 32                                    /* lead phrases per song */

enum { R_NONE, R_LEAD, R_ANSWER, R_SPARSE };       /* what plays in a bar */

typedef struct { uint8_t at, len, note; } nt_t;    /* onset and length in 16ths from the phrase start; MIDI */

typedef struct {
    const song_roll_t *r;
    const song_plan_t *p;
    int key, mode, bpc, loop;
    int off[7];                                    /* the mode's semitone offsets */
    uint16_t scale, pent;                          /* absolute pitch-class masks */
} ctx_t;

/* Legal pitch classes per bar of a region (up to 4 bars), and where each bar's notes must end. */
typedef struct { uint16_t weak[4], strong[4], ct[4], safe[4]; int8_t g3[4], g7[4]; uint8_t bound[4]; } region_t;

static uint32_t mix(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 1) * 0x85EBCA6Bu ^ (c + 1) * 0xC2B2AE35u ^ (d + 1) * 0x27D4EB2Fu;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int mod12(int x) { x %= 12; return x < 0 ? x + 12 : x; }
static uint16_t rot(uint16_t m, int k)             /* a mask relative to the key, made absolute */
{
    k = mod12(k);
    return (uint16_t)(((m << k) | (m >> (12 - k))) & 0xFFFu);
}

static void ctx_init(ctx_t *c, const song_roll_t *r, const song_plan_t *p)
{
    static const uint16_t PENT_MAJ = 1u | 1u << 2 | 1u << 4 | 1u << 7 | 1u << 9;
    static const uint16_t PENT_MIN = 1u | 1u << 3 | 1u << 5 | 1u << 7 | 1u << 10;
    const lib_prog_t *pr = lib_prog_by_id((uint8_t)r->v[RF_PROG]);
    uint16_t pool;
    int i, n = 0;
    if (!pr) pr = &LIB_PROGS[0];
    c->r = r;
    c->p = p;
    c->key = mod12(r->v[RF_KEY]);
    c->mode = r->v[RF_MODE];
    c->bpc = pr->bars_per_chord ? pr->bars_per_chord : 1;
    c->loop = pr->n * c->bpc;
    if (c->loop <= 0) c->loop = 4;
    pool = th_mode_pool(c->mode);
    for (i = 0; i < 12 && n < 7; i++)
        if ((pool >> i) & 1) c->off[n++] = i;
    while (n < 7) { c->off[n] = c->off[n - 1]; n++; }
    c->scale = rot(pool, c->key);
    c->pent = rot((pool >> 4) & 1 ? PENT_MAJ : PENT_MIN, c->key) & c->scale;
}

/* scale-degree index of an in-scale note (octave * 7 + position), and back */
static int deg_of(const ctx_t *c, int note)
{
    int d = note - c->key + 12, pc, i;
    if (d < 0) d = 0;
    pc = d % 12;
    for (i = 6; i > 0 && c->off[i] > pc; i--) {}
    return d / 12 * 7 + i;
}

static int note_of(const ctx_t *c, int deg)
{
    if (deg < 0) deg = 0;
    return c->key - 12 + deg / 7 * 12 + c->off[deg % 7];
}

static int lead_lo(const song_roll_t *r) { return 64 + LIB_MOODS[clampi(r->v[RF_MOOD], 0, 3)].lead_shift; }

/* masks of one bar at song bar `bar`; bound relative to `origin` (the region's first bar) in 16ths */
static void bar_masks(const ctx_t *c, int bar, int origin, region_t *R, int k)
{
    const plan_block_t *b = plan_at(c->p, bar);
    int pcs[5], n, i, pass = bar / c->loop + 1, t3, t7;
    uint16_t ct = 0, weak = 0, safe;
    n = harmony_chord_at(c->r, bar, pass, b && (b->flags & PB_DARK), pcs);
    for (i = 0; i < n; i++) ct |= (uint16_t)(1u << mod12(pcs[i]));
    for (i = 0; i < 12; i++)
        if (((c->scale >> i) & 1) && !th_is_avoid(c->key, c->mode, pcs, n, i)) weak |= (uint16_t)(1u << i);
    safe = (uint16_t)(~(rot(ct, 1) | rot(ct, -1)) & 0xFFFu);
    th_guide_tones(pcs, n, &t3, &t7);
    R->weak[k] = weak;
    R->safe[k] = safe;
    R->strong[k] = weak & ct & safe;
    R->ct[k] = ct;
    R->g3[k] = (int8_t)t3;
    R->g7[k] = (int8_t)t7;
    R->bound[k] = (uint8_t)clampi(((bar / c->bpc + 1) * c->bpc - origin) * 16, 0, 255);
}

/* masks of `bars` bars from song bar P; region_and narrows one region by another (the motif's legality) */
static void region_at(const ctx_t *c, int P, int bars, region_t *R)
{
    int k;
    memset(R, 0, sizeof *R);
    for (k = 0; k < bars && k < 4; k++) bar_masks(c, P + k, P, R, k);
}

static void region_and(region_t *a, const region_t *b, int bars)
{
    int k;
    for (k = 0; k < bars; k++) {
        a->weak[k] &= b->weak[k];
        a->strong[k] &= b->strong[k];
        a->safe[k] &= b->safe[k];
        if (b->bound[k] < a->bound[k]) a->bound[k] = b->bound[k];
    }
}

static uint16_t mask_at(const region_t *R, int t)
{
    int s = t % 16;
    return (s == 0 || s == 8) ? R->strong[t / 16] : R->weak[t / 16];
}

/* the longest a note from t may be held without sounding unsafe on a later step 0 or 8: at most len */
static int hold_len(const region_t *R, int t, int len, int note)
{
    int s;
    for (s = (t / 8 + 1) * 8; s < t + len && s < 64; s += 8)
        if (!((R->safe[s / 16] >> mod12(note)) & 1)) return s - t;
    return len;
}

static int legal(const region_t *R, int t, int len, int note)
{
    return t < 64 && ((mask_at(R, t) >> mod12(note)) & 1) && t + len <= R->bound[t / 16] && len > 0 &&
           hold_len(R, t, len, note) == len;
}

/* the note in [lo, hi] nearest to target whose pitch class is in mask; -1 if none */
static int pick_near(uint16_t mask, int target, int lo, int hi)
{
    int d, t;
    for (d = 0; d <= hi - lo + 12; d++) {
        t = target + d;
        if (t >= lo && t <= hi && ((mask >> mod12(t)) & 1)) return t;
        t = target - d;
        if (t >= lo && t <= hi && ((mask >> mod12(t)) & 1)) return t;
    }
    return -1;
}

/* the length a note may have: up to `want` 16ths, never past its bar's bound or `end` */
static int fit_len(const region_t *R, int t, int want, int end)
{
    int lim = R->bound[t / 16];
    if (end < lim) lim = end;
    return clampi(want, 0, lim - t);
}

/* ---- figures ---- */

/* one-beat rhythm cells: bit i = an onset on 16th i of the beat */
static const uint8_t CELLS[] = {0x1, 0x4, 0x5, 0x9, 0x8, 0x6, 0xD};
static const uint8_t CELL_W[] = {3, 3, 3, 1, 2, 1, 1};
static const int8_t MOVES[] = {-3, -2, -1, 0, 1, 2, 3};
static const uint8_t MOVE_W[] = {1, 3, 4, 1, 4, 3, 1};

static int next_pitch(const ctx_t *c, rng_t *g, const region_t *R, int t, int prev, int lo, int hi)
{
    int center = (lo + hi) / 2, mv = MOVES[rng_weighted(g, MOVE_W, 7)], tgt, n;
    if ((prev > center + 6 && mv > 0) || (prev < center - 6 && mv < 0)) mv = -mv;
    tgt = note_of(c, deg_of(c, prev) + mv);
    n = pick_near(mask_at(R, t) & c->pent, tgt, lo, hi);       /* pentatonic-leaning */
    if (n < 0 || n - tgt > 2 || tgt - n > 2) n = pick_near(mask_at(R, t), tgt, lo, hi);
    return n;
}

static int figure_bars(rng_t *g) { return rng_int(g, 20) < 13 ? 2 : 1; }   /* 2 bars about two times in three */

/* A fresh figure of mb bars: rhythm from cells (about a third of beats played), first note a guide tone of the
   chord under it, then stepwise and pentatonic-leaning. Notes with no legal pitch are left out. */
static int gen_figure(const ctx_t *c, rng_t *g, const region_t *R, int mb, int lo, int hi, nt_t *out)
{
    int at[32], na = 0, beats = 4 * mb, played, i, b, s, tries, n = 0, prev = -1;
    for (tries = 0; tries < 8 && na < 3; tries++) {
        int free[8], nf = beats, roll = rng_int(g, 20);
        na = 0;
        played = mb == 1 ? 2 : (roll < 12 ? 3 : roll < 17 ? 2 : 4);   /* about a third of beats */
        for (i = 0; i < beats; i++) free[i] = i;
        {
            uint8_t used[8] = {0, 0, 0, 0, 0, 0, 0, 0};
            for (i = 0; i < played; i++) {
                int j = (i == 0 && rng_int(g, 10) < 6) ? 0 : rng_int(g, nf);
                used[free[j]] = 1;
                free[j] = free[--nf];
            }
            for (b = 0; b < beats; b++) {
                int cell;
                if (!used[b]) continue;
                cell = CELLS[rng_weighted(g, CELL_W, (int)sizeof CELLS)];
                for (s = 0; s < 4; s++)
                    if ((cell >> s) & 1) at[na++] = b * 4 + s;
            }
        }
    }
    if (na > 8) na = 8;
    for (i = 0; i < na; i++) {
        int t = at[i], want = i + 1 < na ? at[i + 1] - t : 4 + rng_int(g, 3), len, note = -1;
        if (want > 6) want = 6;
        len = fit_len(R, t, want, mb * 16);
        if (len <= 0) continue;
        if (prev < 0) {
            int k = t / 16, first = rng_int(g, 2), center = (lo + hi) / 2 - 2, j;
            for (j = 0; j < 2 && note < 0; j++) {
                int gt = (j ^ first) ? R->g7[k] : R->g3[k];
                if (gt >= 0 && ((mask_at(R, t) >> gt) & 1)) note = pick_near((uint16_t)(1u << gt), center, lo, hi);
            }
            if (note < 0) note = pick_near(mask_at(R, t) & R->ct[k], center, lo, hi);
            if (note < 0) note = pick_near(mask_at(R, t), center, lo, hi);
        } else {
            note = next_pitch(c, g, R, t, prev, lo, hi);
        }
        if (note < 0) continue;
        out[n].at = (uint8_t)t;
        out[n].len = (uint8_t)hold_len(R, t, len, note);
        out[n].note = (uint8_t)note;
        prev = note;
        n++;
    }
    return n;
}

/* The figure h moved to start at `base` 16ths later: same rhythm and contour (in scale steps), starting on the
   chord tone nearest its start under the chord there, with a new last 1-2 notes. */
static int vary(const ctx_t *c, rng_t *g, const region_t *R, const nt_t *h, int hn, int base, int end, int lo,
                int hi, nt_t *out)
{
    int i, n = 0, d0, start, newend, prev = -1;
    if (hn <= 0) return 0;
    {
        int t = base + h[0].at;
        if (t >= 64) return 0;
        start = pick_near(mask_at(R, t) & R->ct[t / 16], h[0].note, lo, hi);
        if (start < 0) start = pick_near(mask_at(R, t), h[0].note, lo, hi);
        if (start < 0) return 0;
    }
    d0 = deg_of(c, h[0].note);
    newend = hn >= 3 ? 1 + rng_int(g, 2) : hn - 1;
    for (i = 0; i < hn; i++) {
        int t = base + h[i].at, len, tgt, note;
        if (t >= end) break;
        len = fit_len(R, t, h[i].len, end);
        if (len <= 0) continue;
        tgt = note_of(c, deg_of(c, start) + deg_of(c, h[i].note) - d0);
        if (i == 0) {
            note = start;
        } else if (i >= hn - newend && prev >= 0) {
            note = next_pitch(c, g, R, t, prev, lo, hi);       /* the new ending */
            if (note == tgt) note = pick_near(mask_at(R, t), note_of(c, deg_of(c, tgt) + (tgt > prev ? -1 : 1)),
                                              lo, hi);
        } else {
            note = pick_near(mask_at(R, t), tgt, lo, hi);
        }
        if (note < 0) continue;
        out[n].at = (uint8_t)t;
        out[n].len = (uint8_t)hold_len(R, t, len, note);
        out[n].note = (uint8_t)note;
        prev = note;
        n++;
    }
    return n;
}

/* ---- the song's lead phrases ---- */

/* Start bars of the lead phrases in song order; returns the count (at most MAXP). */
static int lead_starts(const song_plan_t *p, int *starts)
{
    int i, j, n = 0;
    for (i = 0; i < p->n; i++) {
        const plan_block_t *b = &p->b[i];
        if (!(b->layers & L_LEAD)) continue;
        for (j = 0; j < b->bars / 4 && n < MAXP; j++)
            if (!(b->layers & L_CANDY) || (j & 1) == 0) starts[n++] = b->start + 4 * j;
    }
    return n;
}

/* What plays at `bar`: R_LEAD (lead phrase *idx from *P), R_ANSWER (candy phrase from *P), R_SPARSE or R_NONE. */
static int role_at(const song_plan_t *p, int bar, int *P, int *idx)
{
    int i, j, n = 0;
    for (i = 0; i < p->n; i++) {
        const plan_block_t *b = &p->b[i];
        int in = bar >= b->start && bar < b->start + b->bars;
        if (!(b->layers & L_LEAD)) {
            if (in) return (b->layers & L_CANDY) ? R_SPARSE : R_NONE;
            continue;
        }
        for (j = 0; j < b->bars / 4; j++) {
            int lead = !(b->layers & L_CANDY) || (j & 1) == 0;
            if (in && bar >= b->start + 4 * j && bar < b->start + 4 * j + 4) {
                *P = b->start + 4 * j;
                *idx = n;
                return lead ? (n < MAXP ? R_LEAD : R_NONE) : R_ANSWER;
            }
            if (lead) n++;
        }
        if (in) return R_NONE;
    }
    return R_NONE;
}

/* the motif's notes as played from the same first note */
static int motif_notes(const ctx_t *c, const motif_t *m, nt_t *out)
{
    int i, d0 = deg_of(c, m->first);
    for (i = 0; i < m->n && i < 16; i++) {
        out[i].at = m->at[i];
        out[i].len = m->len[i];
        out[i].note = (uint8_t)clampi(note_of(c, d0 + m->step[i]), 0, 127);
    }
    return i;
}

static int motif_fits(const ctx_t *c, const motif_t *m, int P)
{
    region_t R;
    nt_t h[16];
    int i, hn = motif_notes(c, m, h);
    region_at(c, P, 2, &R);
    for (i = 0; i < hn; i++)
        if (!legal(&R, h[i].at, h[i].len, h[i].note)) return 0;
    return 1;
}

void melody_motif(const song_roll_t *r, uint32_t salt, motif_t *m)
{
    song_plan_t p;
    plan_build(r, &p);
    melody_motif_for(r, &p, salt, m);
}

void melody_motif_for(const song_roll_t *r, const song_plan_t *p, uint32_t salt, motif_t *m)
{
    ctx_t c;
    int starts[MAXP], np, set, attempt, lo, i;
    ctx_init(&c, r, p);
    np = lead_starts(p, starts);
    if (np == 0) { starts[0] = 0; np = 1; }
    lo = lead_lo(r);
    memset(m, 0, sizeof *m);
    /* legal under every lead phrase's chords; failing that, under the first and last; failing that, the first */
    for (set = 0; set < 3; set++) {
        region_t R, Q;
        int k;
        region_at(&c, starts[0], 2, &R);
        for (k = 1; k < np; k++)
            if (set == 0 || (set == 1 && k == np - 1)) {
                region_at(&c, starts[k], 2, &Q);
                region_and(&R, &Q, 2);
            }
        for (attempt = 0; attempt < 24; attempt++) {
            rng_t g;
            nt_t f[16];
            int n;
            rng_seed(&g, mix(salt, (uint32_t)set, (uint32_t)attempt, 0), 130);
            n = gen_figure(&c, &g, &R, figure_bars(&g), lo, lo + 20, f);
            if (n > 0 && n >= m->n) {                      /* keep the fullest figure as a fallback */
                m->n = (uint8_t)n;
                m->first = f[0].note;
                for (i = 0; i < n; i++) {
                    m->at[i] = f[i].at;
                    m->len[i] = f[i].len;
                    m->step[i] = (int8_t)(deg_of(&c, f[i].note) - deg_of(&c, f[0].note));
                }
            }
            if (n >= 3) return;
        }
    }
}

void melody_plan(const song_roll_t *r, const song_plan_t *p, melody_phrases_t *ph)
{
    motif_t m;
    melody_motif(r, r->seed, &m);
    melody_plan_for(r, p, &m, ph);
}

void melody_plan_for(const song_roll_t *r, const song_plan_t *p, const motif_t *m, melody_phrases_t *ph)
{
    ctx_t c;
    rng_t g;
    int starts[MAXP], np, i, ok[MAXP], mid, ns, nv, slot[MAXP];
    ctx_init(&c, r, p);
    memset(ph, PH_NEW, sizeof *ph);
    np = lead_starts(p, starts);
    if (np == 0) return;
    rng_seed(&g, r->seed, 131);
    for (i = 0; i < np; i++) ok[i] = motif_fits(&c, m, starts[i]);
    ph->kind[0] = PH_STATE;
    ph->kind[np - 1] = PH_STATE;
    mid = np - 2;
    if (mid <= 0) return;
    /* song-wide totals toward 50 / 15 / 35: restatements beyond the two heads, then variants, by stochastic
       rounding; the ends already restate, so short songs lean on new phrases */
    ns = (int)((float)np * 0.5f - 2.0f + rng_float(&g));
    nv = (int)((float)np * 0.15f + rng_float(&g));
    ns = clampi(ns, 0, mid);
    nv = clampi(nv, 0, mid - ns);
    for (i = 0; i < mid; i++) slot[i] = i + 1;
    for (i = mid - 1; i > 0; i--) {
        int j = rng_int(&g, i + 1), t = slot[i];
        slot[i] = slot[j];
        slot[j] = t;
    }
    for (i = 0; i < mid; i++) {
        int k = slot[i];
        if (ns > 0 && ok[k]) { ph->kind[k] = PH_STATE; ns--; }
        else if (nv > 0) { ph->kind[k] = PH_VARIANT; nv--; }
    }
}

/* ---- realizing phrases ---- */

/* The notes of lead phrase idx from bar P: head (motif, its variant, or a new figure) and answer. */
static int lead_phrase(const ctx_t *c, const melody_phrases_t *ph, const motif_t *m, int P, int idx, uint32_t salt,
                       nt_t *out)
{
    region_t R;
    nt_t h[16];
    rng_t g;
    int kind = ph->kind[idx], lo = lead_lo(c->r), hi = lo + 20, hn = 0, n, i, key;
    region_at(c, P, 4, &R);
    rng_seed(&g, mix(salt, (uint32_t)idx, 1, 0), 132);
    if (kind == PH_STATE) {
        nt_t mm[16];
        int mn = motif_notes(c, m, mm);
        for (i = 0; i < mn; i++)
            if (legal(&R, mm[i].at, mm[i].len, mm[i].note)) h[hn++] = mm[i];
    } else if (kind == PH_VARIANT) {
        nt_t mm[16];
        int mn = motif_notes(c, m, mm);
        hn = vary(c, &g, &R, mm, mn, 0, 32, lo, hi, h);
    } else {
        hn = gen_figure(c, &g, &R, figure_bars(&g), lo, hi, h);
    }
    memcpy(out, h, sizeof(nt_t) * (size_t)hn);
    n = hn;
    /* the answer: the head's rhythm from bar 3, moved onto that chord, new ending; a rest one time in four.
       Restatements share one answer seed, so they repeat it too where the chords repeat. */
    key = kind == PH_STATE ? 0 : idx + 1;
    rng_seed(&g, mix(salt, (uint32_t)key, 2, 0), 133);
    if (rng_int(&g, 4) != 0) n += vary(c, &g, &R, h, hn, 32, 64, lo, hi, out + n);
    return n;
}

/* A candy figure: up to cnt notes from 16th t, 2 or 4 apart (rising when `up`), the first a chord tone near
   `target`, all ending by `end`; appended at out + n. Returns the new count. */
static int candy_figure(const ctx_t *c, rng_t *g, const region_t *R, int t, int cnt, int end, int target, int up,
                        nt_t *out, int n)
{
    int i, prev = -1;
    for (i = 0; i < cnt && t < end; i++) {
        int gap = 2 + 2 * rng_int(g, 2), len, note;
        len = fit_len(R, t, i + 1 < cnt ? gap : 4 + rng_int(g, 3), end);
        if (prev < 0) note = pick_near(mask_at(R, t) & R->ct[t / 16], target, 72, 96);
        else if (up) note = pick_near(mask_at(R, t), note_of(c, deg_of(c, prev) + 1 + rng_int(g, 2)), prev + 1, 96);
        else note = next_pitch(c, g, R, t, prev, 72, 96);
        if (len > 0 && note >= 0) {
            out[n].at = (uint8_t)t;
            out[n].len = (uint8_t)hold_len(R, t, len, note);
            out[n].note = (uint8_t)note;
            prev = note;
            n++;
        }
        t += gap;
    }
    return n;
}

/* Ear candy answering the lead in the candy's 4-bar phrase from bar Q: 1-4 notes early on, and about half the time
   a second, shorter answer in bar 3. */
static int answer_phrase(const ctx_t *c, int Q, uint32_t salt, nt_t *out)
{
    region_t R;
    rng_t g;
    int n, t, cnt, tgt;                            /* draws in a fixed order: argument order is unspecified */
    region_at(c, Q, 4, &R);
    rng_seed(&g, mix(salt, (uint32_t)Q, 3, 0), 134);
    t = 2 + 2 * rng_int(&g, 3);
    cnt = 1 + rng_int(&g, 4);
    tgt = 82 + rng_int(&g, 6);
    n = candy_figure(c, &g, &R, t, cnt, 32, tgt, 0, out, 0);
    if (rng_int(&g, 2)) {
        t = 34 + 2 * rng_int(&g, 3);
        cnt = 1 + rng_int(&g, 3);
        tgt = 80 + rng_int(&g, 8);
        n = candy_figure(c, &g, &R, t, cnt, 64, tgt, 0, out, n);
    }
    return n;
}

/* Ear candy in a candy-only bar: 1-2 notes, the bar played with the mood's candy_pct chance raised by Motion; the
   last bar before a layer change always plays, a 2-3 note rising figure into the change. */
static int sparse_bar(const ctx_t *c, int bar, uint32_t salt, nt_t *out)
{
    static const uint8_t ON[6] = {2, 4, 6, 10, 12, 14};
    const plan_block_t *b = plan_at(c->p, bar), *nx = plan_at(c->p, bar + 1);
    region_t R;
    rng_t g;
    int pct = LIB_MOODS[clampi(c->r->v[RF_MOOD], 0, 3)].candy_pct + clampi(c->r->v[RF_MOTION], 0, 100) / 5;
    int edge = b && nx && nx != b && nx->layers != b->layers, t, cnt, tgt;
    rng_seed(&g, mix(salt, (uint32_t)bar, 4, 0), 135);
    if (rng_int(&g, 100) >= pct && !edge) return 0;
    region_at(c, bar, 1, &R);
    if (edge) {
        t = 8 + 2 * rng_int(&g, 2);
        cnt = 2 + rng_int(&g, 2);
        tgt = 78 + rng_int(&g, 6);
    } else {
        t = ON[rng_int(&g, 6)];
        cnt = 1 + (rng_int(&g, 3) == 0);
        tgt = 80 + rng_int(&g, 10);
    }
    return candy_figure(c, &g, &R, t, cnt, 16, tgt, edge, out, 0);
}

int melody_step(const song_roll_t *r, const song_plan_t *p, const melody_phrases_t *ph, const motif_t *m, int bar,
                int step, float step_samples, uint32_t salt, song_ev_t *out, int max)
{
    ctx_t c;
    nt_t ns[MAXN * 2];
    int role, P = bar, idx = 0, n, i, rel, cnt = 0, voice;
    if (max <= 0 || bar < 0 || bar >= p->bars || step < 0 || step > 15) return 0;
    role = role_at(p, bar, &P, &idx);
    if (role == R_NONE) return 0;
    ctx_init(&c, r, p);
    if (role == R_LEAD) n = lead_phrase(&c, ph, m, P, idx, salt, ns);
    else if (role == R_ANSWER) n = answer_phrase(&c, P, salt, ns);
    else n = sparse_bar(&c, bar, salt, ns);
    voice = role == R_LEAD ? V_LEAD : V_CANDY;
    rel = (bar - P) * 16 + step;
    for (i = 0; i < n && cnt < max; i++) {
        uint32_t h = mix(salt, (uint32_t)bar, (uint32_t)step, (uint32_t)voice);
        uint32_t delay = 221u + h % 881u;                  /* 5-25 ms late */
        float full = (float)ns[i].len * step_samples, lim = full - (float)delay - 8.0f, len = full * 0.9f;
        int vel;
        if (ns[i].at != rel) continue;
        if (len > lim) len = lim;
        if (len < 1.0f) len = 1.0f;
        vel = voice == V_LEAD ? 70 + (int)((h >> 12) % 19u) + (ns[i].at == 0 ? 6 : 0) : 46 + (int)((h >> 12) % 21u);
        out[cnt].voice = (uint8_t)voice;
        out[cnt].note = ns[i].note;
        out[cnt].vel = (uint8_t)clampi(vel, 25, 110);
        out[cnt].pad = 0;
        out[cnt].delay = delay;
        out[cnt].len = (uint32_t)len;
        cnt++;
    }
    return cnt;
}

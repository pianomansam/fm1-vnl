/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The lead loop (lead-loop design, 2026-11-03; ledger: "Hip hop record-style repetition for leads"): the lead as a
   sampled record. With the song's switch on (song_ov()->lead_loop 1, or -1: LEAD_LOOP_DEFAULT, on), a song of any
   lead style records its phrase once (Jazz-Hop and Ambient first, then every style: Sam, 2026-11-04), L bars (2 or
   4: the chord loop's length, 4 at most), as the style plans those bars the first time the lead reaches them (slot:
   bar % L, so each recorded bar replays over the same place in the chord loop). Every later lead bar replays its slot
   exactly: the same notes, onsets, lengths, velocities and triplet lateness. No new notes. Jazz-Hop records a riff,
   not its sparse phrase (jazzhop.c): per recorded bar about 2.9 notes (Ambient 2.2; the sparse phrase 2.3). A new
   key, mode, chords, set or style, or the workbench's keys or lead octave, records anew from the bar it lands on (no
   transposing), and enters there as a new sample would.
   Harmony: each replayed note is checked against the bar it plays over (the section's own chord, keys and bass:
   leads_legal, a chord tone on beats 1 and 3, the bar's range); one that would rub moves to the nearest note that
   passes (a scale or pentatonic step, 2 scale steps at most; where the bar meets the hook floor, 3 pitches over a 4th,
   one that keeps it first), else drops. A note ringing over the bar line (a stutter's last repeat too) is cut at it
   where it would rub over the next bar's chord.
   The producer's moves on the loop (no new notes), all drawn from the song's phrase seed:
     - the lead's entry (a block that starts a run of lead blocks): the song's first 40% filtered in (low-pass 0.75,
       about 600 Hz, for 2 bars, opening over the next 2); a later one 20% held back (its first 4 bars muted, 8-bar
       blocks), 35% filtered in. A lead block straight after another: 15% filtered in. A live change landing mid-run
       re-records from its bar, a later entry there (held back 20%, its 4 bars muted where 8 of its block remain, else
       filtered in; filtered in 35%), in place of the block's own entry over those 4 bars
     - a bar muted: 12% of 8-bar passes, one hook bar (the pass's bar 3 or 5); never the first pass
     - the drop before a section change: the last bar of a lead block with a block after it, 50%: half of them the
       whole bar out, half its last 2 beats
     - the stuck-note stutter: on the turnaround's hook bar (bar 7 of an 8-bar pass) 40%, on a block's last bar 30%
       (not dropped); never in the song's first lead pass, nor in the bar right after a stutter. One note (55%), or 2
       (30%) or 3 (15%) quick notes (a quarter at most) of the bar, gets stuck: a note repeats every 16th or 8th, a
       fragment back to back, 2-4 more times (fragments 2-3), each repeat 12% softer; then the phrase carries on where
       it would have been (the notes under the stutter are skipped, the last repeat rings for the stuck note's own
       length). */
#include <stdint.h>
#include "harmony.h"
#include "leads.h"
#include "library.h"
#include "override.h"
#include "rng.h"

int leads_loop_plays(int style)
{
    int on = song_ov()->lead_loop;
    if (on < 0) on = LEAD_LOOP_DEFAULT;
    (void)style;
    return on > 0;
}

#ifdef LEADS_LOOP_STATS
static uint8_t lg[256];
int leads_loop_stat(const lead_state_t *ls, int which)
{
    return which >= 0 && which < 4 ? ls->loop.stat[which] : 0;
}
int leads_loop_log(int bar) { return bar >= 0 && bar < 256 ? lg[bar] : 0; }
#define COUNT(ls, i) ((ls)->loop.stat[i]++)
#else
#define COUNT(ls, i) ((void)0)
#endif

enum { LE_NONE, LE_FILTER, LE_HOLD };
typedef struct { unsigned mute; int stut; float lp0, lp1; int entry, hold; } lm_t;

static int lead_before(const plan_block_t *b)
{
    while (b->start > 0) {
        b--;
        if (lead_plays(b)) return 1;
    }
    return 0;
}

/* the moves on bar (b: its plan block; ls: the loop, for the bar a live change re-recorded from), as the header
   says */
static void moves(const song_roll_t *r, const lead_state_t *ls, const plan_block_t *b, int bar, lm_t *m)
{
    uint32_t seed = song_ov_phrase_seed(r->seed);
    const plan_block_t *nb = (b + 1)->bars && (b + 1)->start == b->start + b->bars ? b + 1 : 0;
    int k = bar - b->start, first = !lead_before(b), run, pass, bip, pbars, last, x, lastbar = k == b->bars - 1, fp;
    int from = ls->loop.from - 1, ke = k;
    const plan_block_t *rb = b;
    rng_t g;
    m->mute = 0;
    m->stut = 0;
    m->lp0 = m->lp1 = 0.0f;
    m->entry = LE_NONE;
    m->hold = 0;
    if (!lead_plays(b)) return;
    leads_phrase_at(b, bar, &run, &pass, &bip, &pbars, &last);
    while (rb->start > run) rb--;
    fp = pass == 0 && !lead_before(rb);               /* the song's first lead pass (its run may span blocks) */
    rng_seed(&g, seed ^ ((uint32_t)b->start * 0x9E3779B1u), 610);
    x = rng_int(&g, 100);
    if (run != b->start) m->entry = x < 15 ? LE_FILTER : LE_NONE;   /* a lead block after a lead block */
    else m->entry = first ? (x < 40 ? LE_FILTER : LE_NONE)
                          : x < 20 ? (b->bars >= 8 ? LE_HOLD : LE_FILTER) : x < 55 ? LE_FILTER : LE_NONE;
    if (from > run && bar >= from && bar < from + 4) {   /* a live change re-recorded mid-run from bar `from`: a new
                                                            entry there, at a later entry's odds */
        const plan_block_t *fb = b;
        while (fb->start > from) fb--;
        rng_seed(&g, seed ^ ((uint32_t)from * 0x9E3779B1u), 615);
        x = rng_int(&g, 100);
        m->entry = x < 20 ? (from + 8 <= fb->start + fb->bars ? LE_HOLD : LE_FILTER) : x < 55 ? LE_FILTER : LE_NONE;
        ke = bar - from;
    }
    if (ke < 4 && m->entry == LE_HOLD) { m->mute = 0xFFFFu; m->hold = 1; }
    if (ke < 4 && m->entry == LE_FILTER) {
        m->lp0 = ke < 2 ? 0.75f : ke == 2 ? 0.75f : 0.375f;
        m->lp1 = ke < 2 ? 0.75f : ke == 2 ? 0.375f : 0.0f;
    }
    if (lastbar && nb) {                              /* the drop before the section change */
        rng_seed(&g, seed ^ ((uint32_t)bar * 0x85EBCA6Bu), 611);
        if (rng_int(&g, 100) < 50) m->mute |= rng_int(&g, 2) ? 0xFFFFu : 0xFF00u;
    }
    if (!fp && !m->mute && k >= 4) {
        rng_seed(&g, seed ^ ((uint32_t)run * 0x27D4EB2Fu) ^ ((uint32_t)pass * 0x165667B1u), 612);
        if (rng_int(&g, 100) < 12 && bip == 2 + 2 * rng_int(&g, 2)) m->mute = 0xFFFFu;   /* a bar out */
    }
    if (!m->mute && !fp) {                            /* the stutter */
        rng_seed(&g, seed ^ ((uint32_t)bar * 0xC2B2AE35u), 613);
        x = rng_int(&g, 100);
        if ((bip == 6 && x < 40) || (lastbar && x < 30)) m->stut = 1;
    }
    if (m->stut && bar > 0) {                         /* none in the bar right after a stutter (the draws kept: no
                                                         other bar's move shifts) */
        lm_t p;
        moves(r, ls, bar - 1 >= b->start ? b : b - 1, bar - 1, &p);
        if (p.stut) m->stut = 0;
    }
}

float leads_loop_lp(const song_roll_t *r, const lead_state_t *ls, const plan_block_t *b, int bar, int step,
                    int style)
{
    lm_t m;
    if (!leads_loop_plays(style) || !b) return 0.0f;
    moves(r, ls, b, bar, &m);
    return m.lp0 + (m.lp1 - m.lp0) * (float)(step + 1) * (1.0f / 16.0f);
}

/* a replayed note at step t sounds over this bar: legal, a chord tone on beats 1 and 3, in the bar's range */
static int fits(const leads_ctx_t *c, int note, int t, int ext)
{
    return note >= (ext ? c->lo_x : c->lo) && note <= c->hi && leads_legal(c->r, note, c->pcs, c->np, c->bass) &&
           (t % 8 || leads_is_ct(note, c->pcs, c->np));
}

/* 1 where the bar's notes (on, with step t's note q) meet the hook floor: 3 pitches or more spanning a 4th or wider
   (a bar of under 3 notes always does) */
static int bar_floor(const int *on, int t, int q)
{
    int i, n = 0, d = 0, lo = 128, hi = 0, j;
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

/* the nearest note that passes (a scale or pentatonic step, 2 scale steps at most), else 0; where the bar (on) meets
   the hook floor as recorded, one that keeps it first (a nudge never folds a hook onto its own pitches) */
static int nudge(const leads_ctx_t *c, const int *on, int note, int t, int ext)
{
    unsigned scale = leads_scale_mask(c->r), pent = leads_pent_mask(c->r, 0);
    int cand[6], i, best = 0, bd = 999, keep = bar_floor(on, t, note);
    cand[0] = leads_coll_step(scale, note, -1); cand[1] = leads_coll_step(scale, note, 1);
    cand[2] = leads_coll_step(pent, note, -1); cand[3] = leads_coll_step(pent, note, 1);
    cand[4] = leads_coll_step(scale, note, -2); cand[5] = leads_coll_step(scale, note, 2);
    for (i = 0; i < 6; i++) {
        int d = cand[i] > note ? cand[i] - note : note - cand[i];
        if (keep && !bar_floor(on, t, cand[i])) d += 100;
        if (fits(c, cand[i], t, ext) && d < bd) { bd = d; best = cand[i]; }
    }
    return best;
}

/* The loop floor (Sam 2026-11-04: a loop has no longer phrase to vary its notes in): a finished recording plays 4
   pitches or more (3 where it has fewer than 8 notes: half its notes, 3 at least) spanning a 4th or wider. Where it
   doesn't, its repeated notes move, the last bar's first and latest first (the motif's first note never), to the
   nearest scale note 1-3 steps away that the loop doesn't play yet and that fits its bar as recorded (a chord tone
   on beats 1 and 3), outward first while the span is short. Onsets, lengths and velocities stay. Returns 1 where the
   loop meets the floor. */
static int widen(lead_state_t *ls, const leads_ctx_t *c, int last)
{
    unsigned scale = leads_scale_mask(c->r);
    uint8_t cnt[128] = {0};   /* a pitch's notes in the loop: 64 at most (4 bars of 16ths) */
    int np = 0, lo = 127, hi = 0, n = 0, need, k, i, d, j, pcs[5];
    leads_ctx_t h = *c;
    for (k = 0; k < ls->loop.bars; k++)
        for (i = 0; i < 16; i++) {
            int note = ls->loop.note[k][i];
            if (note <= 0 || note > 127) continue;
            n++;
            if (!cnt[note]++) np++;
            if (note < lo) lo = note;
            if (note > hi) hi = note;
        }
    need = n / 2 < 3 ? 3 : n / 2 > 4 ? 4 : n / 2;
    h.pcs = pcs;
    for (j = 0; j < ls->loop.bars && (np < need || hi - lo < 5); j++) {
        int slot = (last - j + ls->loop.bars) % ls->loop.bars, first = 1;
        for (k = 0; k < 5; k++) pcs[k] = ls->loop.pcs[slot][k];
        h.np = ls->loop.np[slot]; h.bass = ls->loop.bass[slot];
        h.lo = ls->loop.lo[slot]; h.lo_x = ls->loop.lo_x[slot]; h.hi = ls->loop.hi[slot];
        for (i = 0; i < 16 && slot == (last + 1) % ls->loop.bars; i++)
            if (ls->loop.note[slot][i] > 0) { first = i; break; }   /* the motif's first note: kept */
        for (i = 15; i >= 0 && (np < need || hi - lo < 5); i--) {
            int note = ls->loop.note[slot][i], ext = i + ls->loop.len[slot][i] > 16, best = 0, bc = 99;
            if (note <= 0 || note > 127 || cnt[note] < 2 || (slot == (last + 1) % ls->loop.bars && i == first))
                continue;
            for (d = -3; d <= 3; d++) {
                int q = d ? leads_coll_step(scale, note, d) : 0, cost = d < 0 ? -d * 2 : d * 2;
                if (q <= 0 || q > 127 || cnt[q] || !fits(&h, q, i, ext)) continue;
                if (hi - lo < 5 && q > lo && q < hi) cost += 10;   /* inside the span: no wider */
                if (cost < bc) { bc = cost; best = q; }
            }
            if (!best) continue;
            cnt[note]--;
            cnt[best]++;
            np++;
            if (best < lo) lo = best;
            if (best > hi) hi = best;
            ls->loop.note[slot][i] = (uint8_t)best;
        }
    }
    return np >= need && hi - lo >= 5;
}

/* the stuck-note stutter on the bar in ls (its notes already checked; c: the bar's context, a repeat on beat 1 or 3
   a chord tone, else an earlier note stutters) */
static int stutter(lead_state_t *ls, const leads_ctx_t *c, int bar)
{
    const song_roll_t *r = c->r;
    int t[16], n = 0, i, j, k, f, S = 0, N = 0, t0 = 0, x, ok = 0, w, fn[3], fo[3], fl[3], fv[3], fd[3], fr[3];
    rng_t g;
    for (i = 0; i < 16; i++) if (ls->on[i] > 0) t[n++] = i;
    if (!n) return 0;
    rng_seed(&g, song_ov_phrase_seed(r->seed) ^ ((uint32_t)bar * 0x2545F491u), 614);
    k = rng_int(&g, n);
    x = rng_int(&g, 100);
    f = x < 55 ? 1 : x < 85 ? 2 : 3;
    x = rng_int(&g, 2);                               /* a single note: 8ths (1) or 16ths */
    w = rng_int(&g, 3);
    for (; k >= 0 && !ok; k--) {                      /* the chosen note, else an earlier one that fits */
        int ff = f > n - k ? n - k : f;
        t0 = t[k];
        if (ff > 1) {
            S = (k + ff < n ? t[k + ff] : t[k + ff - 1] + 2) - t0;
            if (S > 4) ff = 1;
        }
        if (ff == 1) S = x ? 2 : 1;
        N = ff == 1 ? 2 + w : 2 + (w & 1);
        while (N > 2 && t0 + S * (N + 1) > 16) N--;
        if (t0 + S * (N + 1) > 16 && ff == 1 && S == 2) S = 1;
        if (t0 + S * (N + 1) <= 16) {
            int q, e;
            ok = 1;
            for (q = 1; q <= N && ok; q++)            /* a repeat on beat 1 or 3: a chord tone */
                for (e = 0; e < ff; e++) {
                    int p = t0 + q * S + t[k + e] - t0;
                    if (p < 16 && p % 8 == 0 && !leads_is_ct(ls->on[t[k + e]], c->pcs, c->np)) ok = 0;
                }
            if (ok) f = ff;
        }
    }
    if (!ok) return 0;
    k++;
    for (i = 0; i < f; i++) {                         /* the fragment */
        int s = t[k + i];
        fn[i] = ls->on[s]; fo[i] = s - t0; fl[i] = ls->len[s]; fv[i] = ls->vel[s]; fd[i] = ls->dl[s];
        fr[i] = (ls->ring >> s) & 1u;
    }
    w = t0 + S * (N + 1);
    for (i = 0; i < 16; i++) {                        /* under the stutter: skipped; a note into it cut */
        if (ls->on[i] <= 0) continue;
        if (i >= t0 && i < w) { ls->on[i] = 0; ls->vel[i] = 0; ls->dl[i] = 0; ls->ring &= ~(1u << i); }
        else if (i < t0 && i + ls->len[i] > t0) { ls->len[i] = t0 - i; ls->ring &= ~(1u << i); }
    }
    for (j = 0; j <= N; j++) {
        float v = 1.0f;
        for (i = 0; i < j; i++) v *= 0.88f;
        for (i = 0; i < f; i++) {
            int p = t0 + j * S + fo[i], len = S - fo[i], vv = (int)((float)fv[i] * v + 0.5f);
            if (len > fl[i]) len = fl[i];
            if (j == N && i == f - 1 && p + fl[i] > p + len) {   /* the last repeat: its own length (to the next
                                                                    note; past the next bar line never) */
                int e = p + fl[i] < 32 ? p + fl[i] : 32, q;
                for (q = p + 1; q < 16 && q < e; q++)
                    if (ls->on[q] > 0) { e = q; break; }
                len = e - p;
                if (fr[i] && e == (p + fl[i] < 32 ? p + fl[i] : 32)) ls->ring |= 1u << p;
            }
            if (p > 15 || len < 1) continue;
            ls->on[p] = fn[i]; ls->len[p] = len; ls->dl[p] = (uint8_t)fd[i];
            ls->vel[p] = (uint8_t)(vv < 25 ? 25 : vv);
        }
    }
    return 1;
}

void leads_loop_bar(lead_state_t *ls, const leads_ctx_t *c, const plan_block_t *b, int bar, const leads_next_t *nx,
                    int style, void (*plan)(lead_state_t *, const leads_ctx_t *, const plan_block_t *, int,
                                            const leads_next_t *))
{
    const lib_prog_t *pr = lib_prog_by_id((uint8_t)c->r->v[RF_PROG]);
    int bpc = pr && pr->bars_per_chord ? pr->bars_per_chord : 1, loop = pr && pr->n ? pr->n * bpc : 4;
    int L = loop >= 4 ? 4 : 2, slot = bar % L, i, from = ls->tied ? ls->tie_end : 0, ext, tied = 0, te = 0;
    uint32_t sig = song_ov_phrase_seed(c->r->seed) ^ (uint32_t)(c->r->v[RF_KEY] * 7 + c->r->v[RF_MODE] * 131 +
                   c->r->v[RF_PROG] * 1031 + c->r->v[RF_SET] * 8191 + style * 65537 +
                   (harmony_keys_oct(c->r) / 12 * 3 + song_ov()->lead_oct) * 524287);
    lm_t m, mn;
    const plan_block_t *nb = bar + 1 < b->start + b->bars ? b
                           : (b + 1)->bars && (b + 1)->start == b->start + b->bars ? b + 1 : 0;
    if (ls->loop.sig != sig || ls->loop.bars != L || (!ls->loop.have && ls->loop.sig && !ls->loop.redo)) {
        /* a new song (its state zeroed), or a live change (key, mode, chords, set, style, the keys' or lead's
           octave): record anew; a live change from this bar, its new entry here (moves) */
        int live = ls->loop.sig != 0;
        ls->loop.have = 0;
        ls->loop.tries = 0;
        ls->loop.sig = sig;
        ls->loop.bars = (uint8_t)L;
        ls->loop.from = (int16_t)(live ? bar + 1 : 0);
#ifdef LEADS_LOOP_STATS
        for (i = live && bar > 0 ? bar : 0; i < 256; i++) lg[i] = 0;
#endif
    }
    if (!((ls->loop.have >> slot) & 1u)) {            /* record: the style plans the bar, once */
        plan(ls, c, b, bar, nx);
        for (i = 0; i < 16; i++) {
            ls->loop.note[slot][i] = (uint8_t)ls->on[i];
            ls->loop.len[slot][i] = (uint8_t)ls->len[i];
            ls->loop.vel[slot][i] = ls->vel[i];
            ls->loop.dl[slot][i] = ls->dl[i];
        }
        ls->loop.ring[slot] = (uint16_t)ls->ring;
        ls->loop.acc[slot] = (uint16_t)ls->acc;
        for (i = 0; i < 5; i++) ls->loop.pcs[slot][i] = (uint8_t)(i < c->np ? c->pcs[i] : 0);
        ls->loop.np[slot] = (uint8_t)c->np;
        ls->loop.bass[slot] = (uint16_t)c->bass;
        ls->loop.lo[slot] = (uint8_t)c->lo;
        ls->loop.lo_x[slot] = (uint8_t)c->lo_x;
        ls->loop.hi[slot] = (uint8_t)c->hi;
        ls->loop.have |= (uint8_t)(1u << slot);
        ls->loop.redo = 0;
        if (ls->loop.have == (1u << L) - 1u && !widen(ls, c, slot) && ls->loop.tries < 2) {
            ls->loop.tries++;
            ls->loop.redo = 1;                         /* still under the floor (a lead entering mid-phrase records
                                                         only its tail): played once, then recorded again */
            ls->loop.have = 0;
        }
    } else COUNT(ls, 0);
    for (i = 0; i < 16; i++) {                        /* the replay */
        ls->on[i] = ls->loop.note[slot][i];
        ls->len[i] = ls->loop.len[slot][i];
        ls->vel[i] = ls->loop.vel[slot][i];
        ls->dl[i] = ls->loop.dl[slot][i];
    }
    ls->ring = ls->loop.ring[slot];
    ls->acc = ls->loop.acc[slot];
    for (i = 0; i < 16; i++) {
        int note = ls->on[i];
        if (note <= 0) continue;
        if (i < from) { ls->on[i] = 0; continue; }    /* a note rings in from the last bar */
        ext = i + ls->len[i] > 16;
        if (!fits(c, note, i, ext)) {
            int a = nudge(c, ls->on, note, i, ext);
            if (a) { ls->on[i] = a; COUNT(ls, 1); }
            else { ls->on[i] = 0; COUNT(ls, 2); continue; }
        }
        if (ext && !(c->can_push && nx->np > 0 && leads_legal(c->r, ls->on[i], nx->pcs, nx->np, nx->bass))) {
            ls->len[i] = 16 - i;
            ls->ring &= ~(1u << i);
        }
    }
    moves(c->r, ls, b, bar, &m);
    if (m.stut && stutter(ls, c, bar)) {
        COUNT(ls, 3);
        for (i = 0; i < 16; i++)                      /* a last repeat rung past the bar line: cut at it where it
                                                         would rub over the next bar's chord, as any note, or where
                                                         it wasn't a tie */
            if (ls->on[i] > 0 && i + ls->len[i] > 16 && (!((ls->ring >> i) & 1u) ||
                !(c->can_push && nx->np > 0 && leads_legal(c->r, ls->on[i], nx->pcs, nx->np, nx->bass)))) {
                ls->len[i] = 16 - i;
                ls->ring &= ~(1u << i);
            }
    } else m.stut = 0;
    if (nb) moves(c->r, ls, nb, bar + 1, &mn);
    else mn.mute = 0;
    for (i = 0; i < 16; i++) {                        /* the mutes; a note into a muted step cut there */
        int e = i + ls->len[i], q;
        if (ls->on[i] <= 0) continue;
        if ((m.mute >> i) & 1u) { ls->on[i] = 0; ls->ring &= ~(1u << i); continue; }
        for (q = i + 1; q < 16 && q < e; q++)
            if ((m.mute >> q) & 1u) { ls->len[i] = q - i; ls->ring &= ~(1u << i); break; }
        if (((ls->ring >> i) & 1u) && (mn.mute & 1u)) { ls->len[i] = 16 - i; ls->ring &= ~(1u << i); }
    }
    for (i = 0; i < 16; i++) {
        if (ls->on[i] <= 0) { ls->vel[i] = 0; ls->dl[i] = 0; ls->len[i] = 0; continue; }
        if ((ls->ring >> i) & 1u) { tied = 1; if (i + ls->len[i] - 16 > te) te = i + ls->len[i] - 16; }
        ls->last = ls->on[i];
    }
    ls->tied = tied;
    ls->tie_end = te;
#ifdef LEADS_LOOP_STATS
    if (bar >= 0 && bar < 256)
        lg[bar] = (uint8_t)((m.stut ? 1 : 0) | (m.mute == 0xFFFFu ? 2 : m.mute ? 4 : 0) | (m.lp0 > 0.0f ? 8 : 0) |
                            (m.hold ? 16 : 0));
#endif
}

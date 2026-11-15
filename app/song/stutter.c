/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The whole-mix stutter's placement (Sam, 2026-11-09: the whole mix, after Cut-A-Dor's captured slice; 2026-11-14:
   two kinds, then every section's group ends, not just the lead's, and one on demand; the odds re-tuned so a song
   keeps its totals: simple 650 to 288 in 1000, complex 600 to 520).
   Only on the last bar of a group as the screen shows it (Sam, 2026-11-13: 4 of 4, 8 of 8; a group 8 bars from the
   block's start, a block's 4-bar tail its own group), in a groove or lead section with the lead or without it: never
   in an intro, an outro or a break. Never in the song's first pass: where the lead plays, its first lead pass (the
   first 8 bars of the song's first lead run); where it doesn't, the first 8 bars from the start of the song's first
   block without the lead after the intro. Where the lead plays, never in a bar the lead loop mutes (a bar out, a
   held-back entry). Never in the bar right after a stutter. Two kinds, one a bar at most:
     complex (3 an accelerating roll, 4 the 2-beat catch; evenly): one a song at most, about 0.5 a song: ST_COMPLEX
     in 1000 songs draw one, on a bar drawn evenly from the song's group-last bars where the section doesn't already
     move a filter (a dark block; a drum drop's or a swell's bars; where the lead plays, the lead loop's filtered
     entry)
     simple (1 an 8th repeat, 2 a 16th roll; evenly): ST_SIMPLE in 1000 of the other group-last bars, the filtered
     ones too (about 1 a song)
   On demand (a move at the section's end: Sections K3, F#3): a kind queued (lead_state_t's loop.sq) plays, its figure
   drawn evenly within the kind, on a section's last bar (move_lands; the rules above aside), in place of the song's
   own there (song_move_queue: that bar itself before its figure's catch).
   A stuttering bar's notes play as planned (the lead loop's drop before a section change skips it); the engine
   stutters the whole mix at the bar's end in the figure (voices.c's vx_stutter_fig), the next downbeat live. */
#include <stdint.h>
#include "stutter.h"
#include "leads/leads.h"
#include "override.h"
#include "rng.h"

/* the odds, as the header says: a simple one's in 1000 on a group's last bar; a song's one complex one's in 1000 */
enum { ST_SIMPLE = 288, ST_COMPLEX = 520 };

#ifdef LEADS_LOOP_STATS
static uint8_t lg[256];
static int st_force;
int stutter_log(int bar) { return bar >= 0 && bar < 256 ? lg[bar] : 0; }
void stutter_force(int on) { st_force = on; }
#endif

int stutter_group_last(const plan_block_t *b, int bar)
{
    int k = b ? bar - b->start : -1, g8;
    if (k < 0 || k >= b->bars) return 0;
    g8 = b->bars - (k & ~7) >= 8;
    return (g8 ? k & 7 : k & 3) == (g8 ? 7 : 3);
}

int move_lands(const plan_block_t *b, int bar)
{
    return b && bar == b->start + b->bars - 1 && b->section != SEC_OUTRO && b[1].bars;
}

int stutter_catch(int fig) { return fig == 4 ? 8 : 12; }

/* bar's block's neighbor b may be bar's own or the one before (bar - 1 crossing a block line) */
static const plan_block_t *block_of(const plan_block_t *b, int bar)
{
    return bar >= b->start || b->start == 0 ? b : b - 1;
}

/* where the song's own stutter may go on bar: 0 nowhere, 1 a simple one, 2 a complex one too */
static int cand(const song_roll_t *r, const lead_state_t *ls, const plan_block_t *b, int bar)
{
    int k = bar - b->start, lastbar = k == b->bars - 1, still = 0, quiet;
    if (!stutter_group_last(b, bar) || b->section == SEC_INTRO || b->section == SEC_OUTRO || b->section == SEC_BREAK)
        return 0;
    if (lead_plays(b)) {
        if ((still = leads_loop_still(r, ls, b, bar)) < 0) return 0;   /* muted, or the first lead pass */
    } else {
        const plan_block_t *p = b;                    /* the first pass: from the first block without the lead */
        while (p->start > 0) p--;
        while (p->bars && (p->section == SEC_INTRO || lead_plays(p))) p++;
        if (bar < p->start + 8) return 0;
    }
    quiet = still || (b->flags & PB_DARK) || (lastbar && (b->flags & (PB_DROP_LAST | PB_SWELL))) ||
            ((b->flags & (PB_DROP_LAST | PB_DROP2)) == (PB_DROP_LAST | PB_DROP2) && k == b->bars - 2);
    return quiet ? 1 : 2;
}

/* the song's group-last bars (b: any of its plan's blocks) a stutter may take (cand >= need) into cb, in order;
   returns how many */
static int candidates(const song_roll_t *r, const lead_state_t *ls, const plan_block_t *b, int need, uint8_t *cb)
{
    const plan_block_t *q = b;
    int n = 0, i, j;
    while (q->start > 0) q--;
    for (i = 0; i < 25 && q->bars && (i == 0 || q->start == (q - 1)->start + (q - 1)->bars); i++, q++)
        for (j = 3; j < q->bars && n < 64; j += 4)   /* each group's last bar: 8-bar groups, then a 4-bar tail */
            if (cand(r, ls, q, q->start + j) >= need) cb[n++] = (uint8_t)(q->start + j);
    return n;
}

/* the song's complex stutter's bar: ST_COMPLEX in 1000 of songs draw one, evenly from the bars a complex stutter may
   take; -1 none */
static int complex_bar(const song_roll_t *r, const lead_state_t *ls, const plan_block_t *b, uint32_t seed)
{
    uint8_t cb[64];                                   /* (a group is 4 bars or more) */
    int n;
    rng_t g;
    rng_seed(&g, seed, 614);
    if (rng_int(&g, 1000) >= ST_COMPLEX) return -1;
    n = candidates(r, ls, b, 2, cb);
    return n ? cb[rng_int(&g, n)] : -1;
}

/* the song's draw on bar, before the rule against two in a row */
static int drawn(const song_roll_t *r, const lead_state_t *ls, const plan_block_t *b, int bar)
{
    uint32_t seed = song_ov_phrase_seed(r->seed);
    int c = cand(r, ls, b, bar), f = 0;
    rng_t g;
    if (!c) return 0;
    if (c == 2 && bar == complex_bar(r, ls, b, seed)) {   /* the complex one wins the bar: 3 or 4 */
        rng_seed(&g, seed, 616);
        f = 3 + rng_int(&g, 2);
    } else {                                          /* a simple one: 1 or 2 */
        rng_seed(&g, seed ^ ((uint32_t)bar * 0xC2B2AE35u), 613);
        if (rng_int(&g, 1000) < ST_SIMPLE) f = 1 + rng_int(&g, 2);
    }
#ifdef LEADS_LOOP_STATS
    if (st_force) {                                   /* every one, the figures in turn */
        uint8_t cb[64];
        int n = candidates(r, ls, b, 1, cb), i;
        for (i = 0; i < n && cb[i] != bar; i++) {}
        f = 1 + i % 4;
    }
#endif
    return f;
}

int stutter_song(const song_roll_t *r, const lead_state_t *ls, const plan_block_t *b, int bar)
{
    int f = b ? drawn(r, ls, b, bar) : 0;
    if (f && bar > 0 && drawn(r, ls, block_of(b, bar - 1), bar - 1)) f = 0;   /* none right after a stutter */
    return f;
}

int stutter_draw(const song_roll_t *r, int kind, int bar)
{
    rng_t g;
    rng_seed(&g, song_ov_phrase_seed(r->seed) ^ ((uint32_t)bar * 0x2545F491u), 617);
    return (kind == STUT_COMPLEX ? 3 : 1) + rng_int(&g, 2);
}

int stutter_at(const song_roll_t *r, const lead_state_t *ls, const plan_block_t *b, int bar)
{
    if (ls->loop.sbar == bar + 1) return ls->loop.sfig;   /* planned already */
    if (ls->loop.sq && move_lands(b, bar)) return stutter_draw(r, ls->loop.sq, bar);
    return stutter_song(r, ls, b, bar);
}

void stutter_plan_bar(lead_state_t *ls, const song_roll_t *r, const plan_block_t *b, int bar)
{
    int f;
    if (ls->loop.sq && move_lands(b, bar)) {     /* the one queued lands */
        f = stutter_draw(r, ls->loop.sq, bar);
        ls->loop.sq = 0;
        ls->loop.sod = 1;
    } else {
        f = stutter_song(r, ls, b, bar);
        ls->loop.sod = 0;
    }
    ls->loop.sbar = f ? (unsigned)(bar + 1) & 0x1FFu : 0u;
    ls->loop.sfig = (unsigned)f & 7u;
#ifdef LEADS_LOOP_STATS
    if (bar == 0)
        for (f = 0; f < 256; f++) lg[f] = 0;
    if (bar >= 0 && bar < 256) lg[bar] = (uint8_t)(ls->loop.sfig | (ls->loop.sod ? 8u : 0u));
#endif
}

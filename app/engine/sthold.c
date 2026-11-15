/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The held stutter (sthold.h): its schedule, in 16ths, and the engine's calls. */
#include "sthold.h"
#include <string.h>
#include "voices.h"
#include "stutter.h"

/* the repeats' low-pass by level (a one-pole, 6 dB an octave): the 8th open, then 0.55 of the cutoff a step. The
   repeats are already darker than the live mix (mono, half rate), so it works lower down: measured on a full band,
   the band above 1 kHz down about 1.5, 3 and 6 dB at the 16th, 32nd and 64th, the whole about 1 dB at most (the roll
   darkens as it tightens and its buzz softens, without going dull) */
static const float LP[STH_LEVELS] = {0.0f, 2000.0f, 1100.0f, 600.0f};

float sthold_lp(int level) { return level >= 0 && level < STH_LEVELS ? LP[level] : 0.0f; }

int sthold_tick(sthold_t *h, int down, int st)
{
    int r = 0;
    if (h->on && !down) {                /* let go: live on this 16th */
        h->on = 0;
        r |= STH_LIVE;
    } else if (!h->on && down) {         /* the catch: an 8th from here; the first step on the first beat line once
                                            the 8th has repeated whole (4 16ths on, or more) */
        h->on = h->lock = 1;
        h->k = h->level = 0;
        h->next = (uint8_t)(4 + (4 - st % 4) % 4);
        r |= STH_CATCH;
    } else if (h->on) {
        if (h->k < 255u) h->k++;
        if (h->k == 2u)
            r |= STH_REPLAY;
        else if (h->k == h->next && h->level < STH_LEVELS - 1) {
            h->level++;
            h->next = (uint8_t)(h->next + 4u);
            r |= STH_STEP;
        }
    }
    if (!h->on && h->lock && st == 0) {  /* the bar line after a release: the song's own stutters again */
        h->lock = 0;
        r |= STH_FREE;
    }
    return r;
}

int sthold_run(sthold_t *h, int down, int st, float s16)
{
    int r = sthold_tick(h, down, st);
    uint32_t len = (uint32_t)(2.0f * s16) >> h->level;   /* the repeat: an 8th, halved a level */
    if (r & STH_CATCH) vx_stutter_hold(1, len, sthold_lp(0));
    if (r & STH_REPLAY) vx_stutter_hold(2, 0, sthold_lp(0));
    if (r & STH_STEP) vx_stutter_hold(3, len, sthold_lp(h->level));
    if (r & STH_LIVE) vx_stutter_hold(0, 0, sthold_lp(h->level));   /* (the fade out as it sounded) */
    if (r & STH_FREE) vx_stutter_free();
    return r;
}

void sthold_key(sthold_t *h, int down, uint32_t pos)
{
    if (down && !h->down) h->t_down = pos;
    h->down = (uint8_t)(down != 0);
}

int sthold_song(sthold_t *h, song_t *s, int bar, int st, uint32_t pos)
{
    const lead_state_t *ls = &s->lead;
    int r, kind = h->requeue;
    if (h->lock && !vx_stutter_held()) {          /* stopped or ended since: a fresh press starts it */
        memset(h, 0, sizeof *h);
        kind = 0;
    }
    /* held: the tap window (and its margin) past the press (a tap, shorter, never catches: it queues one instead), or
       holding on */
    r = sthold_run(h, h->down && (h->on || (uint32_t)(pos - h->t_down) >= STH_TAP_SAMPLES), st, song_step_samples(s));
    if (kind && st == 0) {                        /* the bar after one kept from its catch: queued again (unless
                                                     another was queued since: the newer stands) */
        h->requeue = 0;
        if (!ls->loop.sq) song_stutter_queue(s, kind);
    }
    if (h->lock && ls->loop.sod && ls->loop.sbar == (unsigned)(bar + 1) && st == (ls->loop.sfig == 4 ? 8 : 12))
        h->requeue = ls->loop.sfig >= 3 ? STUT_COMPLEX : STUT_SIMPLE;   /* its catch kept out by the hold */
    return r;
}

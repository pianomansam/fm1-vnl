/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The held stutter (G5 held: Sam, 2026-11-14). On the next 16th an 8th of the whole mix is caught, then repeated; at
   each beat after its first whole repeat the repeat halves, to a 16th, a 32nd, then a 64th, where it stays, a low-pass
   on the repeats closing a step each time; let go, the live mix back on the next 16th (vx_stutter's crossfade). The
   song's own stutters wait from the catch to the bar line after the release (vx_stutter_hold's lock), so one whose
   bar the hold touches is skipped; the song's draws never change. A stutter on demand (a move at the section's end:
   song_move_queue) the hold kept from its catch is queued again, for the next section's last bar. One call a 16th
   (sthold_song): app.c's step, wb.c's tick, just before the song's own stutter (vx_stutter_fig). */
#ifndef ENGINE_STHOLD_H
#define ENGINE_STHOLD_H
#include <stdint.h>
#include "song.h"

/* what a 16th does (sthold_tick's bits): the catch, the repeat starting, a step faster, live again, the lock let go */
enum { STH_CATCH = 1, STH_REPLAY = 2, STH_STEP = 4, STH_LIVE = 8, STH_FREE = 16 };
enum { STH_LEVELS = 4 };                 /* the repeat: an 8th, a 16th, a 32nd, a 64th */

typedef struct {
    uint8_t on;                          /* a hold under way (its catch made) */
    uint8_t k;                           /* 16ths since its catch */
    uint8_t next;                        /* the 16th (k) of its next step faster */
    uint8_t level;                       /* the repeat now: 0 an 8th .. 3 a 64th */
    uint8_t lock;                        /* the song's own stutters waiting (until the bar line after the release) */
    uint8_t down, press;                 /* the key held; a press not yet taken by a 16th (a tap shorter than one) */
    uint8_t requeue;                     /* a stutter on demand the hold kept from its bar: its move (MOVE_SIMPLE,
                                            MOVE_COMPLEX), queued again at the next bar line */
} sthold_t;

/* one 16th, st its step in the bar (0..15), down the key held (or pressed since the last 16th): STH_* bits */
int sthold_tick(sthold_t *h, int down, int st);
/* sthold_tick, then the engine told (vx_stutter_hold, vx_stutter_free); s16 a 16th's samples */
int sthold_run(sthold_t *h, int down, int st, float s16);
/* the key down (1; the caller's: only while a song plays) or up (0), taken at the next 16th */
void sthold_key(sthold_t *h, int down);
/* the 16th st of bar, after the song's tick: a stop since (vx_release_all) forgotten, sthold_run, and a stutter on
   demand kept from its bar queued again (song_move_queue) at the next bar line */
int sthold_song(sthold_t *h, song_t *s, int bar, int st);
/* the repeats' low-pass at a level (Hz; 0 open) */
float sthold_lp(int level);

#endif

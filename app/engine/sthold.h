/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The held stutter (G5 held: Sam, 2026-11-14). Held past the tap window (STH_TAP_MS; a shorter press is a tap: the
   navigator's, which queues a stutter on demand), from the next 16th line an 8th of the whole mix is caught, then
   repeated; at each beat after its first whole repeat the repeat halves, to a 16th, a 32nd, then a 64th, where it
   stays, a low-pass on the repeats closing a step each time; let go, the live mix back on the next 16th (vx_stutter's
   crossfade). The song's own stutters wait from the catch to the bar line after the release (vx_stutter_hold's lock),
   so one whose bar the hold touches is skipped; the song's draws never change. A stutter on demand (a tap on G5) the
   hold kept from its catch is queued again, for the next group's last bar. One call a 16th (sthold_song): app.c's
   step, wb.c's tick, just before the song's own stutter (vx_stutter_fig). */
#ifndef ENGINE_STHOLD_H
#define ENGINE_STHOLD_H
#include <stdint.h>
#include "song.h"

/* what a 16th does (sthold_tick's bits): the catch, the repeat starting, a step faster, live again, the lock let go */
enum { STH_CATCH = 1, STH_REPLAY = 2, STH_STEP = 4, STH_LIVE = 8, STH_FREE = 16 };
enum { STH_LEVELS = 4 };                 /* the repeat: an 8th, a 16th, a 32nd, a 64th */

/* G5's tap window, ms, one for the navigator (nav.c: let go within it, a tap; held past it, the hold, said) and the
   engine (sthold_song: the catch on the first 16th line at or after it from the press). Fixed, not the song's 16th:
   the FM-1 reads its keys only between redraws (up to about 90 ms apart), so a quick tap can measure two frames or
   more. A calibration knob: tune it on the hardware */
#define STH_TAP_MS 300u
/* the engine's margin past the window (ms): the press and the release reach the render a block or two apart from
   where the navigator timed them, so a press the navigator calls a tap (let go within the window) never catches;
   one let go inside the margin is said (STUTTER) and does neither */
#define STH_TAP_GUARD_MS 20u
/* the engine's window, samples (44.1 kHz) */
#define STH_TAP_SAMPLES ((STH_TAP_MS + STH_TAP_GUARD_MS) * 441u / 10u)

typedef struct {
    uint8_t on;                          /* a hold under way (its catch made) */
    uint8_t k;                           /* 16ths since its catch */
    uint8_t next;                        /* the 16th (k) of its next step faster */
    uint8_t level;                       /* the repeat now: 0 an 8th .. 3 a 64th */
    uint8_t lock;                        /* the song's own stutters waiting (until the bar line after the release) */
    uint8_t down;                        /* the key held */
    uint32_t t_down;                     /* the sample (the caller's clock) it went down at: held from the window on */
    uint8_t requeue;                     /* a stutter on demand the hold kept from its bar: its kind, queued again at
                                            the next bar line */
} sthold_t;

/* one 16th, st its step in the bar (0..15), down the key held (or pressed since the last 16th): STH_* bits */
int sthold_tick(sthold_t *h, int down, int st);
/* sthold_tick, then the engine told (vx_stutter_hold, vx_stutter_free); s16 a 16th's samples */
int sthold_run(sthold_t *h, int down, int st, float s16);
/* the key down (1; the caller's: only while a song plays) at sample pos, or up (0) */
void sthold_key(sthold_t *h, int down, uint32_t pos);
/* the 16th st of bar starting at sample pos, after the song's tick: a stop since (vx_release_all) forgotten; the key
   down the tap window or more (STH_TAP_SAMPLES) by pos, held (sthold_run); a stutter on demand kept from its bar queued
   again (song_stutter_queue) at the next bar line */
int sthold_song(sthold_t *h, song_t *s, int bar, int st, uint32_t pos);
/* the repeats' low-pass at a level (Hz; 0 open) */
float sthold_lp(int level);

#endif

/* SPDX-License-Identifier: MPL-2.0 */
/* What the engine played, written out for a musician (host simulator only): with VNL_SCORE=PREFIX, PREFIX.log (each
   song's choices and, bar by bar, chord and voicing), and per song PREFIX-N.mid (every part as played, swing and
   lateness included) and PREFIX-N.musicxml (a quantized score: lead with chord symbols, keys, bass). */
#ifndef VNL_SCORE_H
#define VNL_SCORE_H
#include "song.h"

void score_song(const song_t *s, float now);            /* a song starts at `now` seconds since boot */
void score_step(const song_t *s, int bar, int st, const song_ev_t *ev, int n, float now);   /* one 16th's events */
void score_gap(float now, float secs);                  /* the song ended; silence follows */

#endif

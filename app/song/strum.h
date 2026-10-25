/* SPDX-License-Identifier: MPL-2.0 */
/* The strum engine: the guitars' strummed comping (CS_STRUM). A guitar shape (up to 6 strings, low to high, E2-E5)
   struck per 16th step in one of four patterns from the instrument spike: down strokes low to high, up strokes high to
   low on the "and"s (the top 3 or 4 strings), muted chucks on every string, a bossa thumb and pinches. The strings of
   a stroke 5-15 ms apart (quick) or 30-80 ms (slow: only the ballad, in half its songs). Pure functions. */
#ifndef SONG_STRUM_H
#define SONG_STRUM_H
#include "roll.h"
#include "plan.h"
#include "groove.h"

/* D-DU-UDU; chucks on 2 and 4; bossa (thumb and pinches, two bars); the ballad (three strokes a bar) */
enum { SP_DDU, SP_CHUCK, SP_BOSSA, SP_BALLAD, SP_N };

/* The song's pattern (seed stream 194, equal weights); *slow (may be NULL) is 1 for a slow ballad (half of them), 0
   for every other pattern. */
int strum_pattern(const song_roll_t *r, int *slow);

/* A guitar shape for the chord (pcs, root first; npcs of them): out[s] is string s's note (s 0 the low E string, 0
   a string not played), ascending, E2-E5 (MIDI 40-76). Chord tones only, the root on the lowest string, the 3rd and
   the 7th covered first; no note a semitone over another (pads_rub: a minor 2nd or 9th) or over the root; a movable
   hand shape (fretted notes within 4 frets, open strings free). Voice-led from prev (all 0: none): the top note moves
   the least. Returns the number of notes (4-6). */
int strum_voicing(const int *pcs, int npcs, const int prev[6], int out[6]);

/* V_CHORD events of one 16th step (bar and step from 0) striking shape (strum_voicing's). Writes nothing unless the
   block has L_KEYS. Every note ends by the pattern's next stroke; a chuck's notes carry pad 1 (the sampler's muted
   note). Returns the number of events written (at most max). */
int strum_step(const song_roll_t *r, const plan_block_t *b, int bar, int step, float step_samples, const int shape[6],
               song_ev_t *out, int max);

#endif

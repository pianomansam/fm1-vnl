/* SPDX-License-Identifier: GPL-3.0-or-later */
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

/* The song's pattern, a STRUM_PAT index (patterns_gen.h): drawn on seed stream 194 over the approved patterns, equal
   weights tilted by the rolled motion and each pattern's busyness (STRUM_PAT_BUSY, roll.h's motion_tilt; a live Motion
   change never re-draws it; a candidate only when the strum_pat override forces it); *slow (may be NULL) is 1 for a
   slow ballad (half of them), 0 for every other pattern. The harp on the keys plays the ballad, the bossa or the
   charleston only, drawn alike, every stroke rolled low to high, 12-25 ms a string (a forced other pattern: its
   draw); harmony_step centers each stroke on its beat. */
int strum_pattern(const song_roll_t *r, int *slow);

/* A drum fill's bar (the block's last, PB_FILL_LAST and not PB_DROP_LAST: a drop bar keeps the pattern) plays a fill
   figure (Sam, 2026-11-04): the ballad its own (STRUM_BALLAD_FILL); every other pattern the song's draw on seed stream
   199, half the songs each: 0 step back (a strike on 1, then space for the fill), 1 drive (pushes into the change:
   down on the "a"s, up on the "and"s); the bossa their thumb-and-pinch versions (STRUM_FILL_STEP, STRUM_FILL_DRIVE).
   The workbench's strum_fill override forces either. */
int strum_fill_drive(const song_roll_t *r);

/* A guitar shape for the chord (pcs, root first; npcs of them): out[s] is string s's note (s 0 the low E string, 0
   a string not played), ascending, E2-E5 (MIDI 40-76). Chord tones only, the root on the lowest string, the 3rd and
   the 7th covered first; no note a semitone over another (pads_rub: a minor 2nd or 9th) or over the root; a movable
   hand shape (fretted notes within 4 frets, open strings free). Voice-led from prev (all 0: none): the top note moves
   the least; no note over hi (76 = E5 for the whole neck; lower where the keys make room for the lead). Returns the
   number of notes (4-6). */
int strum_voicing(const int *pcs, int npcs, const int prev[6], int out[6], int hi);

/* V_CHORD events of one 16th step (bar and step from 0) striking shape (strum_voicing's). Writes nothing unless the
   block has L_KEYS. Every note ends by the pattern's next stroke. pad carries the string: bit 0 a chuck (the
   sampler's muted note), bits 1-3 the string, 1 (low E) to 6 (the voices play each string on its own voice, a
   re-strike damping its last note). Returns the number of events written (at most max). */
int strum_step(const song_roll_t *r, const plan_block_t *b, int bar, int step, float step_samples, const int shape[6],
               song_ev_t *out, int max);

#endif

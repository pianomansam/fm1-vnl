/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The melody: one generated motif per song, developed head-solos-head over 4-bar lead phrases (exact restatement,
   variant, new). No ear candy (Sam, 2026-10-29: "Remove the ear candy part completely"): nothing emits V_CANDY.
   No clashes: every note in the bar's chord-scale and never a semitone from a chord tone or a walking bass's
   approach note (R25), chord tones on steps 0 and 8, no avoid notes, no note across a chord change. Notes are 5-25
   ms late (no swing). Pure functions of inputs.

   A lead phrase is 4 bars of a block with L_LEAD. Phrase layout: the head (motif or its development) from bar 1,
   the answer (the head's rhythm, moved onto the chord of bar 3, new ending; sometimes a rest) from bar 3. A 1-bar
   head leaves bars 2 and 4 open. */
#ifndef SONG_MELODY_H
#define SONG_MELODY_H
#include <stdint.h>
#include "roll.h"
#include "plan.h"
#include "groove.h"

enum { PH_STATE, PH_VARIANT, PH_NEW };

/* Onsets in 16ths within 1-2 bars (ascending), intervals in scale steps from the first note (step[0] = 0),
   lengths in 16ths. first: the first note (MIDI), so a restatement starts from the same scale degree. */
typedef struct { uint8_t n; uint8_t at[16]; int8_t step[16]; uint8_t len[16]; uint8_t first; } motif_t;

/* The song's motif: rhythm from one-beat cells, about a third of beats played, first note a guide tone of the
   first chord under it, pentatonic-leaning, 3-8 notes. Built to be legal under the chords of every lead phrase
   (seed streams 130+). The sequencer passes salt = r->seed (melody_plan assumes it). */
void melody_motif(const song_roll_t *r, uint32_t salt, motif_t *m);
/* The same over a given plan (melody_motif uses plan_build(r)): for a plan merged from a boundary on. */
void melody_motif_for(const song_roll_t *r, const song_plan_t *p, uint32_t salt, motif_t *m);

/* Kind per lead phrase (song order): first and last PH_STATE, the rest drawn toward 50/15/35 song-wide. */
typedef struct { uint8_t kind[32]; } melody_phrases_t;
void melody_plan(const song_roll_t *r, const song_plan_t *p, melody_phrases_t *ph);
/* The same with the motif given (melody_plan builds melody_motif(r, r->seed)), so a caller holding it builds it once. */
void melody_plan_for(const song_roll_t *r, const song_plan_t *p, const motif_t *m, melody_phrases_t *ph);

/* Lead (V_LEAD, 64-84 shifted by the mood's lead_shift) events of one 16th step (bar from 0, step 0-15). Callable
   for any (bar, step) in any order. Returns the number written (at most max). */
int melody_step(const song_roll_t *r, const song_plan_t *p, const melody_phrases_t *ph, const motif_t *m, int bar,
                int step, float step_samples, uint32_t salt, song_ev_t *out, int max);

#endif

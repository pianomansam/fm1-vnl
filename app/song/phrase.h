/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The sustained lead's phrase (sustained-lead design, 2026-10-24): one 1-2-bar motif per song and an 8-bar question
   and answer built from it, planned in scale degrees and 16ths only (no pitches, no chords; lead.c re-anchors them).
     bars 1-2  the motif (a 1-bar motif plays, then repeats a step higher)
     bar  3    bar 1 with one nudge (a note moved a step, the last interval altered, the second half's contour
               inverted, or a note delayed an 8th)
     bar  4    the question: a step into the chord's 3rd or 5th, held, then at least half a bar of rest
     bars 5-6  bars 1-2, literally
     bar  7    bar 1 with a different nudge
     bar  8    the close: by step to the arrival (degree 0 mod 7), held, then at least half a bar of rest
   Beat 1 rests (no onset) in at least one of every two bars. Pure: a function of (seed, block, pass) only. */
#ifndef SONG_PHRASE_H
#define SONG_PHRASE_H
#include <stdint.h>

/* Onsets in 8ths from the motif's start, lengths in 8ths, scale steps from the previous note (step[0] is from the
   anchor), and ornament flags (1: a passing or neighbor note that low motion may drop). */
typedef struct { uint8_t n; uint8_t at[8]; uint8_t len[8]; int8_t step[8]; uint8_t orn[8]; uint8_t bars; } motif_t2;

void phrase_motif(uint32_t seed, motif_t2 *m);

enum { PR_MOTIF, PR_VARIED, PR_QUESTION, PR_CLOSE, PR_ARRIVAL };   /* PR_CLOSE: the step into a question or arrival */

/* on: 1 for a planned note; at: 16th in the bar; len: 16ths (may run past the bar); deg: scale degree from the
   phrase's anchor (0 = the anchor chord tone); orn: 1 for an ornament. */
typedef struct { uint8_t on; uint8_t at; uint8_t len; int8_t deg; uint8_t role; uint8_t orn; } phrase_note_t;

/* The planned notes starting in bar bar_in_phrase (0-based) of an 8-bar phrase; returns how many (at most max).
   phrase_bars < 8 plays half a phrase (4 bars): the question, or the answer when last_block. Bars past the phrase
   are empty. */
int phrase_bar(uint32_t seed, int block, int pass, int bar_in_phrase, int phrase_bars, int last_block,
               phrase_note_t *out, int max);

#endif

/* SPDX-License-Identifier: MPL-2.0 */
/* The lead (lead design, 2026-10-22): it plays like its instrument. A percussive lead (vibes, kalimba, guitar lick)
   plays the hybrid: one shaped arpeggio per song over the keys' voicing an octave up, landing on a voice-leading
   skeleton note at each chord change (on 1, late, pushed an 8th early, or after a beat's rest). A sustained lead
   (flute, sax, muted trumpet) sings the skeleton line with the song's two rhythm cells and a breath at each 4-bar
   phrase's end. It plays in the plan's lead blocks (L_LEAD, with keys, not the intro or outro). Notes keep the melody's rules: in the
   chord-scale, no avoid notes, chord tones on steps 0 and 8; a note pushed across a chord change is legal over both.
   Planned a bar at a time; all state in lead_state_t. */
#ifndef SONG_LEAD_H
#define SONG_LEAD_H
#include <stdint.h>
#include "roll.h"
#include "plan.h"
#include "groove.h"

typedef struct {
    int on[16], len[16];          /* the bar's onsets (MIDI, 0 none) and lengths in 16ths */
    unsigned acc;                 /* accented onsets */
    int last;                     /* the last skeleton note (0: none yet) */
    int anchor;                   /* the sung line's note at its phrase's start: the arc's base */
    int tied;                     /* the next bar's landing was pushed into this one */
    int active;                   /* the lead sounds in the planned bar (an onset, or a note pushed in) */
    int onsets;                   /* lead onsets that start in the planned bar */
    int chosen;                   /* the song's arpeggio pattern is chosen */
    int pat, rhy, rev, swap;
} lead_state_t;

/* 1 where the lead plays: blocks with L_LEAD and L_KEYS, not intro or outro */
int lead_plays(const plan_block_t *b);
/* Plans one bar (call at step 0). keys, nkeys: this bar's and the next bar's sounding keys voicings (4 notes, 0
   none); pcs: this bar's chord, root first. Over percussive keys every note sits a 4th or more above the keys' top
   (of both chords for a note that sounds on into the next), at most E6, or drops; with a pad the lead keeps to
   C4-B5, C4-C5 when the pad sits above. Returns the number of lead onsets that start in the bar (0 where the lead doesn't play). */
int lead_plan_bar(lead_state_t *ls, const song_roll_t *r, const plan_block_t *b, int bar, const int keys[4],
                  const int nkeys[4], const int *pcs, int np);
/* The lead's events of one step of the planned bar. */
int lead_step(const lead_state_t *ls, int step, float step_samples, song_ev_t *out, int max);

#endif

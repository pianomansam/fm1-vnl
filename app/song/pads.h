/* SPDX-License-Identifier: MPL-2.0 */
/* The pad layer (pads design, 2026-10-24): a quiet background of two parts on V_PAD. The chord part: 3 notes (2 where
   the rules force it) in the mid range, the chord's guide tones first, voice-led from chord to chord, over percussive
   keys only. The solo note: one high note a bar, the scale's 1st, 2nd, 3rd or 7th, kept while it stays legal, in the
   blocks that roll "in". Pure functions; song.c plans and emits them on the pad voice's lanes (voices.h). */
#ifndef SONG_PADS_H
#define SONG_PADS_H
#include "roll.h"

/* The layer as song.c plans it: the last chord part (by lane, 0 silent), the last solo note (0 none yet), and the
   4-bar group's notes, by bar and lane (lanes 1-3 the chord part, 4 the solo note). Zero it at a song's start. */
typedef struct { int chord[3]; int solo; int note[4][4]; } pads_state_t;

/* 1 where the solo note plays in this plan block (an index into song_plan_t.b), 0 where it sits out: per song and
   block (seed stream 190), "in" for 60% of grooves, 50% of intros, breaks and outros, 30% under the lead. */
int pads_section_in(const song_roll_t *r, int block, int section);

/* A semitone rub between two notes: the upper a minor 2nd (or a minor 9th, ...) above the lower. A major 7th is not
   one (the maj7 chord the keys voice). */
int pads_rub(int a, int b);

/* The chord part for a chord (pcs, root first): its notes by lane into out (lane k+1; 0 silent), voice-led from prev
   (the same, all 0 for none). Guide tones first (3rd and 7th), then the 5th or the 9th; MIDI 55-72; common tones held,
   the rest moving to the nearest legal tone, the top voice smoothest. No note rubs a keys note (keys: 4, 0 none) or a
   lead note, or sits a minor 9th over the bass (bass_pc), nor two of its own. out may be prev. Returns the count: 3, or
   2 where no 3 are legal (fewer only if nothing is). A pitch common to prev and out keeps its lane (it rings on).
   song.c passes the lead's notes of a 4-bar group's first bar only (the later bars' are not planned yet); there the
   chord part's avoidance of the lead rests on its notes being chord tones while the lead keeps to the chord-scale
   (the clash test enforces it). */
int pads_chord(const song_roll_t *r, const int keys[4], const int *pcs, int np, unsigned bass_pc,
               const int *lead_notes, int nlead, const int prev[3], int out[3]);

/* The A/B switch's chord part (song_set_pads_double): the keys' voicing doubled, its notes in the chord part's range
   (MIDI 55-72; one outside it is skipped) by voice position, the lowest on lane 1, the top 3 where 4 fit; a pitch
   common to prev keeps its lane (it rings on). out may be prev. Returns the count. */
int pads_double_keys(const int keys[4], int out[3]);

/* The bar's solo note (0: none legal): the scale's 1st, 2nd, 3rd or 7th degree, MIDI 72-84, at least 3 semitones over
   chord_top (0: no chord part), no avoid note over pcs, no rub with the keys or the bar's bass notes (nbass of them;
   over the bass a major 7th rubs too, unless the bass is the root). prev (0: none) is kept if legal; otherwise the
   smallest move from it (from the tonic for none), ties to the tonic, the 3rd, the 2nd, the 7th. */
int pads_solo(const song_roll_t *r, int key, int mode, const int *pcs, int np, const int keys[4], const int *bass,
              int nbass, int chord_top, int prev);

#endif

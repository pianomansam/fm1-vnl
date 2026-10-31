/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The pad layer (pads design, 2026-10-24, and its revision 2): a quiet background of two parts on V_PAD, one or the
   other in a section, never both. The chord part: 2 notes (the design's budget cut (c): the 3rd and the 7th first) in
   the mid range, voice-led from chord to chord, over percussive keys only. The solo note: one high note a bar, the
   scale's 1st, 2nd, 3rd or 7th, kept while it stays legal, on the song's rolled voice (or none). Pure functions;
   song.c plans and emits them on the pad voice's lanes (voices.h). */
#ifndef SONG_PADS_H
#define SONG_PADS_H
#include "roll.h"
#include "plan.h"

/* The layer as song.c plans it: the last chord part (by lane, 0 silent), the last solo note (0 none yet), the
   4-bar group's notes, by bar and lane (lanes 1-3 the chord part, 4 the solo note), and its level by bar (PF_*: the
   fade over a section line, song_pad_level). Zero it at a song's start. */
enum { PF_SILENT, PF_ON, PF_IN, PF_OUT };   /* level 0; 1; rising 0 to 1 over the bar; falling 1 to 0 over it */
typedef struct { int chord[3]; int solo; int note[4][4]; uint8_t fade[4]; } pads_state_t;

/* The song's solo voice (seed stream 191): INS_NONE (no solo note) in 40% of songs; otherwise the formant choir
   (INS_CHOIR, in the vox pad's place since 2026-10-31: the same draw), the flute, the organ or the strings, equally
   likely among those allowed: never the flute when the set's lead is the flute, nor the organ over organ keys. Reads
   the seed and the set only. */
int pads_solo_voice(const song_roll_t *r);

/* The pad layer's part in this plan block (an index into song_plan_t.b), where the keys play: 0 none, 1 the chord
   part, 2 the solo note. Per song and block (seed stream 190). Over struck keys (sustained_keys 0) the chord part, or
   the solo note in 35% of intros and breaks, 25% of grooves, 15% under the lead, 30% of outros; over sustained keys
   the solo note in 60% of grooves, 50% of intros, breaks and outros, 30% under the lead, otherwise none. A song with
   no solo voice (pads_solo_voice) never takes 2. */
int pads_section_part(const song_roll_t *r, int block, int section, int sustained_keys);

/* The pad layer plays in this plan block (an index into p->b), in a song that has one (song_pad_on): about 2 sections
   in 3. In by section: 45% of intros, 75% of grooves, 85% under the lead, 40% of breaks, 60% of outros, never out for
   more than 2 blocks in a row (the third is in). Per song and block (seed stream 196, salted by pad_salt as the
   section parts are). Sam, 2026-10-29: "add the ability for the pad part to drop out of certain song sections".
   song.c fades it over the section line (song_pad_level): out over the bar before a block without it, in over the
   first bar back. */
int song_pad_in(const song_roll_t *r, const song_plan_t *p, int block);

/* A semitone rub between two notes: the upper a minor 2nd (or a minor 9th, ...) above the lower. A major 7th is not
   one (the maj7 chord the keys voice). */
int pads_rub(int a, int b);

/* The chord part for a chord (pcs, root first): its notes by lane into out (lane k+1; 0 silent), voice-led from prev
   (the same, all 0 for none). 2 notes (budget cut (c)): the guide tones first (3rd and 7th), then the 5th or the 9th;
   MIDI 55-72; common tones held,
   the rest moving to the nearest legal tone, the top voice smoothest. No note rubs a keys note (keys: 4, 0 none) or a
   lead note, or sits a minor 9th over the bass (bass_pc), nor two of its own. out may be prev. Returns the count: 2
   (0 only if no pair is legal). A pitch common to prev and out keeps its lane (it rings on).
   song.c passes the lead's notes of a 4-bar group's first bar only (the later bars' are not planned yet); there the
   chord part's avoidance of the lead rests on its notes being chord tones while the lead keeps to the chord-scale
   (the clash test enforces it). */
int pads_chord(const song_roll_t *r, const int keys[4], const int *pcs, int np, unsigned bass_pc,
               const int *lead_notes, int nlead, const int prev[3], int out[3]);

/* The A/B switch's chord part (song_set_pads_double): the keys' voicing doubled, its notes in the chord part's range
   (MIDI 55-72; one outside it is skipped) by voice position, the lowest on lane 1, the top 3 where 4 fit; a pitch
   common to prev keeps its lane (it rings on). out may be prev. Returns the count. */
int pads_double_keys(const int keys[4], int out[3]);

/* The bar's solo note (0: none legal): the scale's 1st, 2nd, 3rd or 7th degree, MIDI lo to lo + 12 (pads_solo_lo),
   at least 3 semitones over chord_top (0: no chord part in the bar), no avoid note over pcs, no rub with the keys or the bar's bass notes (nbass of them;
   over the bass a major 7th rubs too, unless the bass is the root). prev (0: none) is kept if legal; otherwise the
   smallest move from it (from the tonic for none), ties to the tonic, the 3rd, the 2nd, the 7th. */
int pads_solo(const song_roll_t *r, int key, int mode, const int *pcs, int np, const int keys[4], const int *bass,
              int nbass, int chord_top, int lo, int prev);

/* The solo note's lowest MIDI note for its voice: 84 for the strings (an octave up), 72 for the others. */
int pads_solo_lo(int voice);

#endif

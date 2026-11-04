/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The groove: the drum and bass events of one 16th step (swing, lateness, ghosts, fills, drops, bass styles).
   Also declares song_ev_t, the event every voice of the song engine uses. Pure function of its inputs. */
#ifndef SONG_GROOVE_H
#define SONG_GROOVE_H
#include <stdint.h>
#include "roll.h"
#include "plan.h"

/* V_PAD: the pad layer (strings or a warm pad), a pitched role on its own bus; V_RIDE: the shared ride (the sampled
   kits only; a synth kit ignores it) */
enum { V_CHORD, V_BASS, V_LEAD, V_CANDY, V_PAD, V_KICK, V_SNARE, V_HAT, V_OHAT, V_SHAKER, V_RIM, V_RIDE, V_N };
/* the bass styles: roots on the kick's hits; walking quarters (root, third, fifth, approach); pedal (the root held
   through each chord). No root-fifth line (Sam, 2026-11-04). */
enum { BS_ROOTS, BS_WALK, BS_PEDAL };

/* delay and len in samples at 44100 Hz (delay is late only); note is MIDI (drums 0); vel 1-127. pad: the bass's and
   the lead's push flag; on V_PAD, the note's lane (1-4, 0 for none: voices.h). */
typedef struct { uint8_t voice, note, vel; uint8_t pad; uint32_t delay; uint32_t len; } song_ev_t;

/* Per-song bass style from the set's bass instrument, the map and the mood (seed streams 110+; the workbench's
   bass_salt re-rolls it). */
int groove_bass_style(const song_roll_t *r);

/* The bass note (MIDI, 0 = none) struck at this step of `bar` over chord_pcs (root first), by the same rule
   groove_step plays: 0 unless the block has L_BASS. Lets the lead keep clear of what the bass sounds. */
int groove_bass_note(const song_roll_t *r, const plan_block_t *b, int bar, int step, const int *chord_pcs, int npcs);

/* Drum and bass events of one 16th step (bar from 0, step 0-15); step_samples is one 16th in samples.
   chord_pcs: the bar's chord, root first. Returns the number of events written (at most max).
   BS_WALK is a one-in-four 8-bar window: it plays where (bar / 8) % 4 equals a seed hash mod 4, and elsewhere
   the bass plays roots on the kick. */
int groove_step(const song_roll_t *r, const plan_block_t *b, int bar, int step, float step_samples,
                const int *chord_pcs, int npcs, uint32_t salt, song_ev_t *out, int max);

/* The ride (seed stream 193; sampled-kit songs only, RIDE_NONE otherwise): the warm kit rides throughout; another
   sampled kit rides throughout about 15% of its songs, one block about 50% (a lead section, the last groove, or the
   block the walking bass plays in; *section_block its index in plan_build's plan), stays on the hats otherwise. In
   a ride block the hat lanes' hits go to V_RIDE (open hats as accents); kick and snare as they were.
   *section_block is -1 unless RIDE_SECTION. */
enum { RIDE_NONE, RIDE_ALL, RIDE_SECTION };
int groove_ride(const song_roll_t *r, int *section_block);

#endif

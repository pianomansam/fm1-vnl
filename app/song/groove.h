/* SPDX-License-Identifier: MPL-2.0 */
/* The groove: the drum and bass events of one 16th step (swing, lateness, ghosts, fills, drops, bass styles).
   Also declares song_ev_t, the event every voice of the song engine uses. Pure function of its inputs. */
#ifndef SONG_GROOVE_H
#define SONG_GROOVE_H
#include <stdint.h>
#include "roll.h"
#include "plan.h"

enum { V_CHORD, V_BASS, V_LEAD, V_CANDY, V_KICK, V_SNARE, V_HAT, V_OHAT, V_SHAKER, V_RIM, V_N };
enum { BS_ROOTS, BS_ROOT_FIFTH, BS_WALK, BS_PEDAL };

/* delay and len in samples at 44100 Hz (delay is late only); note is MIDI (drums 0); vel 1-127. */
typedef struct { uint8_t voice, note, vel; uint8_t pad; uint32_t delay; uint32_t len; } song_ev_t;

/* Per-song bass style from the set's bass instrument, the map and the mood (seed streams 110+). */
int groove_bass_style(const song_roll_t *r);

/* Drum and bass events of one 16th step (bar from 0, step 0-15); step_samples is one 16th in samples.
   chord_pcs: the bar's chord, root first. Returns the number of events written (at most max).
   BS_WALK is a one-in-four 8-bar window: it plays where (bar / 8) % 4 equals a seed hash mod 4, and elsewhere
   the bass plays root and fifth. */
int groove_step(const song_roll_t *r, const plan_block_t *b, int bar, int step, float step_samples,
                const int *chord_pcs, int npcs, uint32_t salt, song_ev_t *out, int max);

#endif

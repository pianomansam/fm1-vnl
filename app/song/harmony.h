/* SPDX-License-Identifier: MPL-2.0 */
/* Harmony over time: the chord for each bar (reharmonized on repeat: a richer color only) and the
   chord part's events per 16th step, in one of four playing styles. Pure functions of their inputs. */
#ifndef SONG_HARMONY_H
#define SONG_HARMONY_H
#include <stdint.h>
#include "roll.h"
#include "plan.h"
#include "groove.h"

enum { CS_HELD, CS_STABS, CS_OFFBEATS, CS_ARP };

/* Pitch classes (root first, up to 5) of the chord at `bar` (from 0). pass is 1-based: 1 = the first time through
   the loop (bar / (n * bars_per_chord) + 1). Passes >= 2 may recolor or substitute exactly one chord of the pass
   (per-song chance 25-50%); dark (intro, outro) changes nothing here: it is a sound, not a chord. Returns the count. */
int harmony_chord_at(const song_roll_t *r, int bar, int pass, int dark, int *pcs);

int harmony_style(const song_roll_t *r);   /* CS_*, per song (seed stream 121) */

/* Chord-part events of one 16th step (bar and step from 0). Writes nothing unless the block has L_KEYS.
   voicing_state: the previous chord's 4 notes (all 0 = none), updated here for voice leading. Chords are rolled:
   notes 10-60 ms apart, bottom up (not for CS_ARP). Returns the number of events written (at most max). */
int harmony_step(const song_roll_t *r, const plan_block_t *b, int bar, int step, int pass, float step_samples,
                 int *voicing_state, uint32_t salt, song_ev_t *out, int max);

#endif

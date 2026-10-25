/* SPDX-License-Identifier: MPL-2.0 */
/* The drum kits: kick, snare, closed and open hat, shaker and rim, in four synth kits (KIT_DUSTY, KIT_SOFT, KIT_808,
   KIT_BRUSHES; library.h); a sampled kit (KIT_S_*) routes its hits to skits.c (and plays the ride, V_RIDE), and
   every call below covers both. The voices mix calls drums_sample() once per sample. Float only, no allocation. */
#ifndef ENGINE_DRUMS_H
#define ENGINE_DRUMS_H
#include "groove.h"

/* Everything silent, the default kit (dusty), the noise generator reseeded. */
void drums_init(void);
/* A song's start with nothing sounding: every lane cleared and the noise generator seeded from the song's seed (the
   same seed, the same hits); the kit kept. */
void drums_song(uint32_t seed);
/* The kit for the hits that follow (KIT_*; anything else is dusty). Hits already sounding keep playing (a sampled
   kit's on their own kit). */
void drums_set_kit(int kit);
/* Start a hit: e->voice V_KICK..V_RIM (V_RIDE too on a sampled kit; others ignored), e->vel 1-127 scales the level. A new hit on a voice
   restarts that voice (a kick under a kick retriggers; a hat chokes its own tail). */
void drums_on(const song_ev_t *e);
/* One sample of every sounding drum, summed. */
float drums_sample(void);
/* Every drum into a fast fade (under 40 ms). */
void drums_release(void);
/* Drums still sounding. */
int drums_active(void);
/* The kick's amplitude envelope, 0 to 1 (its velocity / 127 at the hit, falling with the kick, exactly 0 once the
   kick has ended): the ducking source for the lo-fi chain. A read of one float; call any time. */
float drums_kick_env(void);

#endif

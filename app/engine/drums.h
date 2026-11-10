/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The drum kits: kick, snare, closed and open hat, shaker and rim, in four synth kits (KIT_DUSTY, KIT_SOFT, KIT_808,
   KIT_BRUSHES; library.h). A mixed sampled kit (KIT_S_*, retired) has no hits of its own any more (their samples
   left the bank, 2026-11-10): its id marks a sampled kit's song (the ride section, the hats), its hits are silent,
   and the shipped kits' lanes (wb_drums.c, through drums_hook) play over it. The voices mix calls drums_render() a segment at a time (drums_sample(), a sample,
   for tests). Float only, no allocation. */
#ifndef ENGINE_DRUMS_H
#define ENGINE_DRUMS_H
#include "groove.h"

/* Everything silent, the default kit (dusty), the noise generator reseeded. */
void drums_init(void);
/* A song's start with nothing sounding: every lane cleared and the noise generator seeded from the song's seed (the
   same seed, the same hits); the kit kept. */
void drums_song(uint32_t seed);
/* The kit for the hits that follow (KIT_*; anything else is dusty). Hits already sounding keep playing. */
void drums_set_kit(int kit);
/* Start a hit: e->voice V_KICK..V_RIM (others, and any on a KIT_S_* kit, ignored), e->vel 1-127 scales the level.
   A new hit on a voice restarts that voice (a kick under a kick retriggers; a hat chokes its own tail). */
void drums_on(const song_ev_t *e);
/* One sample of every sounding drum, summed. */
float drums_sample(void);
/* n samples (at most 256) of the same into x (written): what a segment's render calls. */
void drums_render(float *x, uint32_t n);
/* Every drum into a fast fade (under 40 ms). */
void drums_release(void);
/* Drums still sounding. */
int drums_active(void);
/* The kick's amplitude envelope, 0 to 1 (its velocity / 127 at the hit, falling with the kick, exactly 0 once the
   kick has ended): the ducking source for the lo-fi chain. A read of one float; call any time. */
float drums_kick_env(void);

/* An outside drum source (the workbench's kits; default off): while set, drums_on/render/active/kick_env/release
   call it, so its drums go through the song's own bus, compressor, filters and ducking. NULL: the built-in kits.
   (drums_sample, for tests, and drums_set_kit/song/init stay the built-in's.) */
typedef struct {
    void (*on)(const song_ev_t *e);       /* a drum event (voice V_KICK..V_RIDE) */
    void (*render)(float *x, uint32_t n); /* writes n samples into x, as drums_render */
    int (*active)(void);
    float (*kick_env)(void);
    void (*release)(void);
} drums_hook_t;
void drums_hook(const drums_hook_t *h);

#endif

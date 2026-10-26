/* SPDX-License-Identifier: MPL-2.0 */
/* The sampled kits (KIT_S_*, library.h): every kit plays hits from the one drum pool (samples.h), processed at
   playback per kit (pitch, a low-pass and drive on the kit's bus) and per lane (the darkened Gogodze hits' own pitch
   and low-pass), with a real hi-hat choke. drums.c routes a sampled kit's hits here. Float only, no allocation,
   ISR-safe; the ADPCM decode is the sampler's (sampler.h). */
#ifndef ENGINE_SKITS_H
#define ENGINE_SKITS_H
#include <stdint.h>
#include "groove.h"

/* A kit: the hit per lane (kick, snare, hat, open hat, shaker, rim, ride: V_KICK..V_RIDE in order; -1 none: an open
   hat plays the closed hat), the kit's pitch (semitones), bus low-pass (Hz, 0 off) and drive (0 off), and its level
   in the mix; per lane, a level trim, a pitch and a one-pole low-pass of its own (Hz, 0 off). */
typedef struct {
    int8_t hit[7];
    float semis, lp_hz, drive, level;
    float trim[7];
    int8_t lsemis[7];
    uint16_t llp[7];
} skit_t;

const skit_t *skits_kit(int kit);       /* KIT_S_*; NULL for any other */
void skits_set(int kit);                /* the kit for the hits that follow (KIT_S_*; others ignored) */
void skits_on(const song_ev_t *e);      /* V_KICK..V_RIDE; vel 1-127 */
void skits_render(float *out, uint32_t n); /* n (at most 256) samples of every sounding hit, through their kits'
                                              buses, into out (written) */
float skits_sample(void);               /* one sample of the same */
void skits_clear(void);                 /* everything silent (a song's start) */
void skits_half(int on);                /* 1 (the default): render at half the rate; 0 the full rate (VX_OFF_HALF,
                                           the A/B); a change silences every hit */
void skits_release(void);               /* every hit into the choke's fade */
int skits_active(void);                 /* hits sounding */
float skits_kick_env(void);             /* the sampled kick's level, 0-1 (the ducking source; 0 with none) */
int skits_tiny(void);                   /* denormal states (0 always) */
int skits_lane_len(int kit, int voice); /* the lane's hit length at its pitch, in output samples (0 for none) */

#endif

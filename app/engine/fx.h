/* SPDX-License-Identifier: MPL-2.0 */
/* The lo-fi chain and the bed (see fx.c): dust colors the music, a small plate reverb takes per-role sends from the
   voices, and the bed (surface noise and ambience) sits beside them, neither dusted nor darkened. Float only, seeded
   noise, no malloc. */
#ifndef ENGINE_FX_H
#define ENGINE_FX_H
#include <stdint.h>

/* Everything silent and cleared: dust 0 (the 5 kHz low-pass only), no bed, the plate shut. */
void fx_init(void);
/* A song's chain: dust 0-3, noise 0 none / 1 vinyl / 2 tape, ambience 0 none / 1 rain, and the set's flavor (FL_*;
   with tape noise or the chillwave flavor it picks the wobbly tape dust, else warm vinyl). Opens the bed and the
   plate (fx_init leaves the plate shut: a dry engine, for the voices' own tests). Changes ramp over FX_RAMP_S (dust
   and each bed layer crossfade); a layer that comes in while the bed is closed lands at once. */
void fx_configure(int dust, int noise, int ambience, int flavor);
/* Land the dust ramps at once (nothing is sounding, so nobody hears the jump). */
void fx_land(void);
/* The bed fades out over FX_RAMP_S (the song's end). The plate rings on. */
void fx_close(void);
/* A stop: the bed and the plate's return fade out in 40 ms, then the plate is shut (true silence). */
void fx_stop(void);
/* In: l holds the music (mono, after the dark filter and the song gain) and r the plate's input (mono: the voices'
   per-role sends, voices.c). Out: l and r hold the music dusted, ducked by the kick envelope duck (0-1, read once
   per call) and soft-clipped below 0.7, plus the plate's stereo return, plus the bed. */
void fx_process(float *l, float *r, uint32_t n, float duck);
/* The bed is open or fading, or the plate's tail is still above -100 dB. */
int fx_active(void);
/* Tests: stored state values that are neither 0 nor at least 1e-20 in magnitude (denormal-bound); should be 0. */
int fx_tiny(void);

#define FX_RAMP_S 1.5f

#endif

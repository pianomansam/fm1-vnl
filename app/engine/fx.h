/* SPDX-License-Identifier: MPL-2.0 */
/* The lo-fi chain and the bed (see fx.c): dust colors the music, a small plate reverb takes per-role sends from the
   voices, a chorus takes the chords' bus and a tape delay its own sends, and the bed (surface noise and ambience)
   sits beside them, neither dusted nor filtered. Float only, seeded noise, no malloc. */
#ifndef ENGINE_FX_H
#define ENGINE_FX_H
#include <stdint.h>

/* Everything silent and cleared: dust 0 (the 5 kHz low-pass only), no bed, the plate, the chorus and the delay
   shut (and cleared), the subtle pump. */
void fx_init(void);
/* A song's chain: dust 0-3, noise 0 none / 1 vinyl / 2 tape, ambience 0 none / 1 rain, and the set's flavor (FL_*;
   with tape noise or the chillwave flavor it picks the wobbly tape dust, else warm vinyl). Opens the bed and the
   plate, the chorus and the delay (fx_init leaves them shut: a dry engine, for the voices' own tests). Changes ramp over FX_RAMP_S (dust
   and each bed layer crossfade); a layer that comes in while the bed is closed lands at once. */
void fx_configure(int dust, int noise, int ambience, int flavor);
/* A new song (before fx_configure): the bed's noise, clicks and gusts restart from seed if the bed is closed (it
   always is after the app's 2-4 s gap), else only its noise is reseeded; if quiet (no voice sounds), the dust's and
   the chorus's state and LFOs restart too; if the delay's tail is over (below -100 dB), its line and wow restart. A
   previous song's plate and delay tails ring on, but every LFO (the plate's, the chorus's, the delay's wow), the
   delay's time (it lands at the next fx_tempo) and the wet loop's phases restart, so a seed's wet is its own. */
void fx_song(uint32_t seed, int quiet);
/* Land the dust ramps and the chorus's depth at once (nothing is sounding, so nobody hears the jump). */
void fx_land(void);
/* The bed fades out over FX_RAMP_S (the song's end). The plate rings on. */
void fx_close(void);
/* A stop: the bed and the wet returns fade out in 40 ms, then the plate and the delay are shut (true silence). */
void fx_stop(void);
/* In: l holds the music (mono, after the role buses' filters and the song gain) and r the plate's input (mono: the
   voices' per-role sends, voices.c). Out: l and r hold the music dusted, ducked by the kick envelope duck (0-1, read
   once per call) and soft-clipped below 0.7, plus the wet returns (the plate's, and the chorus's and the delay's
   tails), plus the bed. */
void fx_process(float *l, float *r, uint32_t n, float duck);
/* fx_process with the role buses' wet inputs: ch the chords' bus (the chorus's input; its dry is already in l) and dl
   the tape delay's send. Either may be NULL (no input; tails ring on). */
void fx_process_buses(float *l, float *r, const float *ch, const float *dl, uint32_t n, float duck);
/* The engine's way in (voices.c): fx_process_buses with its inputs already summed per triple of the wet loop's
   third-rate steps: s3[t], c3[t] and d3[t] the sums of the plate's send, the chords' bus and the delay's send over
   the block's samples in triple t, where sample i is in triple (fx_phase() + i) / 3 (fx_phase() read before the
   call); r is output only. n at most 256. */
uint32_t fx_phase(void);
void fx_process_tri(float *l, float *r, const float *s3, const float *c3, const float *d3, uint32_t n, float duck);
/* The chorus's depth on the chords' bus, 0-1 (the chords patch's chorus; it ramps). */
void fx_chorus(float depth);
/* What the chords' dry is scaled by so the chorus's wet does not raise their level (1 with the chorus off or shut). */
float fx_chorus_dry(void);
/* The song's tempo in BPM: the tape delay's time, a dotted 8th (it glides if a tail rings, else lands). */
void fx_tempo(float bpm);
/* Tests and the cost bench: the delay off (not run; its input ignored). Kept through fx_init. */
void fx_wet_off(int delay_off);
/* The kick ducking's depth: about db dB at a full kick body. */
void fx_pump(float db);
/* The bed is open or fading, or the plate's or the delay's tail is still above -100 dB. */
int fx_active(void);
/* Tests: stored state values that are neither 0 nor at least 1e-20 in magnitude (denormal-bound); should be 0. */
int fx_tiny(void);

#define FX_RAMP_S 1.5f

#endif

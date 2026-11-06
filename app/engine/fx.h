/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The lo-fi chain and the bed (see fx.c): dust colors the music, a reverb takes per-role sends from the voices, a
   chorus takes the chords' bus and a tape delay its own sends, and the bed (surface noise and ambience) sits beside
   them, neither dusted nor filtered. The reverb and the chorus are Rings' (rings_fx.c); on the host, fx_rings(0) puts
   our plate and chorus in their place. "The plate" below names the reverb's slot, whichever fills it. Float only,
   seeded noise, no malloc. */
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
/* The engine's way in (voices.c): fx_process_buses with its inputs already summed per pair of samples (the wet
   loop's step, FX_PAIR): s2[t], c2[t] and d2[t] the sums of the plate's send, the chords' bus and the delay's send
   over the block's samples in pair t, where sample i is in pair (fx_phase() + i) / FX_PAIR (fx_phase() read before
   the call); r is output only. n at most 256. */
#define FX_PAIR 2u
uint32_t fx_phase(void);
void fx_process_pairs(float *l, float *r, const float *s2, const float *c2, const float *d2, uint32_t n, float duck);
/* The chorus's depth on the chords' bus, 0-1 (the chords patch's chorus; it ramps). */
void fx_chorus(float depth);
/* What the chords' dry is scaled by so the chorus's wet does not raise their level (1 with the chorus off or shut). */
float fx_chorus_dry(void);
/* The same for a bus whose send gives it a chorus this deep (the pad layer's: its send over the chords' depth). */
float fx_chorus_dry_at(float depth);
/* The song's tempo in BPM: the tape delay's time, a dotted 8th (it glides if a tail rings, else lands). */
void fx_tempo(float bpm);
/* Tests and the cost bench: the delay off (not run; its input ignored). Kept through fx_init. */
void fx_wet_off(int delay_off);
/* The reverb and chorus: Rings' (rings_fx.c; 1, the default) or, on the host only, our plate and chorus (0, VNL_FX=ours,
   for comparison; the device and web images have only Rings'). A switch starts the new pair silent. Kept through
   fx_init. */
void fx_rings(int on);
/* Tests: the dust's fast path on (1, the default) or off (0: every stage runs, the reference). Kept through fx_init. */
void fx_fast(int on);
/* The kick ducking's depth: about db dB at a full kick body. */
void fx_pump(float db);
/* The bed's level (vx_level_set's texture): g a linear gain (1, the default, leaves it exactly as it was), reached
   linearly over about 5 ms (at once while the bed is closed). Kept through fx_init. */
void fx_bed_level(float g);
/* The glue compressor's gain reduction now, in dB (0 for none; its make-up not counted). */
float fx_glue_db(void);
/* Tests and the cost bench: the glue off (1; kept through fx_init). */
void fx_glue_off(int off);
/* The bed is open or fading, or the plate's or the delay's tail is still above -100 dB. */
int fx_active(void);
/* Tests: stored state values that are neither 0 nor at least 1e-20 in magnitude (denormal-bound); should be 0. */
int fx_tiny(void);

#define FX_RAMP_S 1.5f

#endif

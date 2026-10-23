/* SPDX-License-Identifier: MPL-2.0 */
/* The song engine's voices and mix. Pitched roles play the instrument patches (patch.h); the bells and winds render
   with the placeholder FM until Task 5 gives them code. Drums are in drums.c. Float only. */
#ifndef ENGINE_VOICES_H
#define ENGINE_VOICES_H
#include <stdint.h>
#include "groove.h"
#include "roll.h"
#include "patch.h"

/* A plucked voice's Karplus-Strong line, in samples: one per voice, in the pool. Sized for note 28 (E1, 41.2 Hz):
   its period plus one for the interpolation. Lower notes play at 28's period. */
#define VX_KS_N 1072

/* The song's sound: one instrument per pitched role (INS_*; INS_NONE silences the role), the kit, the flavor and the
   lo-fi chain's fields (dust 0-3, noise 0-2, ambience 0-1), and the pump (VX_PUMP_*: the kick ducking's depth). */
enum { VX_PUMP_SUBTLE, VX_PUMP_MEDIUM, VX_PUMP_OBVIOUS, VX_PUMP_N };
typedef struct { uint8_t chords, bass, lead, candy, kit, flavor, dust, noise, ambience, pump; } vx_config_t;

/* Everything silent and cleared; the config back to its defaults (Rhodes, sub, vibes, music box; dusty kit, no
   chain: fx_init, the subtle pump), every bus's filters open, the drums' compressor at rest. */
void vx_init(void);
/* At song start, before the first event, and again when a request changes one of these mid-song: the set's
   instruments, kit and flavor (the chords' patch sets the chorus's depth, each role's patch its delay send; the
   song's comping sets the keys' level), the
   roll's RF_DUST, RF_NOISE and RF_AMBIENCE into the lo-fi chain (fx.h: the bed opens; dust and bed changes ramp, the
   dust landing at once if no voice sounds), RF_TEMPO into the delay's time, and the pump from the seed. Notes already sounding keep the
   instrument they were struck with through their release. */
void vx_configure(const song_roll_t *r);
void vx_bed_off(int on);                 /* 1: no surface noise or rain (host renders, VNL_BED=0) */
/* A new song (the app's song start, before its first event): the lo-fi chain's noise and its LFOs restart from the
   roll's seed wherever nothing is left to hear them, so a seed renders the same bed and dust whatever played before
   (a previous song's ringing tail still sounds), then vx_configure. */
void vx_song(const song_roll_t *r);
/* One pitched role's instrument (voice V_CHORD, V_BASS, V_LEAD or V_CANDY; others ignored). */
void vx_assign(int voice, int ins);
const vx_config_t *vx_config(void);
/* The drum kit (KIT_*); vx_configure sets it from the song's set. */
void vx_set_kit(int kit);
/* The kick's amplitude envelope, 0-1 (see drums.h): the lo-fi chain's ducking source. */
float vx_kick_env(void);
/* A role bus's section filter (the plan's moves, plan_moves): role V_CHORD, V_BASS, V_LEAD, V_CANDY, or any drum
   voice (V_KICK..V_RIM: the drums' bus). vx_set_filter's amount runs 0 (open) to 1 (closed): a one-pole low-pass from
   18 kHz down to 200 Hz. vx_set_hp's runs 0 (off) to 1 (a one-pole high-pass at about 600 Hz). A change lands in
   about 50 ms (a high-pass opening, in about 3 ms); with nothing sounding, at once. */
void vx_set_filter(int role, float amount);
void vx_set_hp(int role, float amount);
/* The song's pump (VX_PUMP_*): the kick ducking at about 1, 3 or 6 dB. vx_configure picks it per song (the seed's
   stream 170, a third each). */
void vx_set_pump(int pump);
/* Tests and the cost bench: switch effects off (VX_OFF_*; 0 for all on). Kept through vx_init. VX_OFF_HALF renders
   the keys and pads at the full rate (the reference for the half-rate path). */
enum { VX_OFF_COMP = 1, VX_OFF_FILTERS = 2, VX_OFF_CHORUS = 4, VX_OFF_DELAY = 8, VX_OFF_HALF = 16, VX_OFF_MUD = 32,
       VX_OFF_GLUE = 64 };
void vx_fx_off(unsigned mask);
/* Tests: the drums' compressor's gain reduction now, in dB (its make-up not counted). */
float vx_comp_db(void);
/* Tests: the mix's stored states (the mud high-pass, the drums' compressor, the buses' filters) that are neither 0
   nor at least 1e-20 in magnitude (denormal-bound); should be 0. */
int vx_tiny(void);
/* Queue an event; its delay counts from now, in samples at 44.1 kHz (the engine assumes that rate). 64 pending at
   most; when full the oldest by insertion order is dropped (not the one due first). When all voices of a pitched lane
   are busy, the victim fades out over about 4 ms first, so a stolen note starts up to ~5 ms late. The chords' pool is
   4 voices, 8 for a pad (its release crosses the next chord's swell), and a pad's swell takes at most half its note. */
void vx_event(const song_ev_t *e);
/* Adds the mix into l and r (sample-accurate event starts). The chords', the lead's and the ear candy's sums through
   the mud high-pass (about 135 Hz, 150 on a pad; VX_OFF_MUD), the keys at the song's comping level; per role bus: the
   drums through a compressor (a peak level, instant at a 32-sample block, released over 120 ms; 4:1 above the kit's
   threshold, at most 6 dB, 0.9 dB of make-up), each bus through its section filters; then the chain (fx.h: its glue
   compressor, VX_OFF_GLUE). */
void vx_render(float *l, float *r, uint32_t n);
/* Every voice into release (about 40 ms), the queue cleared; the bed fades out (FX_RAMP_S), the plate rings on: the
   song's end. */
void vx_release_all(void);
/* vx_release_all, and the bed and the plate's tail out in 40 ms too: a stop, true silence. */
void vx_stop(void);
/* The song's level ramp: g is clamped to [0, 1], reached linearly (exactly) over 20 ms. */
void vx_set_gain(float g);
/* Voices still sounding (queued events not counted). */
int vx_active(void);
/* Anything still to hear: voices, or the chain (the bed open or fading, the plate's tail). The app renders while
   this is nonzero and clears the engine (vx_init) at a song's start only when it is 0. */
int vx_sounding(void);

#endif

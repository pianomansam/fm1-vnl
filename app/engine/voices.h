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
   lo-fi chain's fields (dust 0-3, noise 0-2, ambience 0-1; stored for the chain, Task 7). */
typedef struct { uint8_t chords, bass, lead, candy, kit, flavor, dust, noise, ambience; } vx_config_t;

/* Everything silent and cleared; the config back to its defaults (Rhodes, sub, vibes, music box; dusty kit, no
   chain), the dark filter open. */
void vx_init(void);
/* At song start, before the first event: the set's instruments, kit and flavor, and the roll's RF_DUST, RF_NOISE and
   RF_AMBIENCE. Notes already sounding keep the instrument they were struck with through their release. */
void vx_configure(const song_roll_t *r);
/* One pitched role's instrument (voice V_CHORD, V_BASS, V_LEAD or V_CANDY; others ignored). */
void vx_assign(int voice, int ins);
const vx_config_t *vx_config(void);
/* The drum kit (KIT_*); vx_configure sets it from the song's set. */
void vx_set_kit(int kit);
/* The kick's amplitude envelope, 0-1 (see drums.h): the lo-fi chain's ducking source. */
float vx_kick_env(void);
/* The dark treatment, 0 (open) to 1: a low-pass on the music (down to 1.5 kHz at 1). It ramps linearly, 0 to 1 in
   3 s (about a bar at 70-90 BPM); with nothing sounding it lands at once. */
void vx_set_dark(float d);
/* Queue an event; its delay counts from now, in samples at 44.1 kHz (the engine assumes that rate). 64 pending at
   most; when full the oldest by insertion order is dropped (not the one due first). When all voices of a pitched lane
   are busy, the victim fades out over about 4 ms first, so a stolen note starts up to ~5 ms late. */
void vx_event(const song_ev_t *e);
/* Adds the mix into l and r (sample-accurate event starts). */
void vx_render(float *l, float *r, uint32_t n);
/* Every voice into release (about 40 ms), the queue cleared. */
void vx_release_all(void);
/* The song's level ramp: g is clamped to [0, 1], reached linearly (exactly) over 20 ms. */
void vx_set_gain(float g);
/* Voices still sounding (queued events not counted). */
int vx_active(void);

#endif

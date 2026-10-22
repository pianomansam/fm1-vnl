/* SPDX-License-Identifier: MPL-2.0 */
/* Placeholder voices and mix for the song engine (Plan 3 replaces them). Plain FM, sines and noise; float only. */
#ifndef ENGINE_VOICES_H
#define ENGINE_VOICES_H
#include <stdint.h>
#include "groove.h"

void vx_init(void);
/* Queue an event; its delay counts from now. 64 pending at most; when full the oldest pending one is dropped. */
void vx_event(const song_ev_t *e);
/* Adds the mix into l and r (sample-accurate event starts). */
void vx_render(float *l, float *r, uint32_t n);
/* Every voice into release (about 40 ms), the queue cleared. */
void vx_release_all(void);
/* The song's level ramp: smoothed linearly to g over 20 ms. */
void vx_set_gain(float g);
/* Voices still sounding (queued events not counted). */
int vx_active(void);

#endif

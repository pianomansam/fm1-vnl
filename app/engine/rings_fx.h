/* SPDX-License-Identifier: MIT */
/* Adapted from Mutable Instruments Rings, MIT, Emilie Gillet (see rings_fx.c). */
/* Rings' reverb and chorus, one step a call, for the lo-fi chain's wet loop (fx.c's fx_rings: on the host,
   VNL_FX=rings, in place of the plate and our chorus). Float in and out, 16-bit delay memory. */
#ifndef ENGINE_RINGS_FX_H
#define ENGINE_RINGS_FX_H
#include <stdint.h>

/* Host-only until a pair is chosen by ear: the device and web images leave Rings' pair out (fx_rings does nothing
   there; these calls are empty). */
#if (defined PLAT_HOST || defined __APPLE__) && !defined PLAT_WEB
#define RFX_ON 1
#else
#define RFX_ON 0
#endif

#if RFX_ON

/* The reverb's memory and state cleared, its LFOs restarted. */
void rfx_reverb_clear(void);
/* The chorus's memory cleared, its LFOs restarted. */
void rfx_chorus_clear(void);
/* Both LFOs of each restarted, their memory kept (a ringing tail rings on). */
void rfx_lfo_restart(void);
/* One step of the reverb at the plate's rate (11.025 kHz) on the mono send x: its two loops' outputs, left and
   right. */
void rfx_reverb(float x, float *l, float *r);
/* One step of the chorus at our chorus's rate (7.35 kHz) on the mono chords' bus x, at position p (0-1, the chords
   patch's chorus: Rings' amount p and depth 0.15 + 0.5 p): the wet sides, left and right, already scaled by the
   amount; the dry is scaled by rfx_chorus_dry(p). */
void rfx_chorus(float x, float p, float *l, float *r);
float rfx_chorus_dry(float p);
/* Tests: stored float states that are neither 0 nor at least 1e-20 in magnitude. */
int rfx_tiny(void);
#else
static inline void rfx_reverb_clear(void) {}
static inline void rfx_chorus_clear(void) {}
static inline void rfx_lfo_restart(void) {}
static inline void rfx_reverb(float x, float *l, float *r) { (void)x; *l = *r = 0.0f; }
static inline void rfx_chorus(float x, float p, float *l, float *r) { (void)x; (void)p; *l = *r = 0.0f; }
static inline float rfx_chorus_dry(float p) { (void)p; return 1.0f; }
static inline int rfx_tiny(void) { return 0; }
#endif

#endif

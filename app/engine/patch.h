/* SPDX-License-Identifier: MPL-2.0 */
/* The patch table: one patch per instrument (INS_*), each built on one of six voice models. */
#ifndef ENGINE_PATCH_H
#define ENGINE_PATCH_H
#include <stdint.h>

enum { M_EP, M_PLUCK, M_PAD, M_BELL, M_WIND, M_BASS, M_N };

/* p[] is the model's own (each model documents its layout in patch.c). The envelope, in seconds: att the linear rise
   to full; dec the fall from 1 toward sus (0..1) and rel the release after note-off, both as times to -60 dB (the
   voice is off by then). level scales the voice (Task 8 balances them). */
typedef struct {
    uint8_t model;
    float p[8];
    float att, dec, sus, rel;
    float level;
} vx_patch_t;

const vx_patch_t *vx_patch(int ins); /* NULL for INS_NONE or an unknown id */

#endif

/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The patch table: one patch per instrument (INS_*), each built on one of ten voice models. */
#ifndef ENGINE_PATCH_H
#define ENGINE_PATCH_H
#include <stdint.h>

enum { M_EP, M_PLUCK, M_PAD, M_BELL, M_WIND, M_BASS, M_CHIP, M_SUB, M_ENS, M_SAMP, M_N };

/* p[] is the model's own (each model documents its layout in patch.c). The envelope, in seconds: att the linear rise
   to full; dec the fall from 1 toward sus (0..1) and rel the release after note-off, both as times to -60 dB (the
   voice is off by then). level scales the voice: Task 8's mix evens a role's instruments out with it. chorus (0-1)
   is the chorus's depth on the chords' bus when this patch plays the chords (fx.c), and the pad bus's send into the
   chorus when it plays the pad layer; delay (0-1) the role's send to the tape delay. */
typedef struct {
    uint8_t model;
    float p[16];
    float att, dec, sus, rel;
    float level;
    float chorus, delay;
} vx_patch_t;

const vx_patch_t *vx_patch(int ins); /* NULL for INS_NONE or an unknown id */

#endif

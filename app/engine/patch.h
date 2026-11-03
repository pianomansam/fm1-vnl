/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The patch table: one patch per instrument (INS_*), each built on one of eight voice models. */
#ifndef ENGINE_PATCH_H
#define ENGINE_PATCH_H
#include <stdint.h>

/* (the Karplus-Strong pluck, the FM bell and the subtractive synth went 2026-10-31 with the instruments they played) */
enum { M_EP, M_PAD, M_WIND, M_BASS, M_CHIP, M_ENS, M_SAMP, M_TW, M_MODAL, M_N };

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
/* The tonewheel organ (INS_ORGAN, M_TW). On the keys, one of three drawbar registrations (ORG_A 888000000, ORG_B
   838000000 with percussion, ORG_C 008000000; the song's: lib_song_organ), played an octave up, or (high: the chords
   already sit high, harmony_keys_high) at its register with the 16' out. On the pad layer and the pad's solo lane,
   the one-stop 4'. vx_patch(INS_ORGAN) is the keys' ORG_A raised. */
enum { ORG_A, ORG_B, ORG_C, ORG_N };
const vx_patch_t *vx_organ_keys(int reg, int high);
const vx_patch_t *vx_organ_pad(void);

#endif

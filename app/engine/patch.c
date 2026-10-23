/* SPDX-License-Identifier: MPL-2.0 */
/* The patch table (see patch.h). Each model reads p[] its own way:
   M_EP (Rhodes, Wurli, felt piano; voices.c ep_*): 2-op FM with a decaying index.
     p[0] modulator ratio, p[1] index at velocity 0, p[2] index added at full velocity, p[3] the index's fall to
     -60 dB (s), p[4] the tine level at full velocity (a 14x partial, fading with the index), p[5] the strike: extra
     level at the hit at full velocity, gone in 0.25 s, p[6] the bark: a noise burst at full velocity (Wurli), gone in
     40 ms, p[7] a tone one-pole's cutoff in Hz at full velocity (60% of it at velocity 0; 0 for none).
   M_PAD (pad, vox pad; voices.c pad_*): two polyBLEP saws into an SVF.
     p[0] the detune in cents (the first saw down by it, the second up), p[1] the second saw's ratio, p[2] its level,
     p[3] the low-pass cutoff in Hz plus p[4] times the note's frequency, p[5] its damping k (1/Q), p[6] the vowel
     (0 u, 1 o, 2 a, 3 e, blended between), p[7] 1 for the formants (AMB-1's two: a vox pad) in place of the
     low-pass.
   Every other model still renders with the fallback FM voice, which reads p[0..4] as M_EP does. On the bass role the
   fallback is the sine bass, which uses only the envelope and level. Rows past the keys and pads are rough guesses
   (the bells are the old lead and candy bells); each model's task replaces its rows. */
#include <stddef.h>
#include "library.h"
#include "patch.h"

/* model, {p[0..7]}, att, dec, sus, rel, level */
static const vx_patch_t PATCHES[INS_NONE] = {
    [INS_RHODES] = {M_EP, {1.0f, 0.4f, 1.2f, 1.2f, 0.05f, 0.5f, 0.0f, 0.0f}, 0.002f, 4.5f, 0.0f, 0.08f, 1.0f},
    [INS_WURLI] = {M_EP, {2.0f, 0.8f, 2.0f, 1.5f, 0.0f, 0.6f, 1.0f, 0.0f}, 0.002f, 3.5f, 0.0f, 0.06f, 1.0f},
    [INS_FELT_PIANO] = {M_EP, {1.0f, 0.2f, 0.6f, 0.8f, 0.0f, 0.4f, 0.0f, 1100.0f}, 0.004f, 5.0f, 0.0f, 0.15f, 1.2f},
    [INS_NYLON] = {M_PLUCK, {1.0f, 0.4f, 0.8f, 0.7f, 0.0f}, 0.003f, 3.5f, 0.0f, 0.04f, 1.0f},
    [INS_PAD] = {M_PAD, {5.0f, 2.0f, 0.35f, 900.0f, 1.5f, 1.2f, 0.0f, 0.0f}, 0.6f, 4.0f, 0.85f, 0.8f, 1.0f},
    [INS_UPRIGHT] = {M_PLUCK, {1.0f, 0.3f, 0.6f, 0.7f, 0.0f}, 0.003f, 5.5f, 0.3f, 0.026f, 1.0f},
    [INS_ELEC_BASS] = {M_BASS, {1.0f, 0.5f, 1.0f, 0.55f, 0.0f}, 0.003f, 7.0f, 0.6f, 0.026f, 1.0f},
    [INS_SUB] = {M_BASS, {1.0f, 0.0f, 0.0f, 7.0f, 0.0f}, 0.003f, 7.0f, 1.0f, 0.026f, 1.0f},
    [INS_FLUTE] = {M_WIND, {1.0f, 0.2f, 0.4f, 7.0f, 0.0f}, 0.06f, 7.0f, 0.7f, 0.12f, 1.1f},
    [INS_MUTED_TPT] = {M_WIND, {1.0f, 0.8f, 1.2f, 4.0f, 0.0f}, 0.04f, 5.5f, 0.6f, 0.1f, 1.0f},
    [INS_SAX] = {M_WIND, {1.0f, 0.6f, 1.0f, 5.5f, 0.0f}, 0.05f, 6.2f, 0.65f, 0.1f, 1.0f},
    [INS_VIBES] = {M_BELL, {3.5f, 0.6f, 1.4f, 1.04f, 0.0f}, 0.003f, 2.5f, 0.0f, 0.026f, 1.0f},
    [INS_KALIMBA] = {M_BELL, {3.5f, 0.6f, 1.2f, 0.5f, 0.0f}, 0.003f, 1.4f, 0.0f, 0.026f, 1.0f},
    [INS_GUITAR_LICK] = {M_PLUCK, {1.0f, 0.5f, 0.9f, 1.0f, 0.0f}, 0.003f, 2.8f, 0.0f, 0.04f, 1.0f},
    [INS_MUSIC_BOX] = {M_BELL, {3.5f, 0.6f, 1.4f, 0.63f, 0.0f}, 0.003f, 1.04f, 0.0f, 0.026f, 1.0f},
    [INS_BELLS] = {M_BELL, {3.5f, 0.8f, 1.6f, 1.4f, 0.0f}, 0.003f, 3.5f, 0.0f, 0.026f, 1.0f},
    [INS_VOX_PAD] = {M_PAD, {4.0f, 2.0f, 0.35f, 0.0f, 0.0f, 0.0f, 0.6f, 1.0f}, 0.8f, 4.0f, 0.85f, 0.9f, 1.5f},
};

const vx_patch_t *vx_patch(int ins)
{
    return ins >= 0 && ins < INS_NONE ? &PATCHES[ins] : NULL;
}

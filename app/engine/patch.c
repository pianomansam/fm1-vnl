/* SPDX-License-Identifier: MPL-2.0 */
/* The patch table (see patch.h). Until Tasks 3-5 give the models their own code, every model renders with the
   fallback 2-op FM voice (voices.c), which reads p[] as:
     p[0] modulator ratio, p[1] index at velocity 0, p[2] index added at full velocity,
     p[3] the index's fall to -60 dB (s), p[4] the tine level at full velocity (a 14x partial, fading with the
     index).
   On the bass role the fallback is the sine bass, which uses only the envelope and level. The values start from the
   placeholder's (Rhodes = the old chord EP, the bells = the old lead and candy bells) and are rough guesses past that;
   each model's task replaces its rows. */
#include <stddef.h>
#include "library.h"
#include "patch.h"

/* model, {ratio, idx0, idxv, idx_t60, tine}, att, dec, sus, rel, level */
static const vx_patch_t PATCHES[INS_NONE] = {
    [INS_RHODES] = {M_EP, {1.0f, 0.35f, 0.9f, 1.96f, 0.12f}, 0.003f, 5.2f, 0.0f, 0.026f, 1.0f},
    [INS_WURLI] = {M_EP, {1.0f, 0.5f, 1.1f, 1.4f, 0.16f}, 0.003f, 4.1f, 0.0f, 0.026f, 1.0f},
    [INS_FELT_PIANO] = {M_EP, {1.0f, 0.15f, 0.4f, 1.4f, 0.03f}, 0.005f, 6.2f, 0.0f, 0.04f, 1.1f},
    [INS_NYLON] = {M_PLUCK, {1.0f, 0.4f, 0.8f, 0.7f, 0.0f}, 0.003f, 3.5f, 0.0f, 0.04f, 1.0f},
    [INS_PAD] = {M_PAD, {1.0f, 0.3f, 0.3f, 14.0f, 0.0f}, 0.3f, 8.0f, 0.6f, 0.4f, 1.3f},
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
    [INS_VOX_PAD] = {M_PAD, {2.0f, 0.2f, 0.2f, 14.0f, 0.0f}, 0.3f, 8.0f, 0.6f, 0.4f, 1.3f},
};

const vx_patch_t *vx_patch(int ins)
{
    return ins >= 0 && ins < INS_NONE ? &PATCHES[ins] : NULL;
}

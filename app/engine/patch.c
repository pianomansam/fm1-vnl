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
   M_PLUCK (nylon, guitar lick, upright; voices.c pluck_*): Karplus-Strong, a noise burst into a loop of a delay line,
     an averager and a damping one-pole.
     p[0] the fundamental's fall to -60 dB (s), p[1] the damping one-pole's cutoff in Hz plus p[5] times the note's
     frequency, at full velocity (60% of it at velocity 0), p[2] the burst's one-pole cutoff in Hz at full velocity
     (60% at velocity 0: a softer pluck is darker), p[3] the thump's level at full velocity (a sine at the fundamental;
     0 for none), p[4] the thump's fall to -60 dB (s), p[5] the damping's key tracking.
   M_BASS (sub, electric bass; voices.c bass_*): 2-op FM with an index falling from a struck peak, plus a sine an
     octave down. On the bass role a retrigger of the same patch is legato.
     p[0] modulator ratio, p[1] the settled index, p[2] the index added at the strike at full velocity, p[3] its fall
     to -60 dB (s), p[4] the sub-octave sine's level.
   M_BELL (vibes, kalimba, music box, bells; voices.c bell_*): AMB-1's 2-op FM bell, decaying freely (sus 0).
     p[0] modulator ratio (the bells' non-integer: inharmonic), p[1] index at velocity 0, p[2] index added at full
     velocity, p[3] the index's fall to -60 dB (s), p[4] the tremolo's depth (0..1; the vibes' motor), p[5] its rate
     in Hz, p[6] the decay's key tracking (octaves of decay time lost per octave above note 60).
   M_WIND (flute, sax, muted trumpet; voices.c wind_*): an oscillator blend into a two-pole low-pass that opens with
     the envelope, a mute, breath noise and a delayed vibrato (5 Hz, after 0.3 s, faded in over 0.6 s).
     p[0] the blend (0 sine, 1 triangle, 2 pulse, crossfaded between), p[1] the pulse's width, p[2] the low-pass
     cutoff at full envelope and velocity, as a multiple of the note's frequency (60% of it at velocity 0, 30% at
     envelope 0), p[3] the breath's level, p[4] its band's center in Hz, p[5] the vibrato's depth in cents, p[6] the
     mute: a band-pass center in Hz (0 for none), p[7] its damping k (1/Q: small is narrow).
   M_CHIP (chip chords, lead, bass, blips; voices.c chip_*): a NES-style channel at full rate and no filter (the dust
     colors it): a pulse at a fixed duty (its edges band-limited over one sample), a triangle stepped to 16 levels, or
     noise held for a 16th of the note's period; pitch on the semitone, except a vibrato.
     p[0] the wave (0 pulse, 1 triangle, 2 noise), p[1] the pulse's duty (0.125, 0.25 or 0.5), p[2] the vibrato's rate
     in Hz, p[3] its depth in cents (0 for none), p[4] the arpeggio (0: off, the only setting), p[5] a noise tick's
     level at the onset at full velocity (0 for none), p[6] its fall to -60 dB (s).
   M_SUB (Juno, organ, Moog bass, saw, square and sine leads, synth pluck; voices.c sub_*): frugal as a Juno-106, one
     oscillator, a sub-octave square and noise into a low-pass with its own envelope; mono and glide for the leads and
     the bass. On the chords a 2-pole patch whose highest cutoff stays at 8 kHz or under renders at half the rate.
     p[0] the wave (0 saw, 1 pulse, 2 sine, 3 triangle), p[1] the pulse's width (0.05-0.5), p[2] the sub-octave
     square's level, p[3] the noise's level, p[4] the low-pass's cutoff in Hz plus p[6] times the note's frequency (the
     key tracking), p[5] the resonance (0-1, clamped: the 2-pole's damping from Butterworth's down by 85%, the 4-pole's
     feedback to 3.6 of the 4 that self-oscillates), p[7] the filter envelope's depth in octaves over the cutoff at full
     velocity (half of it at velocity 0), p[8] its attack (s), p[9] its fall to -60 dB (s), p[10] 1 for the 4-pole (two
     SVFs, four equal poles, a ladder's slope; never at half the rate), p[11] 1 for mono (on the lead or the bass: a
     note while the last is held goes legato, gliding), p[12] the glide's time in ms (0: legato, no glide), p[13] a
     delayed vibrato's depth in cents (the winds': 5 Hz, after 0.3 s, faded in over 0.6 s; 0 for none). */
#include <stddef.h>
#include "library.h"
#include "patch.h"

/* model, {p[0..15]}, att, dec, sus, rel, level, chorus, delay. The chorus (on the chords' bus): the electric keys and
   the pads deep, the felt piano and the nylon a touch. The delay's send: leads and ear candy wet, keys a little,
   basses dry. The levels: the lead about 4-6 dB over the keys while it plays, in every set (song_test's
   test_lead_over_keys); the guitar lick rings longer and brighter (3 s, damping 4500 Hz plus 3x the note) so a pluck
   carries as a lead without its peaks standing far out. */
static const vx_patch_t PATCHES[INS_NONE] = {
    [INS_RHODES] = {M_EP, {1.0f, 0.4f, 1.2f, 1.2f, 0.05f, 0.5f, 0.0f, 0.0f}, 0.002f, 4.5f, 0.0f, 0.08f, 0.944f, 0.7f, 0.08f},
    [INS_WURLI] = {M_EP, {2.0f, 0.8f, 2.0f, 1.5f, 0.0f, 0.6f, 1.0f, 0.0f}, 0.002f, 3.5f, 0.0f, 0.06f, 0.88f, 0.6f, 0.08f},
    [INS_FELT_PIANO] = {M_EP, {1.0f, 0.2f, 0.6f, 0.8f, 0.0f, 0.4f, 0.0f, 1100.0f}, 0.004f, 5.0f, 0.0f, 0.15f, 0.668f, 0.2f, 0.06f},
    [INS_NYLON] = {M_PLUCK, {1.8f, 3000.0f, 2500.0f, 0.0f, 0.0f, 2.0f}, 0.002f, 6.0f, 0.0f, 0.08f, 1.62f, 0.15f, 0.08f},
    [INS_PAD] = {M_PAD, {5.0f, 2.0f, 0.35f, 900.0f, 1.5f, 1.2f, 0.0f, 0.0f}, 0.6f, 4.0f, 0.85f, 2.0f, 0.616f, 0.6f, 0.1f},
    [INS_UPRIGHT] = {M_PLUCK, {0.9f, 700.0f, 1500.0f, 0.5f, 0.08f, 3.0f}, 0.002f, 4.0f, 0.0f, 0.05f, 3.4f, 0.0f, 0.0f},
    [INS_ELEC_BASS] = {M_BASS, {1.0f, 0.6f, 2.4f, 0.12f, 0.0f}, 0.003f, 7.0f, 0.6f, 0.04f, 1.57f, 0.0f, 0.0f},
    [INS_SUB] = {M_BASS, {1.0f, 0.0f, 0.0f, 0.1f, 0.6f}, 0.008f, 7.0f, 1.0f, 0.06f, 1.0f, 0.0f, 0.0f},
    [INS_FLUTE] = {M_WIND, {0.25f, 0.5f, 3.0f, 0.13f, 1800.0f, 14.0f, 0.0f, 0.0f}, 0.06f, 7.0f, 0.75f, 0.12f, 1.228f, 0.0f, 0.3f},
    [INS_MUTED_TPT] = {M_WIND, {2.0f, 0.2f, 12.0f, 0.08f, 2200.0f, 10.0f, 1400.0f, 0.35f}, 0.04f, 5.5f, 0.7f, 0.1f, 1.56f, 0.0f, 0.3f},
    [INS_SAX] = {M_WIND, {1.6f, 0.35f, 5.0f, 0.11f, 1500.0f, 18.0f, 0.0f, 0.0f}, 0.05f, 6.2f, 0.7f, 0.1f, 1.4f, 0.0f, 0.25f},
    [INS_VIBES] = {M_BELL, {4.0f, 0.3f, 0.9f, 0.6f, 0.35f, 5.0f, 0.3f}, 0.002f, 3.0f, 0.0f, 0.15f, 1.414f, 0.0f, 0.3f},
    [INS_KALIMBA] = {M_BELL, {3.0f, 0.4f, 1.4f, 0.12f, 0.0f, 0.0f, 0.4f}, 0.002f, 1.4f, 0.0f, 0.06f, 1.69f, 0.0f, 0.35f},
    [INS_GUITAR_LICK] = {M_PLUCK, {3.0f, 4500.0f, 3500.0f, 0.0f, 0.0f, 3.0f}, 0.002f, 3.0f, 0.0f, 0.05f, 3.18f, 0.0f, 0.3f},
    [INS_MUSIC_BOX] = {M_BELL, {5.0f, 0.3f, 0.9f, 0.25f, 0.0f, 0.0f, 0.3f}, 0.002f, 1.2f, 0.0f, 0.05f, 1.1f, 0.0f, 0.4f},
    [INS_BELLS] = {M_BELL, {3.53f, 0.6f, 1.2f, 1.2f, 0.0f, 0.0f, 0.3f}, 0.003f, 3.5f, 0.0f, 0.2f, 0.87f, 0.0f, 0.35f},
    [INS_VOX_PAD] = {M_PAD, {4.0f, 2.0f, 0.35f, 0.0f, 0.0f, 0.0f, 0.6f, 1.0f}, 0.8f, 4.0f, 0.85f, 0.9f, 1.77f, 0.5f, 0.25f},
    /* the chip voices: provisional levels (the synths' Task 5 balances them in their sets) */
    [INS_CHIP_CHORDS] = {M_CHIP, {0.0f, 0.25f}, 0.004f, 3.0f, 0.6f, 0.12f, 0.5f, 0.0f, 0.08f},
    [INS_CHIP_LEAD] = {M_CHIP, {0.0f, 0.5f, 6.5f, 12.0f}, 0.003f, 1.5f, 0.3f, 0.1f, 0.7f, 0.0f, 0.3f},
    [INS_CHIP_BASS] = {M_CHIP, {1.0f}, 0.003f, 7.0f, 1.0f, 0.05f, 1.3f, 0.0f, 0.0f},
    [INS_CHIP_BLIP] = {M_CHIP, {0.0f, 0.125f, 0.0f, 0.0f, 0.0f, 0.6f, 0.02f}, 0.001f, 0.25f, 0.0f, 0.05f, 0.8f, 0.0f, 0.4f},
    /* the subtractive synths, warm (their cutoffs low): provisional levels (the synths' Task 5 balances them). The
       Juno: a saw and its sub, a gentle envelope, the chorus deep (the Juno's); the organ: a square and its sub
       (16' and 8'), no envelope; the Moog bass: a saw and its sub through the 4-pole, plucked open, a short glide; the
       leads mono and gliding; the synth pluck: a pulse snapped open and shut */
    [INS_JUNO] = {M_SUB, {0.0f, 0.5f, 0.4f, 0.0f, 600.0f, 0.15f, 1.0f, 1.0f, 0.005f, 0.8f}, 0.01f, 4.0f, 0.75f, 0.35f,
                  0.55f, 0.8f, 0.1f},
    [INS_ORGAN] = {M_SUB, {1.0f, 0.5f, 0.6f, 0.0f, 1200.0f, 0.0f, 1.0f}, 0.005f, 6.0f, 1.0f, 0.08f, 0.27f, 0.4f, 0.08f},
    [INS_MOOG_BASS] = {M_SUB, {0.0f, 0.5f, 0.5f, 0.0f, 200.0f, 0.35f, 1.0f, 3.0f, 0.001f, 0.35f, 1.0f, 1.0f, 30.0f},
                       0.003f, 1.5f, 0.5f, 0.06f, 1.78f, 0.0f, 0.0f},
    [INS_SAW_LEAD] = {M_SUB, {0.0f, 0.5f, 0.0f, 0.0f, 900.0f, 0.2f, 1.5f, 1.0f, 0.02f, 0.6f, 0.0f, 1.0f, 90.0f},
                      0.01f, 3.0f, 0.8f, 0.12f, 1.06f, 0.0f, 0.3f},
    [INS_SQUARE_LEAD] = {M_SUB, {1.0f, 0.5f, 0.0f, 0.0f, 800.0f, 0.1f, 1.5f, 0.8f, 0.02f, 0.6f, 0.0f, 1.0f, 60.0f,
                                 10.0f},
                         0.01f, 3.0f, 0.8f, 0.12f, 0.61f, 0.0f, 0.3f},
    [INS_SINE_LEAD] = {M_SUB, {2.0f, 0.5f, 0.0f, 0.0f, 3000.0f, 0.0f, 2.0f, 0.0f, 0.0f, 0.1f, 0.0f, 1.0f, 70.0f, 14.0f},
                       0.03f, 3.0f, 0.8f, 0.15f, 0.82f, 0.0f, 0.3f},
    [INS_SYNTH_PLUCK] = {M_SUB, {1.0f, 0.35f, 0.0f, 0.0f, 200.0f, 0.25f, 0.6f, 4.5f, 0.001f, 0.3f}, 0.002f, 1.2f, 0.0f,
                         0.1f, 1.4f, 0.0f, 0.35f},
};

const vx_patch_t *vx_patch(int ins)
{
    return ins >= 0 && ins < INS_NONE ? &PATCHES[ins] : NULL;
}

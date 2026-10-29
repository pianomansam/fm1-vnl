/* SPDX-License-Identifier: GPL-3.0-or-later */
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
   M_PLUCK (the upright; voices.c pluck_*): Karplus-Strong, a noise burst into a loop of a delay line,
     an averager and a damping one-pole.
     p[0] the fundamental's fall to -60 dB (s), p[1] the damping one-pole's cutoff in Hz plus p[5] times the note's
     frequency, at full velocity (60% of it at velocity 0), p[2] the burst's one-pole cutoff in Hz at full velocity
     (60% at velocity 0: a softer pluck is darker), p[3] the thump's level at full velocity (a sine at the fundamental;
     0 for none), p[4] the thump's fall to -60 dB (s), p[5] the damping's key tracking.
   M_BASS (sub, electric bass; voices.c bass_*): 2-op FM with an index falling from a struck peak, plus a sine an
     octave down. On the bass role a retrigger of the same patch is legato.
     p[0] modulator ratio, p[1] the settled index, p[2] the index added at the strike at full velocity, p[3] its fall
     to -60 dB (s), p[4] the sub-octave sine's level.
   M_BELL (kalimba, music box, bells; voices.c bell_*): AMB-1's 2-op FM bell, decaying freely (sus 0).
     p[0] modulator ratio (the bells' non-integer: inharmonic), p[1] index at velocity 0, p[2] index added at full
     velocity, p[3] the index's fall to -60 dB (s), p[4] the tremolo's depth (0..1; a vibes motor, none uses it now), p[5] its rate
     in Hz, p[6] the decay's key tracking (octaves of decay time lost per octave above note 60).
   M_WIND (the flute; voices.c wind_*): an oscillator blend into a two-pole low-pass that opens with
     the envelope, a mute, breath noise and a delayed vibrato (5 Hz, after 0.3 s, faded in over 0.6 s).
     p[0] the blend (0 sine, 1 triangle, 2 pulse, crossfaded between), p[1] the pulse's width, p[2] the low-pass
     cutoff at full envelope and velocity, as a multiple of the note's frequency (60% of it at velocity 0, 30% at
     envelope 0), p[3] the breath's level, p[4] its band's center in Hz, p[5] the vibrato's depth in cents, p[6] the
     mute: a band-pass center in Hz (0 for none), p[7] its damping k (1/Q: small is narrow).
   M_CHIP (the triangle bass; voices.c chip_*): a NES-style triangle channel at full rate (the dust colors it), stepped
     to 16 levels, pitch on the semitone, a gentle low-pass after its crush.
     p[0] the bit crush: the voice's output (after its envelope) rounded to this many bits over its own full level
     (2^(p[0]-1) steps from 0 to the note's level; 0 for none; 7: a few-bit DAC's grit and steppy decays); p[1] a
     one-pole low-pass after the crush at this multiple of the note's frequency (0 for none; Sam: more mellow).
   M_SUB (organ, saw lead, synth pluck; voices.c sub_*): frugal as a Juno-106, one oscillator and a sub-octave square
     into a 2-pole low-pass with its own envelope; mono and glide for the lead. On the chords a patch whose highest
     cutoff stays at 8 kHz or under renders at half the rate.
     p[0] the wave (0 saw, 1 pulse), p[1] the pulse's width (0.05-0.5), p[2] the sub-octave square's level, p[3] the
     low-pass's cutoff in Hz plus p[5] times the note's frequency (the key tracking), p[4] the resonance (0-1,
     clamped: the damping from Butterworth's down by 85%), p[6] the filter envelope's depth in octaves over the cutoff
     at full velocity (half of it at velocity 0), p[7] its attack (s), p[8] its fall to -60 dB (s), p[9] 1 for mono
     (on the lead: a note while the last is held goes legato, gliding), p[10] the glide's time in ms (0: legato, no
     glide).
   M_ENS (the pad layer's strings and warm pad; voices.c ens_*, pad_seg): frugal, in the spirit of a string machine,
     at a quarter of the rate: a polyBLEP saw and a partner saw a little sharp and quieter, into a fixed 2-pole
     low-pass (two one-poles); the ensemble's width is the chorus's (the patch's chorus: the pad bus's send into it).
     p[0] the partner's detune in cents (up), p[1] its level, p[2] the low-pass's cutoff in Hz plus p[3] times the
     note's frequency.
   M_SAMP (the Knight upright, the nylon, the Emilyguitar, the vibraphone; sampler.c): the bank's nearest zone
     (samples.h) retuned, through a one-pole tone, on its own envelope (att, dec, sus, rel as every model's).
     p[0] the bank (SB_*), p[1] the tone's cutoff in Hz at full velocity plus p[8] times the note's frequency (both 0:
     no tone; at most 7.5 kHz), p[2] velocity to tone (0-1: the cutoff at velocity 0 is 1 - p[2] of it), p[3] 1 for a
     muted note (the strum's chuck: the cutoff at 2.5 times the note, gone in 60 ms, released in 30), p[4] a delayed
     vibrato's depth in cents (the winds': 5 Hz after 0.3 s; 0 for none), p[5] the decay's key tracking (octaves of
     decay time lost per octave above note 60), p[6] the cutoff's fall after the strike, to this fraction of it (0:
     none), p[7] that fall's time constant (s). */
#include <stddef.h>
#include "library.h"
#include "patch.h"
#include "samples.h"

/* model, {p[0..15]}, att, dec, sus, rel, level, chorus, delay. The chorus (on the chords' bus): the electric keys and
   the pads deep, the felt piano and the nylon a touch. The delay's send: leads and ear candy wet, keys a little,
   basses dry. The levels: the lead about 4-6 dB over the keys while it plays, in every set (song_test's
   test_lead_over_keys). The leads Sam removed (2026-10-29: the muted trumpet, sax, guitar lick, string lead and
   cello) have no entry: vx_patch gives NULL, so a role assigned one stays silent (their ids are never reused). */
static const vx_patch_t PATCHES[INS_NONE] = {
    [INS_RHODES] = {M_EP, {1.0f, 0.4f, 1.2f, 1.2f, 0.05f, 0.5f, 0.0f, 0.0f}, 0.002f, 4.5f, 0.0f, 0.08f, 0.944f, 0.7f, 0.08f},
    /* the Wurli struck softer (Sam: "still has lots of attack"): less index, strike and bark, a 6 ms attack */
    [INS_WURLI] = {M_EP, {2.0f, 0.8f, 1.2f, 1.5f, 0.0f, 0.3f, 0.4f, 0.0f}, 0.006f, 3.5f, 0.0f, 0.06f, 0.932f, 0.6f, 0.08f},
    [INS_FELT_PIANO] = {M_EP, {1.0f, 0.2f, 0.6f, 0.8f, 0.0f, 0.4f, 0.0f, 1100.0f}, 0.004f, 5.0f, 0.0f, 0.15f, 0.716f, 0.2f, 0.06f},
    /* the sampled nylon (FreePats Spanish classical, CC0; it replaced the Karplus-Strong nylon): the spike's tone,
       6x the note at full velocity falling to 60% */
    [INS_NYLON] = {M_SAMP, {SB_NYLON, 300.0f, 0.55f, 0.0f, 0.0f, 0.45f, 0.6f, 0.8f, 6.0f}, 0.001f, 3.5f, 0.0f, 0.12f,
                   0.633f, 0.15f, 0.08f},
    [INS_PAD] = {M_PAD, {5.0f, 2.0f, 0.35f, 900.0f, 1.5f, 1.2f, 0.0f, 0.0f}, 0.6f, 4.0f, 0.85f, 2.0f, 0.616f, 0.6f, 0.1f},
    [INS_UPRIGHT] = {M_PLUCK, {0.9f, 700.0f, 1500.0f, 0.5f, 0.08f, 3.0f}, 0.002f, 4.0f, 0.0f, 0.05f, 3.4f, 0.0f, 0.0f},
    [INS_ELEC_BASS] = {M_BASS, {1.0f, 0.6f, 2.4f, 0.12f, 0.0f}, 0.003f, 7.0f, 0.6f, 0.04f, 1.57f, 0.0f, 0.0f},
    [INS_SUB] = {M_BASS, {1.0f, 0.0f, 0.0f, 0.1f, 0.6f}, 0.008f, 7.0f, 1.0f, 0.06f, 1.0f, 0.0f, 0.0f},
    /* (the flute 0.3 dB down, 2026-10-29: over set 9's strummed nylon, where the sax was, it read 10.2 dB over the
       keys, past the 8-10 the mix holds; its solo-lane trim up by as much, so the pad's solo flute is unchanged) */
    [INS_FLUTE] = {M_WIND, {0.25f, 0.5f, 3.0f, 0.13f, 1800.0f, 14.0f, 0.0f, 0.0f}, 0.06f, 7.0f, 0.75f, 0.12f, 1.12f, 0.0f, 0.3f},
    /* the vibraphone (VCSL soft mallets, CC0; it replaced the FM vibes, Sam 2026-10-29): as recorded, motor off, no
       tone filter; it rings free (10 s to -60 dB at C4, shorter up the keys as the recordings do), and a released
       note rings on about half a second, a held pedal's tail. The lead only. */
    [INS_VIBES] = {M_SAMP, {SB_VIBES, 0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.0f, 0.0f, 0.0f}, 0.001f, 10.0f, 0.0f, 0.8f,
                   0.6f, 0.0f, 0.3f},
    [INS_KALIMBA] = {M_BELL, {3.0f, 0.4f, 1.4f, 0.12f, 0.0f, 0.0f, 0.4f}, 0.002f, 1.4f, 0.0f, 0.06f, 1.614f, 0.0f, 0.35f},
    [INS_MUSIC_BOX] = {M_BELL, {5.0f, 0.3f, 0.9f, 0.25f, 0.0f, 0.0f, 0.3f}, 0.002f, 1.2f, 0.0f, 0.05f, 1.1f, 0.0f, 0.4f},
    [INS_BELLS] = {M_BELL, {3.53f, 0.6f, 1.2f, 1.2f, 0.0f, 0.0f, 0.3f}, 0.003f, 3.5f, 0.0f, 0.2f, 0.87f, 0.0f, 0.35f},
    [INS_VOX_PAD] = {M_PAD, {4.0f, 2.0f, 0.35f, 0.0f, 0.0f, 0.0f, 0.6f, 1.0f}, 0.8f, 4.0f, 0.85f, 0.9f, 1.77f, 0.5f, 0.25f},
    /* the triangle bass (a chip voice) */
    [INS_CHIP_BASS] = {M_CHIP, {7.0f, 5.0f}, 0.003f, 7.0f, 1.0f, 0.05f, 1.3f, 0.0f, 0.0f},
    /* the subtractive synths, warm (their cutoffs low), their levels balanced in their sets like the rest. The organ:
       a square and its sub (16' and 8'), no envelope; the saw lead mono and gliding; the synth pluck: a pulse snapped
       open and shut */
    [INS_ORGAN] = {M_SUB, {1.0f, 0.5f, 0.6f, 1200.0f, 0.0f, 1.0f}, 0.005f, 6.0f, 1.0f, 0.08f, 0.248f, 0.4f, 0.08f},
    [INS_SAW_LEAD] = {M_SUB, {0.0f, 0.5f, 0.0f, 900.0f, 0.2f, 1.5f, 1.0f, 0.02f, 0.6f, 1.0f, 90.0f},
                      0.01f, 3.0f, 0.8f, 0.12f, 1.09f, 0.0f, 0.3f},
    [INS_SYNTH_PLUCK] = {M_SUB, {1.0f, 0.35f, 0.0f, 200.0f, 0.25f, 0.6f, 4.5f, 0.001f, 0.3f}, 0.002f, 1.2f, 0.0f,
                         0.1f, 1.24f, 0.0f, 0.35f},
    /* the 80s FM keys: a 14x tine on the attack (a high ratio, a bright index falling fast) over a clean body */
    [INS_DX_EP] = {M_EP, {1.0f, 0.3f, 1.8f, 0.9f, 0.12f, 0.4f, 0.0f, 6500.0f}, 0.002f, 2.6f, 0.0f, 0.08f, 0.944f,
                   0.5f, 0.08f},
    /* the slap FM bass: the index strikes high and is gone in 150 ms */
    [INS_FM_BASS] = {M_BASS, {1.0f, 0.1f, 8.0f, 0.15f, 0.15f}, 0.002f, 4.0f, 0.6f, 0.05f, 1.3f, 0.0f, 0.0f},
    /* the pad layer (V_PAD; levels by ear in the pads plan's Task 3): soft strings, a slow swell and a deep ensemble;
       a warm synth pad, darker, slower and a little wetter */
    [INS_STRINGS] = {M_ENS, {7.0f, 0.4f, 1800.0f, 1.0f}, 0.6f, 4.0f, 0.85f, 1.5f, 1.0f, 0.7f, 0.1f},
    [INS_WARM_PAD] = {M_ENS, {5.0f, 0.5f, 900.0f, 1.5f}, 0.8f, 4.0f, 0.85f, 2.0f, 1.0f, 0.5f, 0.15f},
    /* the sampled instruments (M_SAMP) as the spike's demos Sam passed: the Knight upright (VCSL, CC0), its tone 9x the
       note falling to 55%; the Emilyguitar (Karoryfer, CC0), a clean jazz electric, brighter, a deeper chorus. Levels
       matched to the Rhodes on the keys (the Knight 0.5 dB under: its sets' lead over keys) */
    [INS_KNIGHT] = {M_SAMP, {SB_KNIGHT, 300.0f, 0.55f, 0.0f, 0.0f, 0.6f, 0.55f, 1.2f, 9.0f}, 0.001f, 7.0f, 0.0f,
                    0.35f, 0.571f, 0.1f, 0.06f},
    [INS_EMILY] = {M_SAMP, {SB_EMILY, 300.0f, 0.55f, 0.0f, 0.0f, 0.5f, 0.6f, 1.0f, 12.0f}, 0.001f, 5.0f, 0.0f, 0.12f,
                   0.648f, 0.3f, 0.1f},
};

const vx_patch_t *vx_patch(int ins)
{
    return ins >= 0 && ins < INS_NONE && PATCHES[ins].level > 0.0f ? &PATCHES[ins] : NULL; /* (a removed one: none) */
}

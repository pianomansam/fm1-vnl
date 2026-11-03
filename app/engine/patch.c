/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The patch table (see patch.h). Each model reads p[] its own way:
   M_EP (Rhodes, Wurli, felt piano; voices.c ep_*): 2-op FM with a decaying index.
     p[0] modulator ratio, p[1] index at velocity 0, p[2] index added at full velocity, p[3] the index's fall to
     -60 dB (s), p[4] the tine level at full velocity (a 14x partial, fading with the index), p[5] the strike: extra
     level at the hit at full velocity, gone in 0.25 s, p[6] the bark: a noise burst at full velocity (Wurli), gone in
     40 ms, p[7] a tone one-pole's cutoff in Hz at full velocity (60% of it at velocity 0; 0 for none).
   M_PAD (the chillwave pad's keys; voices.c pad_*): two polyBLEP saws into a low-pass SVF.
     p[0] the detune in cents (the first saw down by it, the second up), p[1] the second saw's ratio, p[2] its level,
     p[3] the low-pass cutoff in Hz plus p[4] times the note's frequency, p[5] its damping k (1/Q). (The vox pad and
     its formants went 2026-10-31: the choir, M_ENS, sings in its place.)
   M_BASS (sub, electric bass; voices.c bass_*): 2-op FM with an index falling from a struck peak, plus a sine an
     octave down. On the bass role a retrigger of the same patch is legato.
     p[0] modulator ratio, p[1] the settled index, p[2] the index added at the strike at full velocity, p[3] its fall
     to -60 dB (s), p[4] the sub-octave sine's level.
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
   M_ENS (the pad layer's strings, warm pad and choir; voices.c ens_*, pad_seg): frugal, in the spirit of a string
     machine, at a quarter of the rate: a polyBLEP saw and a partner saw a little sharp and quieter, into a fixed 2-pole
     low-pass (two one-poles); the ensemble's width is the chorus's (the patch's chorus: the pad bus's send into it).
     p[0] the partner's detune in cents (up), p[1] its level, p[2] the low-pass's cutoff in Hz plus p[3] times the
     note's frequency, p[4] 1 for the choir: its voices sing through the formant bank (voices.c's choir_run).
   M_SAMP (the Knight upright, the nylon, the Emilyguitar; sampler.c): the bank's nearest zone
     (samples.h) retuned, through a one-pole tone, on its own envelope (att, dec, sus, rel as every model's).
     p[0] the bank (SB_*), p[1] the tone's cutoff in Hz at full velocity plus p[8] times the note's frequency (both 0:
     no tone; at most 7.5 kHz), p[2] velocity to tone (0-1: the cutoff at velocity 0 is 1 - p[2] of it), p[3] 1 for a
     muted note (the strum's chuck: the cutoff at 2.5 times the note, gone in 60 ms, released in 30), p[4] a delayed
     vibrato's depth in cents (the winds': 5 Hz after 0.3 s; 0 for none), p[5] the decay's key tracking (octaves of
     decay time lost per octave above note 60), p[6] the cutoff's fall after the strike, to this fraction of it (0:
     none), p[7] that fall's time constant (s).
   M_TW (the tonewheel organ; voices.c tw_*), the approach and values after SLOOP's WHEEL engine: a sine per sounding
     drawbar, a percussion wheel, a key click and a slow rotary; on the keys' bus and the pad's, a scanner chorus.
     p[0..8] the drawbars 16' 5 1/3' 8' 4' 2 2/3' 2' 1 3/5' 1 1/3' 1' (0-8, 3 dB a step), p[9] the percussion's
     harmonic (0 none, 2 the 4', 3 the 2 2/3'; the bars 3 dB down with it, single trigger), p[10] its fall to -60 dB
     (s), p[11] the key click's level, p[12] 1 for the slow rotary, p[13] the horn's amplitude depth (the drum's
     half), p[14] the horn's Doppler (a fraction of the pitch; the drum's a quarter), p[15] semitones added to the
     note (12: the keys an octave up, within C7).
   M_MODAL (the modeled vibraphone; modal.h): three decaying modes, a mallet and the tube's bloom (no motor). The
     fundamental's -60 dB time is dec (shorter up the keys by p[9] octaves of time per octave); the common envelope
     only gates it (att the mallet's contact, rel the release).
     p[0], p[1] the 2nd and 3rd modes' ratios, p[2], p[3] their levels (over the fundamental's full) before the mallet,
     p[4] the mallet's cutoff in Hz at full velocity (a 4th-order roll-off on the modes' levels), p[5] velocity to it
     (the cutoff at velocity 0 is 1 - p[5] of it), p[6], p[7] the 2nd and 3rd modes' -60 dB times at C4 (s), p[8]
     their key tracking (octaves of time lost per octave above C4), p[9] the fundamental's, p[10] the fundamental's
     level at the strike (the bar's own share: the tube builds up the rest), p[11] the tube's build-up time constant at
     C4 (s; halved per octave up). */
#include <stddef.h>
#include "library.h"
#include "patch.h"
#include "samples.h"

/* model, {p[0..15]}, att, dec, sus, rel, level, chorus, delay. The chorus (on the chords' bus): the electric keys and
   the pads deep, the felt piano and the nylon a touch. The delay's send: leads and ear candy wet, keys a little,
   basses dry. The levels: the lead about 4-6 dB over the keys while it plays, in every set (song_test's
   test_lead_over_keys). The leads Sam removed (2026-10-29: the muted trumpet, sax, guitar lick, string lead and
   cello) have no entry: vx_patch gives NULL, so a role assigned one stays silent (their ids are never reused); nor
   have the ones no set played (2026-10-31: the upright, the kalimba, music box and bells, the saw lead and synth
   pluck), their models gone with them (an old snapshot's: library.c's lib_lead_approved, lib_bass_approved). */
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
    [INS_ELEC_BASS] = {M_BASS, {1.0f, 0.6f, 2.4f, 0.12f, 0.0f}, 0.003f, 7.0f, 0.6f, 0.04f, 1.57f, 0.0f, 0.0f},
    [INS_SUB] = {M_BASS, {1.0f, 0.0f, 0.0f, 0.1f, 0.6f}, 0.008f, 7.0f, 1.0f, 0.06f, 1.0f, 0.0f, 0.0f},
    /* (the flute 0.3 dB down, 2026-10-29: over set 9's strummed nylon, where the sax was, it read 10.2 dB over the
       keys, past the 8-10 the mix holds; its solo-lane trim up by as much, so the pad's solo flute is unchanged) */
    [INS_FLUTE] = {M_WIND, {0.25f, 0.5f, 3.0f, 0.13f, 1800.0f, 14.0f, 0.0f, 0.0f}, 0.06f, 7.0f, 0.75f, 0.12f, 1.12f, 0.0f, 0.3f},
    /* the vibraphone, modeled (M_MODAL; it replaced the VCSL sample, Sam 2026-11-03: the A/B indistinguishable, the
       motor cut as inaudible). Tuned on the sample's notes (its zones' first 0.4 s): the modes' levels and -60 dB
       times, the bloom; it rings free (10 s to -60 dB at C4), and a released note rings on about half a second, a
       held pedal's tail. The level matched to the sample's on the lead bus (seeds 9, 14 and 11 on sets 33, 4 and 8:
       a mean of -0.33 dB, each song within 1 dB; test_vibes_level). The lead only. */
    [INS_VIBES] = {M_MODAL, {4.0f, 9.9f, 0.18f, 0.9f, 2600.0f, 0.5f, 2.4f, 0.55f, 0.8f, 0.3f, 0.3f, 0.03f}, 0.001f,
                   10.0f, 1.0f, 0.8f, 0.541f, 0.0f, 0.3f},
    /* the triangle bass (a chip voice) */
    [INS_CHIP_BASS] = {M_CHIP, {7.0f, 5.0f}, 0.003f, 7.0f, 1.0f, 0.05f, 1.3f, 0.0f, 0.0f},
    /* the 80s FM keys: a 14x tine on the attack (a high ratio, a bright index falling fast) over a clean body */
    [INS_DX_EP] = {M_EP, {1.0f, 0.3f, 1.8f, 0.9f, 0.12f, 0.4f, 0.0f, 6500.0f}, 0.002f, 2.6f, 0.0f, 0.08f, 0.944f,
                   0.5f, 0.08f},
    /* the slap FM bass: the index strikes high and is gone in 150 ms */
    [INS_FM_BASS] = {M_BASS, {1.0f, 0.1f, 8.0f, 0.15f, 0.15f}, 0.002f, 4.0f, 0.6f, 0.05f, 1.3f, 0.0f, 0.0f},
    /* the pad layer (V_PAD; levels by ear in the pads plan's Task 3): soft strings, a slow swell and a deep ensemble;
       a warm synth pad, darker, slower and a little wetter */
    [INS_STRINGS] = {M_ENS, {7.0f, 0.4f, 1800.0f, 1.0f}, 0.6f, 4.0f, 0.85f, 1.5f, 1.0f, 0.7f, 0.1f},
    [INS_WARM_PAD] = {M_ENS, {5.0f, 0.5f, 900.0f, 1.5f}, 0.8f, 4.0f, 0.85f, 2.0f, 1.0f, 0.5f, 0.15f},
    /* the formant choir (a candidate, Sam's pick of the spike's "oo-aah", 2026-10-30): the ensemble's saw and a
       sharper partner, opened to 3.4 kHz (still a quarter-rate voice), through the choir's formant bank (p[4]:
       voices.c), a 1.6 s swell; otherwise the warm pad's, its level matched to it */
    [INS_CHOIR] = {M_ENS, {9.0f, 0.8f, 3400.0f, 0.0f, 1.0f}, 1.6f, 4.0f, 0.85f, 2.0f, 0.7745f, 0.5f, 0.15f},
    /* the sampled instruments (M_SAMP) as the spike's demos Sam passed: the Knight upright (VCSL, CC0), its tone 9x the
       note falling to 55%; the Emilyguitar (Karoryfer, CC0), a clean jazz electric, brighter, a deeper chorus. Levels
       matched to the Rhodes on the keys (the Knight 0.5 dB under: its sets' lead over keys) */
    [INS_KNIGHT] = {M_SAMP, {SB_KNIGHT, 300.0f, 0.55f, 0.0f, 0.0f, 0.6f, 0.55f, 1.2f, 9.0f}, 0.001f, 7.0f, 0.0f,
                    0.35f, 0.571f, 0.1f, 0.06f},
    [INS_EMILY] = {M_SAMP, {SB_EMILY, 300.0f, 0.55f, 0.0f, 0.0f, 0.5f, 0.6f, 1.0f, 12.0f}, 0.001f, 5.0f, 0.0f, 0.12f,
                   0.648f, 0.3f, 0.1f},
};

/* The organ's keys (Sam's picks, 2026-10-31): the slow rotary (horn 25%, Doppler 0.2%), a click, a 3 ms attack and an
   80 ms release; the bus chorus at 0.4 under the scanner (C3, voices.c). [reg][0] raised an octave, [reg][1] at its
   register with the 16' out (ORG_C has none: as it is). Levels: the chords' bus as the square organ's it replaced
   (active RMS within 0.5 dB, songs 101, 202 and 505 of set 18). */
#define ORG_KEYS(b16, b5, perc, fall, click, oct, lvl) \
    {M_TW, {b16, b5, 8, 0, 0, 0, 0, 0, 0, perc, fall, click, 1, 0.25f, 0.002f, oct}, 0.003f, 6.0f, 1.0f, 0.08f, lvl, \
     0.4f, 0.08f}
static const vx_patch_t ORGAN_KEYS[ORG_N][2] = {
    {ORG_KEYS(8, 8, 0, 0.0f, 0.25f, 12, 0.2290f), ORG_KEYS(0, 8, 0, 0.0f, 0.25f, 0, 0.3084f)},
    {ORG_KEYS(8, 3, 2, 1.0f, 0.25f, 12, 0.3650f), ORG_KEYS(0, 3, 2, 1.0f, 0.25f, 0, 0.5619f)},
    {ORG_KEYS(0, 0, 0, 0.0f, 0.15f, 12, 0.4027f), ORG_KEYS(0, 0, 0, 0.0f, 0.15f, 0, 0.4052f)},
};
/* the pad's organ, the 4' alone: no click, no percussion, the slow rotary lighter (horn 15%, Doppler 0.1%); the pads'
   swell, sustain and release, the warm pad's chorus send and a delay send; the scanner (C3, switched by section) on
   the pad bus. Its level: the warm pad's on the pad bus (within 0.5 dB, songs 319 and 152). */
static const vx_patch_t ORGAN_PAD = {M_TW, {0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 1, 0.15f, 0.001f}, 0.6f, 4.0f,
                                     0.85f, 1.5f, 0.797f, 0.5f, 0.12f};

const vx_patch_t *vx_organ_keys(int reg, int high)
{
    return &ORGAN_KEYS[reg >= 0 && reg < ORG_N ? reg : ORG_A][high != 0];
}

const vx_patch_t *vx_organ_pad(void) { return &ORGAN_PAD; }

const vx_patch_t *vx_patch(int ins)
{
    if (ins == INS_ORGAN) /* (the organ's own tables) */
        return &ORGAN_KEYS[ORG_A][0];
    return ins >= 0 && ins < INS_NONE && PATCHES[ins].level > 0.0f ? &PATCHES[ins] : NULL; /* (a removed one: none) */
}

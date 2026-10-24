/* SPDX-License-Identifier: MPL-2.0 */
/* The song engine's voices and mix. Pitched roles play the instrument patches (patch.h); the bells and winds render
   with the placeholder FM until Task 5 gives them code. Drums are in drums.c. Float only. */
#ifndef ENGINE_VOICES_H
#define ENGINE_VOICES_H
#include <stdint.h>
#include "groove.h"
#include "roll.h"
#include "patch.h"

/* A plucked voice's Karplus-Strong line, in samples: one per voice, in the pool. Sized for note 28 (E1, 41.2 Hz):
   its period plus one for the interpolation. Lower notes play at 28's period. */
#define VX_KS_N 1072

/* The song's sound: one instrument per pitched role (INS_*; INS_NONE silences the role), the kit, the flavor and the
   lo-fi chain's fields (dust 0-3, noise 0-2, ambience 0-1), and the pump (VX_PUMP_*: the kick ducking's depth). */
enum { VX_PUMP_SUBTLE, VX_PUMP_MEDIUM, VX_PUMP_OBVIOUS, VX_PUMP_N };
typedef struct { uint8_t chords, bass, lead, candy, pad, kit, flavor, dust, noise, ambience, pump; } vx_config_t;

/* Everything silent and cleared; the config back to its defaults (Rhodes, sub, vibes, music box, strings; dusty kit, no
   chain: fx_init, the subtle pump), every bus's filters open, the drums' compressor at rest. */
void vx_init(void);
/* At song start, before the first event, and again when a request changes one of these mid-song: the set's
   instruments, kit and flavor (the chords' patch sets the chorus's depth, each role's patch its delay send; the
   song's comping sets the keys' level), the
   roll's RF_DUST, RF_NOISE and RF_AMBIENCE into the lo-fi chain (fx.h: the bed opens; dust and bed changes ramp, the
   dust landing at once if no voice sounds), RF_TEMPO into the delay's time, and the pump from the seed. Notes already sounding keep the
   instrument they were struck with through their release. */
void vx_configure(const song_roll_t *r);
/* 1 if roll b differs from a in a field vx_configure reads (a request that landed mid-song needs it again). */
int vx_reconfigures(const song_roll_t *a, const song_roll_t *b);
void vx_bed_off(int on);                 /* 1: no surface noise or rain (host renders, VNL_BED=0) */
/* A new song (the app's song start, before its first event): the lo-fi chain's noise and its LFOs restart from the
   roll's seed wherever nothing is left to hear them, so a seed renders the same bed and dust whatever played before
   (a previous song's ringing tail still sounds); with no voice sounding, the voices' side clears and its noise is
   seeded from the roll's seed too; then vx_configure. */
void vx_song(const song_roll_t *r);
/* One pitched role's instrument (voice V_CHORD, V_BASS, V_LEAD, V_CANDY or V_PAD; others ignored). The pad layer
   (V_PAD) plays M_ENS patches only (INS_STRINGS, INS_WARM_PAD), and only it plays them: another role's notes are
   silent there.
   The pad layer's lanes: its 4 voices are lanes 1-4 (the song engine's: 1-3 the chord, 4 the solo note), and a V_PAD
   event names its lane in song_ev_t.pad (0: no lane, the voice taken from the pool like any role's). A note on a lane
   whose voice is held (or released under 25 ms ago) goes on legato: the same pitch extends it (to the new len from
   now), another glides to it over about 60 ms with no restrike, the envelope going on; on a lane that is silent it
   starts fresh (its swell), and on one still releasing the old note fades out (about 4 ms, as a steal) first. A lane
   never moves another's voice. */
void vx_assign(int voice, int ins);
/* The pad layer's solo lane (lane 4) its own patch: INS_VOX_PAD (the choir), INS_FLUTE, INS_ORGAN or INS_STRINGS,
   anything else (INS_NONE) the pad's (vx_assign's V_PAD). Lane 4 alone plays it, in its own model at that model's
   usual rate (the ensemble at a quarter, the choir at half, the flute and the organ at full), on the pad's bus (its
   filters, its sends), trimmed to the strings' loudness; lanes 1-3 and lane-less notes keep the pad's M_ENS patch. A
   held lane-4 note goes on legato to its next note of the same patch (gliding, as any lane's); a note of another patch
   (a solo voice changed) starts fresh, the old one fading out first. Kept until the next call or vx_init (none);
   vx_assign and vx_configure leave it. */
void vx_assign_pad_solo(int ins);
/* Tests: a role's patch given directly (a changed copy of a table patch; it must outlive its notes), its sends and
   filters as vx_assign sets them; the config is left as it was. */
void vx_assign_patch(int voice, const vx_patch_t *p);
const vx_config_t *vx_config(void);
/* The drum kit (KIT_*); vx_configure sets it from the song's set. */
void vx_set_kit(int kit);
/* The kick's amplitude envelope, 0-1 (see drums.h): the lo-fi chain's ducking source. */
float vx_kick_env(void);
/* A role bus's section filter (the plan's moves, plan_moves): role V_CHORD, V_BASS, V_LEAD, V_CANDY, V_PAD, or any drum
   voice (V_KICK..V_RIM: the drums' bus). vx_set_filter's amount runs 0 (open) to 1 (closed): a one-pole low-pass from
   18 kHz down to 200 Hz. vx_set_hp's runs 0 (off) to 1 (a one-pole high-pass at about 600 Hz). A change lands in
   about 50 ms (a high-pass opening, in about 3 ms); with nothing sounding, at once. */
void vx_set_filter(int role, float amount);
void vx_set_hp(int role, float amount);
/* The song's pump (VX_PUMP_*): the kick ducking at about 1, 3 or 6 dB. vx_configure picks it per song (the seed's
   stream 170, a third each). */
void vx_set_pump(int pump);
/* Tests and the cost bench: switch effects off (VX_OFF_*; 0 for all on). Kept through vx_init. VX_OFF_HALF renders
   the keys and pads, and the pad layer, at the full rate (the reference for the half- and quarter-rate paths). */
enum { VX_OFF_COMP = 1, VX_OFF_FILTERS = 2, VX_OFF_CHORUS = 4, VX_OFF_DELAY = 8, VX_OFF_HALF = 16, VX_OFF_MUD = 32,
       VX_OFF_GLUE = 64 };
void vx_fx_off(unsigned mask);
/* Tests: the drums' compressor's gain reduction now, in dB (its make-up not counted). */
float vx_comp_db(void);
/* Tests and level checks: only one role's bus reaches the mix (V_CHORD, V_BASS, V_LEAD, V_CANDY, V_PAD, or any drum
   voice for the drums' bus; -1, the default, for all). Every voice still plays, so the keys' duck still follows the
   lead. Kept through vx_init. */
void vx_solo(int role);
/* Tests: a sounding chip voice's (M_CHIP) last output sample on role, as the voice returns it to the mix (after its
   envelope and its bit crush); *raw the same sample before the crush, *step the crush's step (the voice's level over
   2^(bits-1)). All 0 for none or no crush. */
float vx_chip_out(int role, float *raw, float *step);
/* Tests: a pad lane's (1-4) voice now: its frequency in Hz (gliding), *step its rate's step (1, 2 or 4 samples) and
   *held its gate; all 0 for a silent lane. */
float vx_pad_lane(int lane, int *step, int *held);
/* Tests: a pad lane's (1-4) voice's patch now, NULL for a silent lane. */
const vx_patch_t *vx_pad_lane_patch(int lane);
/* Tests: the mono synth's legato notes to a new pitch (its glides) on V_LEAD since vx_init (0 for any other role). */
uint32_t vx_glides(int role);
/* Tests: the keys' duck under the lead now, in dB (0 at rest, negative while ducked). */
float vx_duck_db(void);
/* Tests: the mix's stored states (the mud high-pass, the drums' compressor, the buses' filters) that are neither 0
   nor at least 1e-20 in magnitude (denormal-bound); should be 0. */
int vx_tiny(void);
/* Queue an event; its delay counts from now, in samples at 44.1 kHz (the engine assumes that rate). 64 pending at
   most; when full the oldest by insertion order is dropped (not the one due first). When all voices of a pitched lane
   are busy, the victim fades out over about 4 ms first, so a stolen note starts up to ~5 ms late. A mono synth patch
   (M_SUB) on the lead plays a note that arrives while its last is still held, or within 25 ms of its
   end and a 4th or less from it, legato on that voice, gliding to it; after a longer rest or a wider leap it starts
   on its pitch. The chords' pool is
   4 voices, 8 for a pad (its release crosses the next chord's swell), and a pad's swell takes at most half its note;
   the pad layer's (V_PAD) is 4 (a 3-note chord and a solo note), its swell capped alike. */
void vx_event(const song_ev_t *e);
/* Adds the mix into l and r (sample-accurate event starts). The chords', the lead's, the ear candy's and the pad
   layer's sums through the mud high-pass (about 135 Hz, 150 on a pad or the pad layer; VX_OFF_MUD), the keys at the
   song's comping level and ducked about 3 dB while a lead note sounds (in about 25 ms, back over about 400 ms once the
   lead rests); per role bus: the drums through a compressor (a peak level, instant at a 32-sample block, released over
   120 ms; 4:1 above the kit's threshold, at most 6 dB, 0.9 dB of make-up), each bus through its section filters; then
   the chain (fx.h: its glue compressor, VX_OFF_GLUE). */
void vx_render(float *l, float *r, uint32_t n);
/* Every voice into release (about 40 ms), the queue cleared; the bed fades out (FX_RAMP_S), the plate rings on: the
   song's end. */
void vx_release_all(void);
/* vx_release_all, and the bed and the plate's tail out in 40 ms too: a stop, true silence. */
void vx_stop(void);
/* The song's level ramp: g is clamped to [0, 1], reached linearly (exactly) over 20 ms. */
void vx_set_gain(float g);
/* Voices still sounding (queued events not counted). */
int vx_active(void);
/* Anything still to hear: voices, or the chain (the bed open or fading, the plate's tail). The app renders while
   this is nonzero and clears the engine (vx_init) at a song's start only when it is 0. */
int vx_sounding(void);

#endif

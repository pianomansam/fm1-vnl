/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The song engine's voices and mix. Pitched roles play the instrument patches (patch.h); the bells and winds render
   with the placeholder FM until Task 5 gives them code. Drums are in drums.c. Float only. */
#ifndef ENGINE_VOICES_H
#define ENGINE_VOICES_H
#include <stdint.h>
#include "groove.h"
#include "roll.h"
#include "patch.h"

/* The song's sound: one instrument per pitched role (INS_*; INS_NONE silences the role), the kit, the flavor and the
   lo-fi chain's fields (dust 0-3, noise 0-2, ambience 0-1), and the pump (VX_PUMP_*: the kick ducking's depth). */
enum { VX_PUMP_SUBTLE, VX_PUMP_MEDIUM, VX_PUMP_OBVIOUS, VX_PUMP_N };
typedef struct { uint8_t chords, bass, lead, candy, pad, kit, flavor, dust, noise, ambience, pump; } vx_config_t;

/* Everything silent and cleared; the config back to its defaults (Rhodes, sub, vibes, music box, strings; dusty kit, no
   chain: fx_init, the subtle pump), every bus's filters open, the drums' compressor at rest. */
void vx_init(void);
/* At song start, before the first event, and again when a request changes one of these mid-song: the set's
   instruments (the pad layer's: lib_song_pad, the set's or the choir), kit and flavor (the chords' patch sets the
   chorus's depth, each role's patch its delay send; the
   song's comping sets the keys' level), the pad's solo voice (pads_solo_voice: the seed and the set), the
   roll's RF_DUST, RF_NOISE and RF_AMBIENCE into the lo-fi chain (fx.h: the bed opens; dust and bed changes ramp, the
   dust landing at once if no voice sounds), RF_TEMPO into the delay's time, and the pump from the seed. Notes already sounding keep the
   instrument they were struck with through their release. */
void vx_configure(const song_roll_t *r);
/* 1 if roll b differs from a in a field vx_configure reads (a request that landed mid-song needs it again). */
int vx_reconfigures(const song_roll_t *a, const song_roll_t *b);
void vx_bed_off(int on);                 /* 1: no surface noise or rain (host renders, VNL_BED=0) */
/* Tests and the cost bench: the pad layer's instrument (INS_STRINGS, INS_WARM_PAD, INS_CHOIR or INS_ORGAN) in place
   of the song's (lib_song_pad) from the next vx_configure on; INS_NONE (the default, or any other) the song's. Kept
   through vx_init. */
void vx_pad_ins(int ins);
/* Tests and the cost bench: the organ keys' registration (ORG_*) and register (high 1: at its register with the 16'
   out; 0: an octave up) in place of the song's (lib_song_organ, harmony_keys_high), -1 each for the song's; from the
   next vx_assign of INS_ORGAN on the keys. Kept through vx_init. */
void vx_organ_force(int reg, int high);
/* The pad's organ's scanner chorus (on the pad layer or the solo lane): on (1, the default) or still (0), a 60 ms
   crossfade; the app's and the workbench's step say where the full kit plays (plan_full_kit). Kept through
   vx_init. */
void vx_organ_scan(int on);
/* Tests: the choir's formant k (0-2, F1-F3) in Hz as its bank last set it (voices.c: the "oo-aah" morph and the
   drift), 0 before it first sang */
float vx_choir_formant(int k);
/* A new song (the app's song start, before its first event): the lo-fi chain's noise and its LFOs restart from the
   roll's seed wherever nothing is left to hear them, so a seed renders the same bed and dust whatever played before
   (a previous song's ringing tail still sounds); with no voice sounding, the voices' side clears and its noise is
   seeded from the roll's seed too; then vx_configure. */
void vx_song(const song_roll_t *r);
/* One pitched role's instrument (voice V_CHORD, V_BASS, V_LEAD, V_CANDY or V_PAD; others ignored). The pad layer
   (V_PAD) plays M_ENS patches (INS_STRINGS, INS_WARM_PAD, INS_CHOIR), and only it plays them, or the organ (INS_ORGAN:
   its one-stop 4' there; on the keys, the song's registration and register, vx_configure's): another role's notes
   are silent there.
   The pad layer's lanes: its 4 voices are lanes 1-4 (the song engine's: 1-3 the chord, 4 the solo note), and a V_PAD
   event names its lane in song_ev_t.pad (0: no lane, the voice taken from the pool like any role's). A note on a lane
   whose voice is held (or released under 25 ms ago) goes on legato: the same pitch extends it (to the new len from
   now), another glides to it over about 60 ms with no restrike, the envelope going on; on a lane that is silent it
   starts fresh (its swell), and on one still releasing the old note fades out (about 4 ms, as a steal) first. A lane
   never moves another's voice. */
void vx_assign(int voice, int ins);
/* The pad layer's solo lane (lane 4) its own patch: INS_CHOIR (the formant choir, its bank shared with a choir pad's),
   INS_FLUTE, INS_ORGAN (the pad's organ, the one-stop 4') or INS_STRINGS,
   anything else (INS_NONE) the pad's (vx_assign's V_PAD). Lane 4 alone plays it, in its own model (the ensemble, the
   choir and the organ at a quarter of the rate, the flute at half), on the pad's bus (its filters, its sends),
   trimmed to the strings' loudness; lanes 1-3 and lane-less notes keep the pad's M_ENS patch. A
   held lane-4 note goes on legato to its next note of the same patch (gliding, as any lane's); a note of another patch
   (a solo voice changed) starts fresh, the old one fading out first. Kept until the next call or vx_init (none);
   vx_assign leaves it, vx_configure sets the song's (pads_solo_voice). */
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
   voice (V_KICK..V_RIDE: the drums' bus). vx_set_filter's amount runs 0 (open) to 1 (closed): a one-pole low-pass from
   18 kHz down to 200 Hz. vx_set_hp's runs 0 (off) to 1 (a one-pole high-pass at about 600 Hz). A change lands in
   about 50 ms (a high-pass opening, in about 3 ms); with nothing sounding, at once. */
void vx_set_filter(int role, float amount);
/* the whole-mix stutter (voices.c): 1 catches the next len samples of the mix, 2 repeats them, 3 repeats their first
   len, 0 live again (crossfaded); vx_release_all ends it */
void vx_stutter(int mode, uint32_t len);
int vx_stuttering(void);
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
/* The workbench's mute (default off: 0, the mix unchanged): a bit per role bus out of the mix (bit V_CHORD, V_BASS,
   V_LEAD or V_PAD; bit V_KICK for the drums' bus). Listening only: every voice still plays (the keys' duck still
   follows a muted lead). A bus going out ramps to silence over the next chunk; coming back, the notes struck while it
   was muted end first (the drums released), so only notes struck after are heard. Kept through vx_init. */
void vx_mute(unsigned mask);
/* The mixer's level per part (VX_PART_*), in dB: VX_LEVEL_MIN to VX_LEVEL_MAX (clamped), 0 the default (the mix
   unchanged, bit for bit). The drums are the drums' bus (every drum voice, after its compressor), the keys the comp's
   (V_CHORD), the texture the bed (fx_bed_level: the surface noise and the ambience; the dust colors the music, it has
   no level of its own). Mixer state: heard at once (a bus ramps to it over the next chunk, the bed over about 5 ms),
   kept through vx_init and every song (no roll or override touches it), on its bus's wet sends too, and apart from
   vx_mute and vx_solo (a muted part keeps its level). vx_level_get returns the dB set (0 for an unknown part). */
enum { VX_PART_DRUMS, VX_PART_BASS, VX_PART_KEYS, VX_PART_PAD, VX_PART_LEAD, VX_PART_TEXTURE, VX_PART_N };
#define VX_LEVEL_MIN (-12)
#define VX_LEVEL_MAX 6
void vx_level_set(int part, int db);
int vx_level_get(int part);
/* Tests: a sounding chip voice's (M_CHIP) last output sample on role, as the voice returns it to the mix (after its
   envelope and its bit crush); *raw the same sample before the crush, *step the crush's step (the voice's level over
   2^(bits-1)). All 0 for none or no crush. */
float vx_chip_out(int role, float *raw, float *step);
/* Tests: a pad lane's (1-4) voice now: its frequency in Hz (gliding), *step its rate's step (1, 2 or 4 samples) and
   *held its gate; all 0 for a silent lane. */
float vx_pad_lane(int lane, int *step, int *held);
/* Tests: a pad lane's (1-4) voice's patch now, NULL for a silent lane. */
const vx_patch_t *vx_pad_lane_patch(int lane);
/* Tests: the chords' pool's steals of a voice still sounding (held, or in its release; a string re-struck with no
   spare voice free counts) since vx_init (0 for any other role). */
uint32_t vx_steals(int role);
/* The CPU guard's voice rungs (app/drums/cpuguard.h CG_VOICES, CG_THIN). 0 (the default): none, sample for sample as
   before. 1, the release tails go early, each fading out over about 4 ms (as a steal): a released note of the keys,
   the ear candy or the pad layer once under -24 dB of its note, the keys' pool past 4 voices sounding (the quietest
   released first), a released lead note under -12 dB while the lead's next note sounds. Never a held note, the bass
   or the drums. 2, also thinner: the keys hold at most 3 notes (a 4th struck fades the oldest held: a chord's lowest,
   which the bass has), the pad layer's solo lane (lane 4) is silent. Kept through vx_init. vx_shed: the voices cut,
   since boot. */
void vx_guard(int level);
uint32_t vx_shed(void);
/* Tests: the keys' duck under the lead now, in dB (0 at rest, negative while ducked). */
float vx_duck_db(void);
/* Tests: the mix's stored states (the mud high-pass, the drums' compressor, the buses' filters) that are neither 0
   nor at least 1e-20 in magnitude (denormal-bound); should be 0. */
int vx_tiny(void);
/* Queue an event; its delay counts from now, in samples at 44.1 kHz (the engine assumes that rate). 64 pending at
   most; when full the oldest by insertion order is dropped (not the one due first). When all voices of a pitched lane
   are busy, the victim fades out over about 4 ms first, so a stolen note starts up to ~5 ms late. The chords' pool is
   4 voices, 8 for a pad (its release crosses the next chord's swell) or a sampled instrument. On a sampled patch a
   V_CHORD event's pad is the strum's string (strum.h): bit 0 a chuck (the patch's muted note), bits 1-3 the string
   1-6, played on its own voice (the chords' first 6); a re-strike moves the string's last note to a spare voice (the
   last 2) that damps it over about 4 ms, so the new note starts on time and no stroke takes another string's tail.
   A pad's swell takes at most half its note;
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
/* The pad layer's level (song_pad_level, each step): g clamped to [0, 1], the layer's dry scale easing to it over a
   few chunks (about 25 ms), its sends with it. 1 (vx_init) leaves the layer as it was. */
void vx_set_pad_level(float g);
/* Voices still sounding (queued events not counted). */
int vx_active(void);
/* Anything still to hear: voices, or the chain (the bed open or fading, the plate's tail). The app renders while
   this is nonzero and clears the engine (vx_init) at a song's start only when it is 0. */
int vx_sounding(void);
/* The live state's causes (app.c): the events started so far (a count that wraps), and 1 while a section filter or
   the master gain still ramps (a section's moves, a fade) */
uint32_t vx_fires(void);
int vx_ramping(void);

#endif

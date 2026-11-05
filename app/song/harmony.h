/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Harmony over time: the chord for each bar (reharmonized on repeat: a richer color only) and the
   chord part's events per 16th step, in one of four playing styles. Pure functions of their inputs. */
#ifndef SONG_HARMONY_H
#define SONG_HARMONY_H
#include <stdint.h>
#include "roll.h"
#include "plan.h"
#include "groove.h"

/* (CS_HALF, appended: the voicing struck on beats 1 and 3, the second a little softer; sustained keys hold instead) */
/* (CS_STRUM, appended: the guitars' strummed chord, strum.h's engine on a guitar shape) */
/* (CS_HELD2, CS_LIGHT8 and CS_HELD24, appended: held on 2, struck on beat 2 and held to the bar's end; light eighths,
   the voicing struck soft and short on every 8th; held on 2 and 4, held on 2 with a soft re-strike on beat 4 every
   bar. Percussive keys only (harmony_comp_ok), a busy lead leaves them be; the workbench's for now: weight 0 in the
   roll until Sam sets it) */
/* (CS_PAT, appended: a comp rhythm from patterns/comp-rhythms past the fixed six (patterns_gen.h's COMP_PAT): where
   held on 2 plays. The roll draws it only once one is approved; a candidate plays when the comp_pat override forces
   it (the workbench's "Comp rhythm" row) */
enum { CS_HELD, CS_STABS, CS_OFFBEATS, CS_ARP, CS_HALF, CS_STRUM, CS_HELD2, CS_LIGHT8, CS_HELD24, CS_PAT, CS_N };
/* 0 where the chords instrument ins can't play comping style cs (held on 2, held on 2 and 4 and light eighths: the
   percussive keys only, Rhodes, felt piano and the Knight; the guitars only strum); a
   forced style it can't play holds instead (the guitars strum) */
int harmony_comp_ok(int ins, int cs);

/* Pitch classes (root first, up to 5) of the chord at `bar` (from 0). pass is 1-based: 1 = the first time through
   the loop (bar / (n * bars_per_chord) + 1). Passes >= 2 may recolor or substitute exactly one chord of the pass
   (per-song chance 25-50%); dark (intro, outro) changes nothing here: it is a sound, not a chord. Returns the count. */
int harmony_chord_at(const song_roll_t *r, int bar, int pass, int dark, int *pcs);
/* The chord as the keys and the pad sound it, in place: a dominant without its minor 7th (Sam 2026-11-04: the 7th
   sounds cheesy there), its major triad with its colors (C9 sounds as C-E-G-D, an add9);
   any other chord as it is. The lead and the bass read harmony_chord_at's, the 7th included. Returns the count. */
int harmony_sound_pcs(int *pcs, int n);

/* The comping's per-strike hash (velocities, roll gaps): strum.c's too. */
uint32_t harmony_mix(uint32_t a, uint32_t b, uint32_t c, uint32_t d);

/* CS_*, per song (seed stream 121), weighted by the set's chords instrument; the guitars (the nylon, the Emilyguitar)
   always strum, under any lead (Sam, 2026-11-04) */
int harmony_style(const song_roll_t *r);
int harmony_style_ins(int ins, uint32_t seed);   /* the same for a chords instrument */
unsigned harmony_stab_steps(const song_roll_t *r);   /* the steps stab comping strikes: 1 and a step the kick leaves free */
/* the COMP_PAT index a CS_PAT song plays: the comp_pat override's, else one of the approved drawn from the seed
   (stream 202; any entry when none is approved); -1 where the table is empty (the firmware, until one is approved) */
int harmony_comp_pat(const song_roll_t *r);

int harmony_half_strikes(const song_roll_t *r);   /* 1 when the song really plays half notes (not held by sustained keys or the kick) */
/* the steps stabs, half notes, held on 2 (and 4), light eighths or a comp rhythm strike (0 if only a note on 1) */
unsigned harmony_comp_steps(const song_roll_t *r);

/* Chord-part events of one 16th step (bar and step from 0). Writes nothing unless the block has L_KEYS.
   voicing_state: the previous chord's 4 notes (all 0 = none), updated here for voice leading. Chords are rolled:
   notes 10-60 ms apart, bottom up (not for CS_ARP). Returns the number of events written (at most max). */
/* The chord part's voicing state: the last voicing sounded, and the loop's voicings, worked out once per song (and
   again when the harmony changes) as a cycle that voice-leads smoothly all the way around, wrap included. Every pass
   plays them (Sam: one voicing per chord through the song); a chord recolored on a pass is voiced on its own.
   Zero it at a song's start. */
/* A strummed song (CS_STRUM) keeps its guitar shapes here instead: one per chord through the song (gtab, keyed by the
   chord's pitch classes in gsig; the 16 found last: a loop's chords and their recolorings), each voice-led from the
   shape found before it. */
enum { HV_SHAPES = 16 };
typedef struct {
    int last[4]; int loop[4][4]; int nloop; uint32_t sig;
    uint8_t gtab[HV_SHAPES][6]; uint32_t gsig[HV_SHAPES]; int gn;
} harmony_voicing_t;
/* the loop lock above, on by default; off, each chord is voiced from the last sounded (VNL_LOOPLOCK=0 in renders) */
void harmony_set_loop_lock(int on);
/* Where sustained keys (a pad) sit against the lead, one per song (seed stream 160, equal weights): below (as
   percussive keys always do), with (the keys an octave up, in the lead's range) or above (the keys an octave up,
   the lead kept to C4-C5). The lead design's register rule. */
enum { PL_BELOW, PL_WITH, PL_ABOVE };
int harmony_pad_place(const song_roll_t *r);
/* The keys' window (lead-styles design): percussive keys (not a strum's guitar shape; sustained keys keep their
   place) voice with their top note at most hi and their bottom at least lo (0: no bound; hi 0: no window), by the same
   voice-leading rules, one voicing per chord. Where no voicing fits (the top under C4, the bottom out of the
   instrument's range or under lo, a chord with no voicing there), the window is ignored and the lead's band moves up
   instead (leads_range_bar). Global: song_tick sets its song's (from the lead's band, worked out at song_start and
   again where the set or the workbench's lead register or style moves it) and clears it before returning. */
void harmony_keys_window(int lo, int hi);
/* semitones the workbench's keys_oct moves the keys (override.h; 0 when unset or out of the instrument's range) */
int harmony_keys_oct(const song_roll_t *r);
/* The voicing this bar's chord sounds with (what harmony_step strikes), without striking it: vs->last is unchanged
   (its loop cache may fill). Lets the lead plan a bar over the keys before they strike. A strummed song: the top 4
   notes of its guitar shape, ascending (0 first where the shape has fewer; its shape cache may fill). */
void harmony_voicing_at(const song_roll_t *r, int bar, int pass, harmony_voicing_t *vs, int out4[4]);
/* Every note the keys sound in the bar, as harmony_voicing_at finds them (0 where fewer): a strummed song's whole
   guitar shape, its low strings too (what the pad's notes must not rub); else harmony_voicing_at's 4, then 0, 0. */
void harmony_keys_notes_at(const song_roll_t *r, int bar, int pass, harmony_voicing_t *vs, int out6[6]);
/* The keys sit high: the mean note of the voicings the first 16 bars sound (harmony_voicing_at, the first pass) is
   HV_HIGH (F4) or over. The means fall in two clusters, about 58-63 (the keys under the lead) and 69-75 (sustained
   keys raised an octave, with or over the lead: harmony_pad_place), with the gap at 64-65 (set 18, 600 seeds: 1 song
   at 64, 4 at 65). The organ keys go up an octave unless they sit high (vx_configure). */
enum { HV_HIGH = 65 };
int harmony_keys_high(const song_roll_t *r);
/* One step of the keys. busy: the lead has 4+ onsets this bar, so stabs strike on 1 only, offbeats on 6 and 14, the
   arpeggio on even steps, half notes on step 0 of every bar, held a bar (held comping, held on 2 (and 4), light
   eighths and sustained keys unchanged); the voicing never changes. */
int harmony_step(const song_roll_t *r, const plan_block_t *b, int bar, int step, int pass, float step_samples,
                 harmony_voicing_t *vs, int busy, uint32_t salt, song_ev_t *out, int max);

#endif

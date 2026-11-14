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
/* (CS_HELD2 and CS_HELD24, appended: held on 2, struck on beat 2 and held to the bar's end; held on 2 and 4, held on
   2 with a soft re-strike on beat 4 every bar. Percussive keys and the vibes only (harmony_comp_ok), a busy lead
   leaves them be; approved, Sam 2026-11-05. CS_LIGHT8_GONE: light eighths, removed (Sam, 2026-11-05: "remove light
   eighths completely"); its id is never reused, and a forced 7 (an old snapshot's) holds) */
/* (CS_PAT, appended: the code path of the comp rhythms added past the fixed five (ids 7 on), each its own row of
   patterns_gen.h's COMP; a candidate plays when the comp_pat override forces it (the workbench's Comping row).
   Every keys comping rhythm, fixed or added, is a row of COMP (Sam, 2026-11-05: "I think of all comping rhythms the
   same"): its keys, weight, busyness, rules and velocities are data (patterns/model.json); the CS_* ids stay as the
   code paths and the override's values) */
enum { CS_HELD, CS_STABS, CS_OFFBEATS, CS_ARP, CS_HALF, CS_STRUM, CS_HELD2, CS_LIGHT8_GONE, CS_HELD24, CS_PAT, CS_N };
/* 0 where the chords instrument ins can't play comping style cs: a rhythm's row's keys (COMP: held and half notes all
   keys, stabs the Knight, held on 2 (and 4) and the added rhythms the percussive keys and the vibes); the guitars only
   strum; the harp (on the keys) strum, held and half notes only; CS_LIGHT8_GONE none; strum, offbeats
   and the arpeggio (forced) any keys but the vibes. A forced style the keys can't play holds instead (the guitars
   strum) */
int harmony_comp_ok(int ins, int cs);

/* Pitch classes (root first, up to 5) of the chord at `bar` (from 0). pass is 1-based: 1 = the first time through
   the loop (bar / (n * bars_per_chord) + 1). Passes >= 2 may recolor exactly one chord of the pass, never substitute
   it (per-song chance 25-50%); dark (intro, outro) changes nothing here: it is a sound, not a chord. Returns the
   count. */
int harmony_chord_at(const song_roll_t *r, int bar, int pass, int dark, int *pcs);
/* The chord as the keys and the pad sound it, in place: a dominant without its minor 7th (Sam 2026-11-04: the 7th
   sounds cheesy there), its major triad with its colors (C9 sounds as C-E-G-D, an add9);
   any other chord as it is. The lead and the bass read harmony_chord_at's, the 7th included. Returns the count. */
int harmony_sound_pcs(int *pcs, int n);

/* The comping's per-strike hash (velocities, roll gaps): strum.c's too. */
uint32_t harmony_mix(uint32_t a, uint32_t b, uint32_t c, uint32_t d);

/* The song's COMP row (patterns_gen.h): the workbench's forced one, else drawn per song (seed stream 121) from the
   approved rows the set's chords instrument plays, at their weights tilted by the rolled motion (roll_motion: a live
   Motion request never re-draws it); -1 where no row plays (the guitars strum; a forced strum, offbeats or
   arpeggio) */
int harmony_comp_row(const song_roll_t *r);
/* CS_*, per song: the row's style; the guitars (the nylon, the Emilyguitar) always strum, under any lead (Sam,
   2026-11-04); the harp on the keys (sets 49-51) strums in half its songs (harmony.c's harp_strums), else rolls
   held or half notes */
int harmony_style(const song_roll_t *r);
int harmony_style_ins(int ins, uint32_t seed, int motion);   /* the draw for a chords instrument at a motion */
unsigned harmony_stab_steps(const song_roll_t *r);   /* the steps stab comping strikes: 1 and a step the kick leaves free */
/* the COMP_PAT index (an added rhythm's) the song plays, else -1 */
int harmony_comp_pat(const song_roll_t *r);

int harmony_half_strikes(const song_roll_t *r);   /* 1 when the song really plays half notes (not held by sustained keys or the kick) */
/* the steps stabs, half notes, held on 2 (and 4) or a comp rhythm strike (0 if only a note on 1) */
unsigned harmony_comp_steps(const song_roll_t *r);

/* Chord-part events of one 16th step (bar and step from 0). Writes nothing unless the block has L_KEYS.
   voicing_state: the previous chord's 4 notes (all 0 = none), updated here for voice leading. The keys strike a
   chord together: 0-2.7 ms a note, bottom up, about 0-8 ms in all; only the harp rolls it, 20-40 ms a note (not for
   CS_ARP: one note a step). Returns the number of events written (at most max). */
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
    int16_t ant;   /* the harp's anticipation: 1 + the bar whose downbeat's early notes were played (0 none) */
} harmony_voicing_t;
/* The harp's anticipation across a bar line (harmony_step): on by default; the app turns it off while a section jump
   waits (the song jumps at a bar line: its next bar may never come) */
void harmony_antic(int on);
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
/* The lift over a low lead (library.h lib_lead_low): 12 in a bar of the plan's lead blocks where the keys go an
   octave up over it (the keys and the pad's chord part: the lead in, the workbench's keys register as rolled, the
   keys' range holding it), else 0. It reads the plan harmony_keys_plan names: song_tick's (set while it ticks, as
   the keys' window is; NULL: none, no lift). The keys' is 0 in a song with them with the lead (harmony_keys_with:
   their own register all song); the pad's chord part's lifts either way. */
int harmony_keys_lift(const song_roll_t *r, int bar);
/* The keys over a low lead: 1 with the lead, 0 lifted over it. harmony_keys_draw: the song's draw (3 in 4 with the
   lead, its own seed stream); harmony_keys_with: the workbench's keys_with where set, else the draw. */
int harmony_keys_draw(const song_roll_t *r);
int harmony_keys_with(const song_roll_t *r);
int harmony_pad_lift(const song_roll_t *r, int bar);
void harmony_keys_plan(const song_plan_t *p);
/* The voicing this bar's chord sounds with (what harmony_step strikes), without striking it: vs->last is unchanged
   (its loop cache may fill). Lets the lead plan a bar over the keys before they strike. A strummed song: the top 4
   notes of its guitar shape, ascending (0 first where the shape has fewer; its shape cache may fill). */
void harmony_voicing_at(const song_roll_t *r, int bar, int pass, harmony_voicing_t *vs, int out4[4]);
/* The same, and in raw4 (NULL: none) the voicing before it is placed (what a strike leaves in vs->last; a strummed
   song leaves raw4 as it was): the vibes' notes lifted to F3 can't be placed back by an octave shift */
void harmony_voicing_raw_at(const song_roll_t *r, int bar, int pass, harmony_voicing_t *vs, int out4[4], int raw4[4]);
/* Every note the keys sound in the bar, as harmony_voicing_at finds them (0 where fewer): a strummed song's whole
   guitar shape, its low strings too (what the pad's notes must not rub); else harmony_voicing_at's 4, then 0, 0. */
void harmony_keys_notes_at(const song_roll_t *r, int bar, int pass, harmony_voicing_t *vs, int out6[6]);
/* The keys sit high: the mean note of the voicings the first 16 bars sound (harmony_voicing_at, the first pass) is
   HV_HIGH (F4) or over. The means fall in two clusters, about 58-63 (the keys under the lead) and 69-75 (sustained
   keys raised an octave, with or over the lead: harmony_pad_place), with the gap at 64-65 (set 18, 600 seeds: 1 song
   at 64, 4 at 65). The organ keys go up an octave unless they sit high (vx_configure). */
enum { HV_HIGH = 65 };
int harmony_keys_high(const song_roll_t *r);
/* One step of the keys. lead: HS_* flags of the lead's bar. HS_BUSY: it has 4+ onsets, so stabs strike on 1 only,
   offbeats on 6 and 14, the arpeggio on even steps, half notes on step 0 of every bar, held a bar (held comping, held
   on 2 (and 4), light eighths and sustained keys unchanged); the voicing never changes. HS_LEAD: it sounds (an onset
   or a note tied in), where keys with a low lead (harmony_keys_with) thin and soften. */
enum { HS_BUSY = 1, HS_LEAD = 2 };
int harmony_step(const song_roll_t *r, const plan_block_t *b, int bar, int step, int pass, float step_samples,
                 harmony_voicing_t *vs, int lead, uint32_t salt, song_ev_t *out, int max);

#endif

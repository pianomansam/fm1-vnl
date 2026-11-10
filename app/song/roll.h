/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The roll: every per-song choice, drawn from one seed. Each field draws from its own stream, so changing one
   choice never shifts another. Novelty against the previous song, DJ-style key flow, length rules, dice and picks. */
#ifndef SONG_ROLL_H
#define SONG_ROLL_H
#include <stdint.h>
#include "library.h"

enum { RF_KEY, RF_TEMPO, RF_MOTION, RF_MOOD, RF_MODE, RF_MAP, RF_PROG, RF_COLOR, RF_SET, RF_PATTERN, RF_SWING,
       RF_DUST, RF_NOISE, RF_AMBIENCE, RF_BARS, RF_N };

/* v[RF_KEY] 0-11; RF_TEMPO BPM 70-90; RF_MOTION 0-100; RF_MOOD 0-3 (LIB_MOODS index); RF_MODE MODE_*;
   RF_MAP/PROG/SET/PATTERN library ids; RF_COLOR COLOR_*; RF_SWING per-mille 520-620; RF_DUST 0-3;
   RF_NOISE 0 none / 1 vinyl / 2 tape; RF_AMBIENCE 0 none / 1 rain; RF_BARS 36-76, a multiple of 4, with
   RF_BARS * 240 / RF_TEMPO within 120-210 s. A progression is legal only if it suits both the mood and the mode.
   Progressions and drum patterns are drawn (roll_song, roll_dice, the novelty passes) from the approved entries only
   (patterns_gen.h's *_STATUS 1), the drum patterns evenly, tilted by the rolled motion (LIB_PATTERNS_BUSY,
   motion_tilt); roll_set takes a candidate too (the workbench's way to hear one), and a mode only a candidate
   progression suits (that progression then, unless the song's suits it). */
/* motion_moved: how far a live Motion request (song.c's, from a 4-bar boundary) moved v[RF_MOTION] from the motion the
   song was rolled (or staged) with; 0 in any roll that was never live-changed. roll_motion reads the rolled one. */
typedef struct { uint32_t seed; int16_t v[RF_N]; int16_t motion_moved; } song_roll_t;

void roll_song(const song_roll_t *prev, uint32_t seed, song_roll_t *out);   /* prev NULL for the first song */
int roll_dice(song_roll_t *r, int field, uint32_t salt);  /* 1: changed to another legal value; 0: none exists */
int roll_set(song_roll_t *r, int field, int value);       /* manual pick, validated and clamped; the value applied
                                                             (the current one if refused); -1 if field is out of range */
/* murmur3's finalizer over (a, b): a seed hashed apart for a draw of its own (xorshift is linear, so streams that
   differ by a constant correlate) */
uint32_t roll_mix(uint32_t a, uint32_t b);
int roll_seconds(const song_roll_t *r);                   /* RF_BARS * 240 / RF_TEMPO */
/* The motion the song was rolled with (v[RF_MOTION] less a live request's move): what a per-song draw that must not
   change mid-song (the keys' comping rhythm, the drum pattern, the bass style, the strum pattern) reads. roll_song
   draws the motion first, before the drum pattern that leans on it. */
int roll_motion(const song_roll_t *r);
/* The motion tilt, shared by every per-song draw that leans on the song's motion ("motion roll -> informed busy-ness
   roll", Sam 2026-11-05): an entry of base weight `weight` and busyness `busy` (0 sparse, 1 middle, 2 busy) drawn at
   `motion` (20-80; clamped) weighs weight * (60 + (motion - 50) * (busy - 1)): at 50 every busyness alike, at 20 sparse
   3x busy (90 to 30, middle 60), at 80 busy 3x sparse; never 0 for a weight over 0. Integer only; the result fits a
   uint32_t for any uint8_t weight (draw with rng_next() % total, not rng_weighted's uint8_t weights). */
uint32_t motion_tilt(int weight, int busy, int motion);
/* The song's drum kit (seed stream 192, hashed apart from the set's and the mood's draws): the set's synth kit or its
   sampled kit (skit), about half each; a set with no skit (the 808 and Brushes sets) its synth kit always, a set
   with no synth kit its skit. A pure function of the seed and the set. */
int lib_song_kit(const song_roll_t *r);
/* The song's pad layer instrument (seed stream 197, hashed apart from every other draw): the formant choir
   (INS_CHOIR) in LIB_CHOIR_PCT% of songs, the organ (INS_ORGAN, its one-stop 4') in LIB_ORGAN_PCT% (never over organ
   keys: the set's pad there), else the set's pad (the strings or the warm pad); the workbench's custom set
   (LIB_SET_WB) its pad always (the pad row's pick). A pure function of the seed and the set. */
enum { LIB_CHOIR_PCT = 25, LIB_ORGAN_PCT = 15 };
int lib_song_pad(const song_roll_t *r);
/* The organ keys' drawbar registration (ORG_A, ORG_B or ORG_C, patch.h's; seed stream 198, hashed apart from every
   other draw), 0-2 at equal odds: Sam liked all three. A pure function of the seed. */
int lib_song_organ(const song_roll_t *r);
/* The song has a pad layer (pads.h): about 2 songs in 3 (seed stream 195, hashed apart from every other draw, so no
   other choice moves with it; Sam, 2026-10-29: "make it possible for the pad part to not be in a song"). The
   workbench's pad_on override (override.h) forces it. A pure function of the seed. */
int song_pad_on(const song_roll_t *r);

/* The drums' two-bar conversations (after Cut-A-Dor's rhythm families, cad.c, GPL-3.0-only; Sam, 2026-11-10: vary,
   not replace). A boom-bap song (pattern family PF_BOOMBAP) plays, in place of its pattern's own:
   - a kick phrase (patterns_gen.h's KICK_PHRASE: Sam's k2-k6 and four of Cut-A-Dor's, k7-k10) in LIB_KICK_PCT% of
     songs, an approved one at even odds (seed stream 230), its kick unghosted; the bass's roots follow it;
   - a hat figure in LIB_HATS_PCT% of songs (stream 231): one of Cut-A-Dor's eight hat bars over its 8ths (a hat
     dropped under the backbeat, a 16th pickup, an 8th moved a 16th later) in bar A and another in bar B.
   Drawn apart: about one boom-bap song in thirteen has both, and over half keep the pattern as written. The kick
   phrase is drawn from the song's seed, as the pattern is (the bass follows it); the hat figure from the drums' (the
   song's, or the drums' re-roll: override.h's drums_salt). The kick override forces the kick (the pattern's own or
   any phrase, on any pattern). The workbench's custom pattern (LIB_PATTERN_WB) plays as edited: neither applies.
   Pure functions. */
enum { LIB_KICK_PCT = 30, LIB_HATS_PCT = 25 };
int lib_song_kick(const song_roll_t *r);          /* a KICK_PHRASE index, -1 the pattern's own */
int lib_song_hat_figure(const song_roll_t *r);    /* 0-7, -1 the pattern's own hats */
/* the song's lane hits and ghosts in `bar` over pattern p (lib_lane_hits and lib_lane_ghosts with the above): what
   the groove plays and every part that reads the kick (the bass's roots, the comp's and the strum's fit) */
unsigned lib_song_hits(const song_roll_t *r, const lib_pattern_t *p, int lane, int bar);
unsigned lib_song_ghosts(const song_roll_t *r, const lib_pattern_t *p, int lane, int bar);

/* Test only: the progression table the roll draws from (default LIB_PROGS); NULL restores it. */
void roll_use_progs(const lib_prog_t *progs, int n);

#endif

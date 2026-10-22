/* SPDX-License-Identifier: MPL-2.0 */
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
   RF_BARS * 240 / RF_TEMPO within 120-210 s. A progression is legal only if it suits both the mood and the mode. */
typedef struct { uint32_t seed; int16_t v[RF_N]; } song_roll_t;

void roll_song(const song_roll_t *prev, uint32_t seed, song_roll_t *out);   /* prev NULL for the first song */
int roll_dice(song_roll_t *r, int field, uint32_t salt);  /* 1: changed to another legal value; 0: none exists */
int roll_set(song_roll_t *r, int field, int value);       /* manual pick, validated and clamped; the value applied
                                                             (the current one if refused); -1 if field is out of range */
int roll_seconds(const song_roll_t *r);                   /* RF_BARS * 240 / RF_TEMPO */

/* Test only: the progression table the roll draws from (default LIB_PROGS); NULL restores it. */
void roll_use_progs(const lib_prog_t *progs, int n);

#endif

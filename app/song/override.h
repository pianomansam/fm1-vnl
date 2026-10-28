/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Per-song overrides (the workbench's): force a choice the roll makes, or re-roll one part. Default off: with none
   set, every choice is the roll's, exactly as before. Read where each choice is made (harmony, lead, melody, groove). */
#ifndef SONG_OVERRIDE_H
#define SONG_OVERRIDE_H
#include <stdint.h>

typedef struct {
    int8_t comp;        /* -1 rolled, else CS_* */
    int8_t lead_style;  /* -1 rolled, 0 phrase planner, 1 hybrid */
    int8_t pad_place;   /* -1 rolled, else PL_* (sustained keys only: percussive keys are always below) */
    int8_t ride;        /* -1 rolled, else RIDE_NONE / RIDE_ALL / RIDE_SECTION (sampled kits only) */
    int8_t keys_oct;    /* -1, 0, +1: the keys' register, clamped to the instrument's range */
    int8_t lead_oct;    /* -1, 0, +1: the lead's register, clamped likewise */
    uint32_t voicing_salt;  /* 0: as rolled; else a different, still rule-abiding voicing path */
    uint32_t phrase_salt;   /* 0: as rolled; else a new motif, rhythm and contour for the lead and melody */
} song_ov_t;

void song_override(const song_ov_t *ov);   /* NULL: none (default); the struct is copied */
const song_ov_t *song_ov(void);            /* never NULL: a static all-rolled struct when none */

/* The lead's and melody's seed: seed itself unless phrase_salt is set */
uint32_t song_ov_phrase_seed(uint32_t seed);
/* An instrument's playable range (MIDI), for the register overrides */
void song_ov_range(int ins, int *lo, int *hi);

#endif

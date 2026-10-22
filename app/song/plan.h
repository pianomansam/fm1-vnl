/* SPDX-License-Identifier: MPL-2.0 */
/* The plan: a rolled song and its map's skeleton turned into blocks of bars, each with the layers that play, a
   section name and per-block flags. Blocks start on 4-bar boundaries and cover exactly RF_BARS. Motion (0-100)
   sets how many optional moves happen on top of the skeleton (hats out, 1-bar drops, lead toggles, ear candy). */
#ifndef SONG_PLAN_H
#define SONG_PLAN_H
#include <stdint.h>
#include "roll.h"

enum { SEC_INTRO, SEC_GROOVE, SEC_LEAD, SEC_BREAK, SEC_OUTRO };

enum {
    PB_FILL_LAST = 1,   /* drum fill in the block's last bar */
    PB_DROP_LAST = 2,   /* 1-bar drum drop on the block's last bar */
    PB_DARK = 4,        /* darker chord voicing (intro, keys outro) */
    PB_FADE = 8,        /* fade over the block */
    PB_SWELL = 16       /* a swell before the next block (sound in Plan 3) */
};

typedef struct { uint8_t start, bars; uint8_t section; uint8_t layers; uint8_t flags; } plan_block_t; /* layers: L_* */
typedef struct { uint8_t n; uint8_t bars; plan_block_t b[24]; } song_plan_t;

void plan_build(const song_roll_t *r, song_plan_t *p);           /* deterministic per roll */
const plan_block_t *plan_at(const song_plan_t *p, int bar);      /* NULL outside [0, bars) */
const char *plan_section_name(int section);                      /* "intro", "groove", "lead", "break", "outro" */

#endif

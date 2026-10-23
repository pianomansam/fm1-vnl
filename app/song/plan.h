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

/* The section moves, per role bus (PM_*: the same numbers as V_CHORD..V_CANDY, and V_KICK for the drums; plan.c
   checks): at bar and step (0-15), each bus's low-pass amount lp[] (0 open .. 1 closed) and high-pass amount hp[] (0 off .. 1),
   for vx_set_filter and vx_set_hp. Intro and outro (PB_DARK): drums and bass 0.7, the rest 0.4. A break: 0.8,
   opening to 0 across its last bar. A drum-drop bar: 0.6. A swell: 0.5 to 0 over the block's last bar (a dark or
   dropped block's own amounts to 0: the swell opens it). The bar
   before a groove block that follows a break or a drum drop: the high-pass rises 0 to 1 on every bus but the lead,
   open again on the downbeat. Elsewhere (and outside the plan) 0. */
enum { PM_CHORDS, PM_BASS, PM_LEAD, PM_CANDY, PM_DRUMS, PM_N };
void plan_moves(const song_plan_t *p, int bar, int step, float *lp, float *hp);
void plan_build(const song_roll_t *r, song_plan_t *p);           /* deterministic per roll */
const plan_block_t *plan_at(const song_plan_t *p, int bar);      /* NULL outside [0, bars) */
const char *plan_section_name(int section);                      /* "intro", "groove", "lead", "break", "outro" */

#endif

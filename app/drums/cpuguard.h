/* SPDX-License-Identifier: GPL-3.0-only */
/* The CPU guard (cpuguard.c, adapted from Optimist's firmware/src/system/cpuguard.c, after Flowstate's guard by
   Zakaria Chowdhury; both GPL-3.0-only): when the render's load nears its budget, quality steps down a level at a time,
   in the order a lo-fi mix hides best, and back up once the load falls, with hysteresis. Off (cg_on 0, the default:
   the workbench, the host simulator) it never acts and nothing changes. On: the FM-1 (its render's time against its
   block's) and the host cost bench (VNL_GUARD: the render's time against AMB-1's). */
#ifndef CPUGUARD_H
#define CPUGUARD_H
#include <stdint.h>

/* the levels, in the order they step down (each keeps the ones under it) */
enum {
    CG_OFF,
    CG_TAILS,   /* the drum lanes' sounds end sooner: faded out after CG_TAIL_S (a hit's body plays; its ring goes) */
    CG_BED,     /* the bed (surface noise, rain) fades out and stops */
    CG_PLATE,   /* the plate takes no new input: its tail rings out, then it stops (the chorus and delay stay) */
    CG_VOICES,  /* the release tails go early (voices.h vx_guard 1): a released keys, pad or lead note fades out once
                   quiet, the keys' pool keeps 4 sounding; held notes, the bass and the drums untouched */
    CG_THIN,    /* thinner (vx_guard 2): the keys hold 3 notes (a chord's lowest goes: the bass has it), no pad solo */
    CG_LEVELS
};
#define CG_TAIL_S 0.25f

/* Recovery: a level comes off after CG_CALM_S of calm (the load under the release level, no late blocks), each
   further level after CG_RELEASE's 2 s more, so the quality comes back a step at a time and a busy song doesn't
   flap. */
#define CG_CALM_S 20
/* on: the guard acts; ceil, rel: the load (1/256 of the budget) over which it steps down, under which (counting what
   the level saves) it steps back up; full: a late block (the FM-1: its whole block, a drop-out), which after one over
   the ceiling steps at once. Resets its state. */
void cg_on(int on, uint32_t ceil, uint32_t rel, uint32_t full);
/* n > 0: a step on the load also needs n late blocks (full) among the blocks over the ceiling before it: the FM-1's,
   where a peak that makes no block late is no reason to cut anything (Sam, 2026-11-07: he heard the steps). 0, cg_on's
   default: the load alone (the bench, whose budget is AMB-1's, not a block's time) */
void cg_need_late(uint32_t n);
/* before a block's render: voices, what sounds now (the prediction: the last block plus what it adds) */
void cg_pre(uint32_t voices);
/* after it: its load, 1/256 of the budget (256: the whole budget; more: late) */
void cg_post(uint32_t load);
int cg_level(void);
/* what it did: steps down (early: on a prediction), levels come back off (recov), late blocks it saw, blocks spent
   at each level, the highest level */
typedef struct { uint32_t steps, early, recov, late, blocks[CG_LEVELS]; uint8_t top; } cg_stats_t;
const cg_stats_t *cg_stats(void);
void cg_stats_reset(void);              /* its steps and times back to zero (the live state's reset) */
/* the host cost bench's: the level held at `level` (its rungs on, the stepping off), -1 to let go (off) */
void cg_pin(int level);

#endif

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
    CG_LEVELS
};
#define CG_TAIL_S 0.25f

/* on: the guard acts; ceil, rel: the load (1/256 of the budget) over which it steps down, under which (counting what
   the level saves) it steps back up; full: a late block (the FM-1: its whole block, a drop-out), which after one over
   the ceiling steps at once. Resets its state. */
void cg_on(int on, uint32_t ceil, uint32_t rel, uint32_t full);
/* before a block's render: voices, what sounds now (the prediction: the last block plus what it adds) */
void cg_pre(uint32_t voices);
/* after it: its load, 1/256 of the budget (256: the whole budget; more: late) */
void cg_post(uint32_t load);
int cg_level(void);
/* what it did: steps down (early: on a prediction), blocks spent at each level, the highest level */
typedef struct { uint32_t steps, early, blocks[CG_LEVELS]; uint8_t top; } cg_stats_t;
const cg_stats_t *cg_stats(void);

#endif

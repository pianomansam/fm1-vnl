/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The render's time by stage, for the device's CPU meter (the live state's "prof": tools/fm1_state.py prints it).
   The idea is DUB-1's EN_PROF (GPL-3.0-only: no code taken): each stage of a block timed on a free-running clock,
   summed, with the heaviest block's own breakdown kept. Off (nothing read, nothing kept) until pf_init gives it a
   clock: the workbench and the unit tests never do. Never changes a sample.
   The voices render together, a sample at a time (voices.c render_seg), so their loop is timed whole and split
   between the parts by a probe: one sample a segment, at a random place, timed part by part (pf_v_*). */
#ifndef ENGINE_PROF_H
#define ENGINE_PROF_H
#include <stdint.h>

/* the stages, in the live state's order; PF_CONTROL is the render's rest (the song's steps, notes starting,
   commands, the output), the block's time less the others */
enum { PF_DRUMS, PF_BASS, PF_COMP, PF_LEAD, PF_PAD, PF_TEXTURE, PF_BUS, PF_FX, PF_CONTROL, PF_N };
#define PF_NAMES {"drums", "bass", "comp", "lead", "pad", "texture", "bus", "fx", "control"}

typedef uint32_t (*pf_clock_t)(void);
extern pf_clock_t pf_clock;              /* (0: off) */
#define PF_T() (pf_clock ? pf_clock() : 0u)

/* clock: a free-running count, ticks_per_us a microsecond (plat_ticks24, PLAT_TICKS_PER_US); 0 turns it off */
void pf_init(pf_clock_t clock, uint32_t ticks_per_us);
/* a block's render: begin, then end (its stages summed, the heaviest block's kept) */
void pf_block_begin(void);
void pf_block_end(void);
/* the engine's stages: ticks added to this block's (PF_DRUMS .. PF_FX); the voices' loop, whole */
void pf_add(int stage, uint32_t ticks);
void pf_loop(uint32_t ticks);
/* the voices' probe (voices.c): the segment's sample to time (pos .. pos + n - 1; none when off), then at that sample
   the voices' loop begins, each voice's index before it renders (its part from the index), and the loop ends */
uint32_t pf_v_pick(uint32_t pos, uint32_t n);
void pf_v_begin(void);
void pf_v_mark(int part);
void pf_v_end(uint32_t n);

/* the readout, since pf_reset: blocks rendered, each stage's microseconds in all and in the heaviest block, that
   block's whole time; the render's microseconds in all since pf_init (never reset: a main-loop span less the render
   inside it is the main loop's own time) */
typedef struct {
    uint32_t blocks, sum_us[PF_N], worst_us[PF_N], worst_total_us;
} pf_stats_t;
void pf_read(pf_stats_t *s);
void pf_reset(void);
uint32_t pf_render_us(void);
#endif

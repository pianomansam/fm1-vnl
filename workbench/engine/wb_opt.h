/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Optimist's synth drum voices (optimist/drum_synth.c) for the workbench's lanes (wb_drums.c): a fixed pool, each
   voice one sound at a time, rendered in Optimist's blocks of 32 and handed out any number of samples at a time. */
#ifndef WB_OPT_H
#define WB_OPT_H
#include <stdint.h>

#define WB_OPT_NV 8
int wb_opt_has(int kit, int lane);              /* kit WB_OPT_*, lane 0..15 (DS_LANES) */
void wb_opt_on(int v, int kit, int lane, int vel);  /* voice v starts kit's lane at vel 1-127 (the last sound cut) */
int wb_opt_render(int v, float *x, uint32_t n); /* n samples of voice v into x (written; peak about 1); 0 once ended */
void wb_opt_off(int v);
void wb_opt_seed(uint32_t seed);                /* the voices' noise (drum_synth.c draws a seed per hit) */

#endif

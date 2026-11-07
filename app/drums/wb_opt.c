/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Optimist's synth drums for the workbench (wb_opt.h): drum_synth.c as Optimist ships it, over its dependencies
   (optimist/ds_deps.h) and the LO-FI kit's table. A voice renders a block of CTL samples at a time (its envelopes step
   per block, ds_control) into a buffer the lane reads from; a hit starts the next block at its sample. */
#include "wb_opt.h"
#include "optimist/ds_deps.h"
#include "optimist/drum_synth.c"

typedef struct {
    dsv_t s;
    int32_t blk[CTL];
    uint32_t at, have;                  /* blk's next sample, and how many it holds */
    uint8_t on, alive;                  /* sounding; ds_render's word on the last block */
} ov_t;
static ov_t ov[WB_OPT_NV];
/* each lane's GM note at its own pitch (DS_MAP: semi 0) */
static const uint8_t LANE_NOTE[DS_LANES] = {36, 38, 39, 42, 46, 43, 48, 49, 51, 70, 63, 37, 56, 75, 35, 40};

int wb_opt_has(int kit, int lane) { return kit >= 0 && kit < (int)DS_NKITS && lane >= 0 && lane < DS_LANES; }

void wb_opt_on(int v, int kit, int lane, int vel)
{
    ov_t *o;
    if (v < 0 || v >= WB_OPT_NV || !wb_opt_has(kit, lane))
        return;
    o = &ov[v];
    ds_on(&o->s, &DS_KITS[kit], LANE_NOTE[lane], (uint32_t)(vel < 1 ? 1 : vel > 127 ? 127 : vel));
    o->at = o->have = 0;
    o->on = o->alive = 1;
}

int wb_opt_render(int v, float *x, uint32_t n)
{
    ov_t *o = &ov[v];
    uint32_t i = 0;
    while (i < n && o->on) {
        if (o->at == o->have) {
            if (!o->alive) {
                o->on = 0;
                break;
            }
            o->alive = (uint8_t)ds_render(&o->s, o->blk, CTL);
            o->at = 0;
            o->have = CTL;
        }
        for (; i < n && o->at < o->have; i++)
            x[i] = (float)o->blk[o->at++] * (1.0f / 32768.0f);
    }
    for (; i < n; i++)
        x[i] = 0.0f;
    return o->on;
}

void wb_opt_off(int v) { ov[v].on = 0; }

void wb_opt_seed(uint32_t seed)
{
    rng_state = (seed * 2654435761u) ^ 0x1234567u;
    if (!rng_state)
        rng_state = 0x1234567u;
}

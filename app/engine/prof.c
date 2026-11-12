/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The render's time by stage (prof.h). The render (an interrupt on the FM-1) writes; the main loop reads through a
   sequence count (odd while a block's figures are being added) and asks for a reset, which the render does. Integer
   ticks, a float only for the voices' split (no 64-bit multiply). */
#include "prof.h"

pf_clock_t pf_clock;
static uint32_t tpu = 1u;                /* ticks a microsecond */
static uint32_t t_blk;                   /* this block's start */
static uint32_t blk[PF_N];               /* this block's ticks by stage, timed (the voices' parts: none) */
static uint32_t loop_t;                  /* this block's voices' loop, whole */
static float est[PF_N];                  /* the probes' estimates of the loop's ticks by part (a probe's time x its
                                            segment's samples) */
static uint32_t rnd = 0x9E3779B9u;       /* the probe's place (its own: the music's random numbers untouched) */
static uint32_t v_t;                     /* the probe: the last mark's time, its part (-1 none yet) */
static int v_part = -1;
static float v_got[PF_N];                /* the probe's ticks by part, this sample */
static float v_ovh;                      /* a mark's own ticks (its call and the clock's read), taken off each part's */
/* since the reset: the blocks and the heaviest one (st), each stage's timed us, the loop's, the probes' estimates (us;
   split at the read: over a window, not a block, so a probe's noise averages out) */
static pf_stats_t st;
static uint32_t sum_us[PF_N], loop_us;
static float est_us[PF_N];
static uint32_t carry[PF_N + 2];         /* ticks short of a microsecond, carried to the next block (by stage, the
                                            loop, the block's whole) */
static volatile uint32_t seq;            /* odd while those change */
static volatile uint8_t reset_ask;
static volatile uint32_t render_us;

/* ticks to whole microseconds, the rest carried (c) */
static uint32_t to_us(uint32_t t, uint32_t *c)
{
    uint32_t u;
    *c += t;
    u = *c / tpu;
    *c -= u * tpu;
    return u;
}

/* the loop's us split between the voices' parts by the estimates e (us); what they did not see (the mud high-pass,
   the buses' writes) the bus's; estimates over the loop's time scaled down to it. Into us[] */
static void split(uint32_t loop, const float *e, uint32_t *us)
{
    uint32_t k, used = 0;
    float sum = 0.0f, sc;
    for (k = 0; k < PF_N; k++)
        sum += e[k];
    sc = sum > (float)loop ? (float)loop / sum : 1.0f;
    for (k = 0; k < PF_N; k++)
        if (e[k] > 0.0f) {
            uint32_t v = (uint32_t)(e[k] * sc);
            us[k] += v;
            used += v;
        }
    us[PF_BUS] += loop > used ? loop - used : 0u;
}

void pf_init(pf_clock_t clock, uint32_t ticks_per_us)
{
    int r, k;
    pf_clock = clock;
    tpu = ticks_per_us ? ticks_per_us : 1u;
    v_ovh = 0.0f;
    if (!clock)
        return;
    for (r = 0; r < 4; r++) {            /* a mark's cost: marks with nothing between them, the least of 4 runs */
        float o;
        pf_v_begin();
        for (k = 0; k < 64; k++)
            pf_v_mark(k & 1);
        pf_v_mark(-1);
        o = (v_got[0] + v_got[1]) / 64.0f;
        if (r == 0 || o < v_ovh)
            v_ovh = o;
    }
}

void pf_block_begin(void)
{
    uint32_t k;
    if (!pf_clock)
        return;
    for (k = 0; k < PF_N; k++) {
        blk[k] = 0;
        est[k] = 0.0f;
    }
    loop_t = 0;
    t_blk = pf_clock();
}

void pf_add(int stage, uint32_t ticks) { blk[stage] += ticks; }

void pf_loop(uint32_t ticks) { loop_t += ticks; }

uint32_t pf_v_pick(uint32_t pos, uint32_t n)
{
    if (!pf_clock || !n)
        return 0xFFFFFFFFu;
    rnd ^= rnd << 13;
    rnd ^= rnd >> 17;
    rnd ^= rnd << 5;
    return pos + rnd % n;
}

void pf_v_begin(void)
{
    uint32_t k;
    for (k = 0; k < PF_N; k++)
        v_got[k] = 0.0f;
    v_part = -1;
    v_t = pf_clock();
}

void pf_v_mark(int part)
{
    if (part == v_part)
        return;
    {
        uint32_t t = pf_clock();
        if (v_part >= 0)
            v_got[v_part] += (float)(t - v_t) - v_ovh;
        v_part = part;
        v_t = t;
    }
}

void pf_v_end(uint32_t n)
{
    uint32_t k;
    pf_v_mark(-1);
    for (k = 0; k < PF_N; k++)
        est[k] += v_got[k] * (float)n;
}

void pf_block_end(void)
{
    uint32_t k, total, sum = 0, us[PF_N], lu;
    float e[PF_N];
    if (!pf_clock)
        return;
    total = pf_clock() - t_blk;
    for (k = 0; k < PF_N; k++)
        sum += blk[k];
    blk[PF_CONTROL] = total > sum + loop_t ? total - sum - loop_t : 0u;   /* (the render's rest) */
    for (k = 0; k < PF_N; k++)
        us[k] = to_us(blk[k], &carry[k]);
    lu = to_us(loop_t, &carry[PF_N]);
    total = to_us(total, &carry[PF_N + 1]);
    for (k = 0; k < PF_N; k++)
        e[k] = est[k] / (float)tpu;
    render_us += total;
    seq++;
    if (reset_ask) {
        reset_ask = 0;
        st = (pf_stats_t){0};
        loop_us = 0;
        for (k = 0; k < PF_N; k++) {
            sum_us[k] = 0;
            est_us[k] = 0.0f;
        }
    }
    st.blocks++;
    for (k = 0; k < PF_N; k++) {
        sum_us[k] += us[k];
        est_us[k] += e[k];
    }
    loop_us += lu;
    if (total >= st.worst_total_us) {    /* the heaviest block: its own split */
        st.worst_total_us = total;
        split(lu, e, us);
        for (k = 0; k < PF_N; k++)
            st.worst_us[k] = us[k];
    }
    seq++;
}

void pf_read(pf_stats_t *s)
{
    uint32_t a, k, loop;
    float e[PF_N];
    do {
        a = seq;
        *s = st;
        loop = loop_us;
        for (k = 0; k < PF_N; k++) {
            s->sum_us[k] = sum_us[k];
            e[k] = est_us[k];
        }
    } while ((a & 1u) || a != seq);
    split(loop, e, s->sum_us);
}

void pf_reset(void)
{
    reset_ask = 1;
}

uint32_t pf_render_us(void) { return render_us; }

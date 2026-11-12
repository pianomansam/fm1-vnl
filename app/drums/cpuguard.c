/* SPDX-License-Identifier: GPL-3.0-only */
/* The CPU guard (cpuguard.h). Adapted from Optimist's firmware/src/system/cpuguard.c (w0ts/optimist 8a91efd,
   GPL-3.0-only: optimist/LICENSE; "after Flowstate's guard (firmware/src/guard.c, docs/guardrails.md 2.5) by Zakaria
   Chowdhury, GPL-3.0-only"). Kept: its control (a load that is the larger of the measured one and a prediction from
   what started; CG_HOLD blocks over the ceiling, or a late block after one over it, step a level up; a level comes
   off after CG_RELEASE blocks under the release level counting the load the level saves, measured over the CG_HOLD
   blocks after its step, so it does not come and go; the reactive path's leaky count of late blocks among lighter
   ones). Altered for VNL-1: our ladder (cpuguard.h: the drum lanes' tails, the bed, the plate) in place of Optimist's
   (engine quality, unison, voices, the reverb); its cost model (a table of its engines' costs) replaced by a cost a
   voice learned from the measured load; its thresholds parameters (cg_on), so the host bench can run it against its
   own budget; two voice rungs on top, their shedding ours (voices.h vx_guard); recovery after CG_CALM_S of calm,
   then a level each CG_RELEASE (PERF-1's pace). This is not the original software. */
#include "cpuguard.h"
#include "fx.h"
#include "wb_drums.h"
#include "voices.h"

#define CG_HOLD 8u          /* blocks over the ceiling before a step (46 ms of 256-frame blocks) */
#define CG_RELEASE 345u     /* blocks under the release level before a further level comes off (2 s) */
#define CG_CALM (CG_CALM_S * 44100u / 256u)   /* ... and before the first (20 s: 3445 blocks) */
/* blocks after a step before its saving shows (the lanes' sounds fade CG_TAIL_S into a hit; the bed's ramp; the
   plate's tail rings out, about 1.8 s): no further step on the load (a late block after one over the ceiling still
   steps), and what the level saves is measured after them */
static const uint16_t CG_SETTLE[CG_LEVELS] = {0u, 69u, 9u, 345u, 9u, 172u};   /* (the tails: a fade a chunk; the
                                                                                 thinning: the next chord's) */
#define CG_GAIN_MAX 40u     /* what a level is taken to save at most (16 % of the budget): measured after a step taken
                               in a burst (a song's start, a spike), the load before it reads high and would hold the
                               level for good (the FM-1, 2026-11-07: "bed" held for 25 s at 24-47 %) */
#define CG_SPK_HIT 64u      /* the reactive path: a late block adds this to a count that loses 1 a block ... */
#define CG_SPK_ON 160u      /* ... and reacts at this (3 late blocks within ~30) */
#define CG_RFLOOR 128u      /* ... only at a mean load of half the budget or more (else the lateness isn't the music's) */

static struct {
    uint8_t on, level, over, gpend, gn;
    uint16_t settle;                   /* blocks left before the last step's saving shows */
    uint8_t need, lates;               /* cg_need_late's; the late blocks in the run over the ceiling so far */
    uint8_t back;                      /* recovering: a level came off and no step since (the next comes sooner) */
    uint8_t gain[CG_LEVELS];           /* the load each level saved (1/256), measured after its step */
    uint16_t under, spk, stuck;        /* (stuck: blocks under the release level that the level's saving holds up) */
    uint32_t ceil, rel, full;
    uint32_t meas, est, avg, vcost, nv; /* the last block's load, its prediction, the mean of ~8, a voice's cost
                                           (1/16 of 1/256), the voices at the last block's start */
    uint32_t gpre, gsum;
    cg_stats_t st;
} cg;

static void cg_apply(void)
{
    wb_drums_guard(cg.level >= CG_TAILS);
    fx_guard(cg.level >= CG_PLATE, cg.level >= CG_BED);
    vx_guard(cg.level >= CG_THIN ? 2 : cg.level >= CG_VOICES);
}

void cg_pin(int level)
{
    cg_on(0, 0u, 0u, 0u);
    cg.level = (uint8_t)(level < 0 ? 0 : level < CG_LEVELS ? level : CG_LEVELS - 1);
    cg_apply();
}

void cg_need_late(uint32_t n) { cg.need = (uint8_t)(n < 255u ? n : 255u); }

void cg_on(int on, uint32_t ceil, uint32_t rel, uint32_t full)
{
    uint8_t was = cg.on;
    cg = (__typeof__(cg)){0};
    cg.on = (uint8_t)(on != 0);
    cg.ceil = ceil;
    cg.rel = rel;
    cg.full = full;
    if (was || on)
        cg_apply();
}

static void cg_step(void)
{
    if (cg.level >= CG_LEVELS - 1)
        return;
    cg.level++;
    cg.over = 0;
    cg.under = 0;
    cg.back = 0;
    cg.st.steps++;
    cg.st.top = cg.level > cg.st.top ? cg.level : cg.st.top;
    cg.gpend = cg.level;
    cg.settle = CG_SETTLE[cg.level];
    cg.gpre = cg.avg;                   /* (the load before: a spike's own block would make the level stay) */
    cg.gn = 0;
    cg.gsum = 0;
    cg_apply();
}

void cg_pre(uint32_t voices)
{
    int32_t pred;
    if (!cg.on)
        return;
    pred = (int32_t)cg.meas + ((int32_t)voices - (int32_t)cg.nv) * (int32_t)cg.vcost / 16;   /* (what started since) */
    pred = pred < 0 ? 0 : pred;
    cg.nv = voices;
    if ((uint32_t)pred >= cg.full && cg.meas > cg.ceil && cg.meas < cg.full && !cg.gpend &&
        cg.level < CG_LEVELS - 1 && cg.lates >= cg.need) {
        cg.st.early++;                  /* predicted late after a block over the ceiling: a step before it renders */
        cg_step();
    }
    cg.est = (uint32_t)pred;
}

void cg_post(uint32_t m)
{
    uint32_t load, prev = cg.meas, c, late;
    if (!cg.on || m > 16u * 256u)       /* (16 blocks' time for one: not a reading; the guard never acts on it) */
        return;
    cg.st.blocks[cg.level]++;
    load = m > cg.est ? m : cg.est;
    c = m < cg.full ? m : cg.full;
    late = m >= cg.full;
    cg.st.late += late;
    cg.meas = m;
    cg.vcost = (cg.vcost * 15u + c * 16u / (cg.nv + 1u)) / 16u;
    if (cg.settle) {
        cg.settle--;
    } else if (cg.gpend) {              /* what the last step saved: the load before it less the blocks' after it */
        cg.gsum += c;
        if (++cg.gn >= CG_HOLD) {
            uint32_t after = cg.gsum / CG_HOLD;
            uint32_t g = cg.gpre > after ? cg.gpre - after : 0u;
            cg.gain[cg.gpend] = (uint8_t)(g < CG_GAIN_MAX ? g : CG_GAIN_MAX);
            cg.gpend = 0;
        }
    }
    if (load > cg.ceil) {
        if (m > cg.ceil && prev > cg.ceil)
            cg.under = cg.back = 0;      /* (one block over among lighter ones, its prediction the next: the release
                                            keeps counting; else the calm starts again) */
        cg.lates = (uint8_t)(cg.lates + (late && cg.lates < 255u));
        if (!cg.gpend && cg.lates >= cg.need && ((late && prev > cg.ceil) || ++cg.over >= CG_HOLD))
            cg_step();                   /* (one step a burst: the next only once its saving shows, so a transient
                                            takes the drum tails, not the bed and the plate with them) */
    } else {
        uint32_t back = load + cg.gain[cg.level];   /* (the load with the level's saving back) */
        cg.over = 0;
        cg.lates = 0;
        if (cg.level && !(late && cg.avg >= CG_RFLOOR) && back < cg.rel) {
            cg.stuck = 0;
            if (++cg.under >= (cg.back ? CG_RELEASE : CG_CALM)) {
                cg.level--;
                cg.under = 0;
                cg.back = 1;
                cg.st.recov++;
                cg_apply();
            }
        } else {
            cg.under = 0;
            if (cg.level && load < cg.rel && ++cg.stuck >= CG_RELEASE) {
                cg.stuck = 0;            /* under the release level, held only by what the level is said to save:
                                            it saves less than that, by half each time */
                cg.gain[cg.level] = (uint8_t)(cg.gain[cg.level] / 2u);
            }
        }
    }
    cg.avg = (cg.avg * 7u + c + 4u) >> 3;
    if (late)                           /* the reactive path: late blocks among lighter ones */
        cg.spk = (uint16_t)(cg.spk + CG_SPK_HIT < 0xFFFFu ? cg.spk + CG_SPK_HIT : 0xFFFFu);
    else if (cg.spk)
        cg.spk--;
    if (cg.spk >= CG_SPK_ON && cg.avg >= CG_RFLOOR && !cg.gpend) {   /* (late blocks themselves: no need's count) */
        cg.spk = 0;
        cg_step();
    }
}

int cg_level(void) { return cg.level; }
const cg_stats_t *cg_stats(void) { return &cg.st; }
void cg_stats_reset(void)
{
    cg.st = (cg_stats_t){0};
    cg.st.top = cg.level;
}

/* the stepping logic's check (tests/host/song_test.c test_cpuguard runs it): a load held over the ceiling steps
   level by level, a lone late block doesn't, and under the release level the levels come off one at a time */
#define CK(c) (k++, bad = !bad && !(c) ? k : bad)
int cg_selftest(void)
{
    int i, n, k = 0, bad = 0;
    cg_on(1, 218u, 204u, 256u);
    for (i = 0; i < 7; i++)
        cg_pre(4), cg_post(230u);
    CK(cg_level() == CG_OFF);                  /* (7 blocks over: not yet) */
    cg_pre(4), cg_post(230u);
    CK(cg_level() == CG_TAILS);                /* the 8th: tails */
    for (n = 0; n < 2000 && cg_level() < CG_PLATE; n++)
        cg_pre(4), cg_post(230u);
    CK(n == 69 + 8 + 7 + 9 + 8 + 7);           /* each next step after its settle, its measure (whose last block counts) and 8 over */
    for (n = 0; n < 2000 && cg_level() < CG_THIN; n++)
        cg_pre(4), cg_post(230u);
    CK(n == 345 + 8 + 7 + 9 + 8 + 7);          /* the voice rungs: after the plate's tail, then the tails' fade */
    for (i = 0; i < 400; i++)
        cg_pre(4), cg_post(230u);
    CK(cg_level() == CG_THIN);                 /* the top holds */
    for (i = 0; i < (int)CG_CALM; i++)         /* (the first block under: its prediction, the last load, still over) */
        cg_pre(4), cg_post(100u);
    CK(cg_level() == CG_THIN);
    cg_pre(4), cg_post(100u);
    CK(cg_level() == CG_VOICES);               /* 20 s of calm: one level off */
    for (i = 0; i < (int)CG_RELEASE - 1; i++)
        cg_pre(4), cg_post(100u);
    CK(cg_level() == CG_VOICES);
    cg_pre(4), cg_post(100u);
    CK(cg_level() == CG_PLATE);                /* then a level each 2 s */
    cg_pre(4), cg_post(230u);
    cg_pre(4), cg_post(230u);                  /* two blocks over the ceiling (no step: 2 of 8): the calm again */
    for (i = 0; i < (int)CG_CALM; i++)
        cg_pre(4), cg_post(100u);
    CK(cg_level() == CG_PLATE);
    cg_pre(4), cg_post(100u);
    CK(cg_level() == CG_BED);
    CK(cg_stats()->steps == 5u && cg_stats()->recov == 3u && cg_stats()->late == 0u);
    cg_on(1, 218u, 204u, 256u);
    cg_pre(4), cg_post(150u);
    cg_pre(4), cg_post(300u);                    /* one late block after a light one: nothing */
    CK(cg_level() == CG_OFF);
    cg_pre(4), cg_post(230u);
    cg_pre(4), cg_post(300u);                    /* a late block after one over the ceiling: a step */
    CK(cg_level() == CG_TAILS);
    /* the device's stuck case (2026-11-07): a step in a burst of late blocks after a light load, then the load light
       again (24-47 %) with a block over the ceiling now and then: every level comes off, after the calm */
    cg_on(1, 218u, 204u, 256u);
    for (i = 0; i < 30; i++)
        cg_pre(4), cg_post(100u);
    for (i = 0; i < 20; i++)
        cg_pre(40), cg_post(400u);
    CK(cg_level() >= CG_TAILS);
    CK(cg_stats()->late == 20u);
    for (n = 0; n < 20000 && cg_level() != CG_OFF; n++)
        cg_pre(4), cg_post(n % 100 == 50 ? 230u : 90u + (uint32_t)(n * 7 % 31));
    CK(n < 345 + 8 + (int)CG_CALM + CG_LEVELS * (345 + 2));   /* a settle, its measure, the calm, a level each 2 s */
    /* the FM-1's (cg_need_late 2): a peak over the ceiling with no late block steps nothing, nor one late block in it;
       a second late one in the same run does */
    cg_on(1, 240u, 204u, 256u);
    cg_need_late(2);
    for (i = 0; i < 40; i++)
        cg_pre(4), cg_post(i == 20 ? 260u : 245u);
    CK(cg_level() == CG_OFF);
    cg_pre(4), cg_post(260u);
    CK(cg_level() == CG_TAILS);
    cg_need_late(0);
    cg_on(0, 0u, 0u, 0u);
    for (i = 0; i < 40; i++)
        cg_pre(4), cg_post(1000u);
    CK(cg_level() == CG_OFF);                  /* off: never */
    return bad;   /* 0, or the first check that failed */
}

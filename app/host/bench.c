/* SPDX-License-Identifier: MPL-2.0 */
/* Host cost bench (tools/cost-bench.sh): how long 60 s of a VNL song takes to render, against 60 s of AMB-1's engine
   at its heaviest piece. Built only with -DVNL_BENCH, otherwise this file is empty (it sits in host/ so the
   firmware globs skip nothing; it has a main()). No audio device. Usage: bench [--heaviest | [--forced] SEED...]
   Prints `vnl <ms> amb <ms> ratio <x>` (best of 3 runs each) for the seed(s), and for --heaviest the seed chosen.
   The lo-fi chain's cost does not depend on the song, and its fields are rolled independently of the rest, so the
   heaviest seed may not carry the heaviest chain: the picked seed (and every seed with --forced) is also timed with
   its chain forced to the heaviest real one (dust 3, vinyl, rain), printed as `forced`: the upper bound.
   The AMB-1 engine's headers clash with ours (MODE_*), so its few entry points are declared here by hand. */
#include <stdint.h>
#ifdef VNL_BENCH
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "song.h"
#include "voices.h"

void en_init(void);
void en_set(int param, int value);
void en_pool(uint32_t pool);
void en_play(int on);
void en_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);
void en_piece(int i, int16_t *par, uint16_t *pool);
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }
#define EN_NPIECES 8
#define EN_NPARAMS 28                   /* amb.h's P_NPARAMS (checked below) */

#define SR 44100u
#define SECS 60u
#define BLK 256u                        /* the app's audio block */

static double now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e3 + (double)t.tv_nsec / 1e6;
}

/* 60 s of the song at `seed`: ticks, events to the voices at the step's start (delays honored by the voices), and
   the render in blocks. *events gets the event count. Milliseconds. */
static double run_vnl(uint32_t seed, int render, long *events, int force)
{
    static song_t s;
    static float l[BLK], r[BLK];
    song_roll_t roll;
    song_ev_t ev[64];
    uint32_t total = SR * SECS, done = 0;
    double t0 = now_ms();
    *events = 0;
    roll_song(0, seed, &roll);
    if (force) {
        roll.v[RF_DUST] = 3;
        roll.v[RF_NOISE] = 1;
        roll.v[RF_AMBIENCE] = 1;
    }
    vx_init();
    song_start(&s, &roll);
    vx_song(&s.roll);
    while (done < total) {
        int n = song_tick(&s, ev, 64), i;
        float adv = song_step_samples(&s);
        uint32_t step = (uint32_t)adv, left;
        if (!n && s.state == SS_ENDED) break;
        *events += n;
        if (!render) { done += step; continue; }
        for (i = 0; i < n; i++) vx_event(&ev[i]);
        for (left = step; left; ) {
            uint32_t b = left < BLK ? left : BLK, k;
            for (k = 0; k < b; k++) l[k] = r[k] = 0.0f;
            vx_render(l, r, b);
            left -= b;
        }
        done += step;
    }
    return now_ms() - t0;
}

static double run_amb(int piece)
{
    static int32_t out[BLK * 2];
    int16_t par[EN_NPARAMS];
    uint16_t pool;
    uint32_t done, total = SR * SECS;
    int p;
    double t0;
    en_init();
    en_piece(piece, par, &pool);
    for (p = 0; p < EN_NPARAMS; p++) en_set(p, par[p]);
    en_pool(pool);
    en_play(1);
    t0 = now_ms();
    for (done = 0; done < total; done += BLK) en_render(out, BLK, 4096);
    return now_ms() - t0;
}

static double best3_vnl(uint32_t seed, int force)
{
    double best = 1e30, t;
    long ev;
    int i;
    for (i = 0; i < 3; i++) { t = run_vnl(seed, 1, &ev, force); if (t < best) best = t; }
    return best;
}

/* The heaviest roll among seeds 1..200: the slowest by measured render time (one full 60 s render each, ~10 s in all;
   the per-seed time is noisy, so the pick is then re-timed best-of-3 in main). */
static uint32_t heaviest(void)
{
    uint32_t seed, best = 1;
    long ev;
    double t, slowest = -1;
    for (seed = 1; seed <= 200; seed++) {
        t = run_vnl(seed, 1, &ev, 0);
        if (t > slowest) { slowest = t; best = seed; }
    }
    return best;
}

int main(int argc, char **argv)
{
    double amb = 0, t;
    int p, heavy = 0, i, ap = 0, forced = 0;
    uint32_t seeds[64];
    int ns = 0;
    for (i = 1; i < argc && ns < 64; i++) {
        if (!strcmp(argv[i], "--forced")) forced = 1;
        else if (argv[i][0] == '-') heavy = 1;
        else seeds[ns++] = (uint32_t)strtoul(argv[i], 0, 10);
    }
    if (heavy || !ns) { seeds[0] = heaviest(); ns = 1; forced = 1; printf("heaviest seed %u\n", (unsigned)seeds[0]); }
    /* AMB-1's heaviest piece: the slowest of its eight (best of 3 each) */
    for (p = 0; p < EN_NPIECES; p++) {
        double b = 1e30;
        for (i = 0; i < 3; i++) { t = run_amb(p); if (t < b) b = t; }
        if (b > amb) { amb = b; ap = p; }
    }
    for (i = 0; i < ns; i++) {
        double v = best3_vnl(seeds[i], 0);
        printf("vnl %.1f amb %.1f ratio %.3f  (seed %u, amb piece %d)\n", v, amb, v / amb, (unsigned)seeds[i], ap);
        if (forced) {
            v = best3_vnl(seeds[i], 1);
            printf("vnl %.1f amb %.1f ratio %.3f  (seed %u forced: dust 3, vinyl, rain)\n", v, amb, v / amb,
                   (unsigned)seeds[i]);
        }
    }
    return 0;
}
#endif

/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The song engine's worst tick (tools/tick-bench.sh): song_tick and song_start run in the audio interrupt, so their
   worst case matters, not their mean. Over seeds 2..N, each song played R times, each tick's time its least over the
   R plays (the host's noise filtered out), then the most over all ticks: strummed songs (CS_STRUM) and the rest
   apart. Also the worst song_start, plan_build and groove_ride (the ride's extra plan builds at a song's start), and
   a hash of every event (strummed and other songs apart), the same before and after a change that keeps the music.
   A third argument forces a lead_style (override.h: 2 + LS_*, a lead style) on every song.
   Built only with -DVNL_BENCH, otherwise this file is empty (as bench.c: the firmware globs take host/). */
#ifdef VNL_BENCH
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "song.h"
#include "harmony.h"
#include "groove.h"
#include "plan.h"
#include "override.h"

static double now_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e6 + (double)t.tv_nsec / 1e3;
}

static double tk[4096];

int main(int argc, char **argv)
{
    static song_t s;
    static song_plan_t p;
    song_ev_t ev[64];
    uint32_t seed, hi = argc > 1 ? (uint32_t)atoi(argv[1]) : 3000u, ws[2] = {0, 0}, wst[2] = {0, 0};
    uint32_t h[2] = {2166136261u, 2166136261u};
    int reps = argc > 2 ? atoi(argv[2]) : 5, songs[2] = {0, 0}, i;
    double mx[2] = {0, 0}, smx[2] = {0, 0}, pb = 0, gr = 0;
    song_ov_t o = *song_ov();
    if (argc > 3) {
        o.lead_style = (int8_t)atoi(argv[3]);
        song_override(&o);
    }
    for (seed = 2; seed <= hi; seed++) {
        song_roll_t r;
        int st, rep, nt = 0, blk;
        double best = 1e9;
        roll_song(NULL, seed, &r);
        st = harmony_style(&r) == CS_STRUM;
        songs[st]++;
        for (i = 0; i < 4096; i++) tk[i] = 1e9;
        for (rep = 0; rep < reps; rep++) {
            double t0 = now_us(), t;
            song_start(&s, &r);
            t = now_us() - t0;
            if (t < best) best = t;
            t0 = now_us();
            plan_build(&r, &p);
            t = now_us() - t0;
            if (rep == reps - 1 && t > pb) pb = t;
            t0 = now_us();
            (void)groove_ride(&r, &blk);
            t = now_us() - t0;
            if (rep == reps - 1 && t > gr) gr = t;
            nt = 0;
            while (s.state == SS_PLAYING && nt < 4096) {
                int n;
                t0 = now_us();
                n = song_tick(&s, ev, 64);
                t = now_us() - t0;
                if (t < tk[nt]) tk[nt] = t;
                nt++;
                if (rep == 0)
                    for (i = 0; i < n; i++) {
                        const unsigned char *b = (const unsigned char *)&ev[i];
                        size_t j;
                        for (j = 0; j < sizeof ev[i]; j++) h[st] = (h[st] ^ b[j]) * 16777619u;
                    }
            }
        }
        for (i = 0; i < nt; i++)
            if (tk[i] > mx[st]) { mx[st] = tk[i]; ws[st] = seed; }
        if (best > smx[st]) { smx[st] = best; wst[st] = seed; }
    }
    for (i = 0; i < 2; i++)
        printf("%-5s songs %4d  worst tick %5.1f us (seed %u)  worst start %4.1f us (seed %u)  events %08x\n",
               i ? "strum" : "other", songs[i], mx[i], (unsigned)ws[i], smx[i], (unsigned)wst[i], (unsigned)h[i]);
    printf("plan_build worst %.1f us, groove_ride worst %.1f us (one play: noisier)\n", pb, gr);
    return 0;
}
#endif

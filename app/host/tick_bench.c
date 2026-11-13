/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The song engine's worst tick (tools/tick-bench.sh): song_tick and song_start run in the audio interrupt, so their
   worst case matters, not their mean. Over seeds 2..N, each song played R times, each tick's time its least over the
   R plays (the host's noise filtered out), then the most over all ticks: strummed songs (CS_STRUM) and the rest
   apart. Also the worst song_start (its block's: the start, or a part of the first bar's planning, each a block of
   its own), plan_build and groove_ride (the ride's extra plan builds at a song's start), and a hash of every event
   (strummed and other songs apart), the same before and after a change that keeps the music.
   Also the lead loop's filter (song_lead_lp_at, once per step after the tick, as app.c's step): its worst and mean
   per step, timed alike (each step's 200 calls in a batch: the host clock's 1 us).
   A third argument forces a lead_style (override.h: 2 + LS_*, a lead style) on every song, a fourth a comping style
   (CS_*; -1 for none), held where the keys can't play it. VNL_LEAD_LOOP=1 turns the lead loop on (lead_loop 1) for
   every song. VNL_KEYS=<INS_*> puts those keys in every song's set (a custom set: the harp on the keys, 32, a
   candidate no set has).
   VNL_REQ=1 lands a row's request on every 4-bar line from bar 4 (an instrument for each part, the kit, the comp's
   rhythm, the lead's style, a mood or key; a kind a line, the next the next line), asked on the group's second bar
   (VNL_REQ_AT=k: k 16ths before the line instead, a late one); the landings' downbeats timed apart, by kind.
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
#include "leads/leads.h"

static double now_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e6 + (double)t.tv_nsec / 1e3;
}

static double tk[4096], lk[4096];
static uint8_t tkl[4096];                /* the tick was a landing's downbeat: 1 + its kind */

/* the requests VNL_REQ lands, a kind a line (row, values cycled by line) */
enum { RQ_KINDS = 9 };
static const char *const rq_name[RQ_KINDS] = {"comp ins", "bass ins", "lead ins", "pad ins", "kit", "comp rhythm",
                                              "lead style", "mood", "key"};
static void rq_ask(song_t *s, int kind, int line)
{
    static const int keys[] = {INS_RHODES, INS_FELT_PIANO, INS_ORGAN, INS_SYNTH_PLUCK, INS_KNIGHT, INS_NYLON};
    static const int bass[] = {INS_UPRIGHT, INS_ELEC_BASS, INS_SUB, INS_FM_BASS};
    static const int lead[] = {INS_FLUTE, INS_VIBES, INS_SAW_LEAD, INS_JAZZ_GTR, INS_WHISTLE, INS_SAMP_CELLO};
    static const int pad[] = {INS_STRINGS, INS_WARM_PAD, INS_CHOIR};
    static const int kit[] = {KIT_DUSTY, KIT_SOFT, KIT_808, SONG_KIT_PRESET, SONG_KIT_PRESET + 3, SONG_KIT_PRESET + 7};
    switch (kind) {
    case 0: song_request_row(s, SR_ROLE + V_CHORD, keys[line % 6]); break;
    case 1: song_request_row(s, SR_ROLE + V_BASS, bass[line % 4]); break;
    case 2: song_request_row(s, SR_ROLE + V_LEAD, lead[line % 6]); break;
    case 3: song_request_row(s, SR_ROLE + V_PAD, pad[line % 3]); break;
    case 4: song_request_row(s, SR_KIT, kit[line % 6]); break;
    case 5: song_request_row(s, SR_OV + OV_COMP, line % 5); break;
    case 6: song_request_row(s, SR_OV + OV_LEAD_STYLE, 2 + line % LS_N); break;
    case 7: song_request_row(s, SR_ROLL + RF_MOOD, 1 << (line % 4)); break;
    default: song_request_row(s, SR_ROLL + RF_KEY, (s->roll.v[RF_KEY] + 5) % 12); break;
    }
}
static uint8_t tk0[4096];                /* the tick was a bar's first 16th (the downbeat's generation) */

int main(int argc, char **argv)
{
    static song_t s;
    static song_plan_t p;
    song_ev_t ev[64];
    uint32_t seed, hi = argc > 1 ? (uint32_t)atoi(argv[1]) : 3000u, ws[2] = {0, 0}, wst[2] = {0, 0};
    uint32_t h[2] = {2166136261u, 2166136261u};
    int reps = argc > 2 ? atoi(argv[2]) : 5, songs[2] = {0, 0}, i;
    double mx[2] = {0, 0}, smx[2] = {0, 0}, pb = 0, gr = 0, lmx = 0, lsum = 0, dmx = 0, omx = 0, dsum = 0, osum = 0;
    long dn = 0, on = 0;
    uint32_t dws = 0;
    long lsteps = 0, ln[RQ_KINDS] = {0};
    double lmax[RQ_KINDS] = {0}, lsumk[RQ_KINDS] = {0}, amax[RQ_KINDS] = {0};
    uint32_t lseed[RQ_KINDS] = {0};
    int req = getenv("VNL_REQ") && *getenv("VNL_REQ") == '1';
    int req_at = getenv("VNL_REQ_AT") ? atoi(getenv("VNL_REQ_AT")) : 0;
    int keys = getenv("VNL_KEYS") ? atoi(getenv("VNL_KEYS")) : -1;
    uint32_t lws = 0;
    song_ov_t o = *song_ov();
    if (argc > 3) {
        o.lead_style = (int8_t)atoi(argv[3]);
        if (argc > 4) o.comp = (int8_t)atoi(argv[4]);
    }
    if (getenv("VNL_LEAD_LOOP") && *getenv("VNL_LEAD_LOOP") == '1') o.lead_loop = 1;
    song_override(&o);
    song_first_bar_later(1);
    for (seed = 2; seed <= hi; seed++) {
        song_roll_t r;
        int st, rep, nt = 0, blk;
        double best = 1e9;
        roll_song(NULL, seed, &r);
        if (keys >= 0) {                 /* VNL_KEYS: the song's set with those keys (the workbench's custom set) */
            static lib_set_t ks;
            ks = *lib_set_by_id((uint8_t)r.v[RF_SET]);
            ks.chords = (uint8_t)keys;
            lib_set_wb(&ks, (uint8_t)r.v[RF_SET]);
            r.v[RF_SET] = LIB_SET_WB;
        }
        st = harmony_style(&r) == CS_STRUM;
        songs[st]++;
        for (i = 0; i < 4096; i++) tk[i] = lk[i] = 1e9;
        for (rep = 0; rep < reps; rep++) {
            double t0 = now_us(), t, piece;
            int part;
            song_start(&s, &r);
            t = now_us() - t0;
            for (part = 0; part < SONG_FIRST_BAR_PARTS; part++) {   /* the first bar's planning, a part a block
                                                                       (app.c's song start) */
                t0 = now_us();
                song_first_bar(&s, part);
                piece = now_us() - t0;
                if (piece > t) t = piece;
            }
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
                int n, b0 = s.bar, s0 = s.step, k, land = 0;
                volatile float lp;
                if (req) {                       /* a request asked: on the group's second bar, or req_at 16ths early */
                    int at = b0 * 16 + s0, line = req_at > 0 ? (at + req_at) / 64 : (at - 16) / 64 + 1;
                    if (line >= 1 && (req_at > 0 ? (at + req_at) % 64 == 0 : (at - 16) % 64 == 0))
                        rq_ask(&s, (int)((seed + (uint32_t)line) % RQ_KINDS), line);
                    if (s0 == 0 && b0 % 4 == 0 && b0 >= 4)
                        land = 1 + (int)((seed + (uint32_t)(b0 / 4)) % RQ_KINDS);
                }
                t0 = now_us();
                n = song_tick(&s, ev, 64);
                t = now_us() - t0;
                if (t < tk[nt]) tk[nt] = t;
                tk0[nt] = (uint8_t)(s0 == 0);
                tkl[nt] = (uint8_t)land;
                t0 = now_us();
                for (k = 0; k < 200; k++) lp = song_lead_lp_at(&s, b0, s0);   /* the host clock's 1 us: a batch */
                t = (now_us() - t0) / 200.0;
                (void)lp;
                if (t < lk[nt]) lk[nt] = t;
                nt++;
                if (rep == 0)
                    for (i = 0; i < n; i++) {
                        const unsigned char *b = (const unsigned char *)&ev[i];
                        size_t j;
                        for (j = 0; j < sizeof ev[i]; j++) h[st] = (h[st] ^ b[j]) * 16777619u;
                    }
            }
        }
        for (i = 0; i < nt; i++) {
            if (tk[i] > mx[st]) { mx[st] = tk[i]; ws[st] = seed; }
            if (tkl[i]) {
                int q = tkl[i] - 1, a;
                if (tk[i] > lmax[q]) { lmax[q] = tk[i]; lseed[q] = seed; }
                lsumk[q] += tk[i];
                ln[q]++;
                for (a = 2; a <= 4 && a <= i; a++)   /* the bar before's 13th to 15th 16ths: its planning ahead */
                    amax[q] = tk[i - a] > amax[q] ? tk[i - a] : amax[q];
            }
            if (tk0[i]) {
                if (tk[i] > dmx) { dmx = tk[i]; dws = seed; }
                dsum += tk[i];
                dn++;
            } else {
                omx = tk[i] > omx ? tk[i] : omx;
                osum += tk[i];
                on++;
            }
            if (lk[i] > lmx) { lmx = lk[i]; lws = seed; }
            lsum += lk[i];
        }
        lsteps += nt;
        if (best > smx[st]) { smx[st] = best; wst[st] = seed; }
    }
    for (i = 0; i < 2; i++)
        printf("%-5s songs %4d  worst tick %5.1f us (seed %u)  worst start %4.1f us (seed %u)  events %08x\n",
               i ? "strum" : "other", songs[i], mx[i], (unsigned)ws[i], smx[i], (unsigned)wst[i], (unsigned)h[i]);
    printf("plan_build worst %.1f us, groove_ride worst %.1f us (one play: noisier)\n", pb, gr);
    printf("a bar's first 16th (the downbeat): worst %.1f us (seed %u), mean %.2f us; the other 16ths: worst %.1f us, mean "
           "%.2f us\n", dmx, (unsigned)dws, dn ? dsum / (double)dn : 0.0, omx, on ? osum / (double)on : 0.0);
    if (req)
        for (i = 0; i < RQ_KINDS; i++)
            printf("landing %-11s: worst %5.1f us (seed %u), mean %5.2f us over %ld; the bar before's planning ahead "
                   "worst %5.1f us\n", rq_name[i], lmax[i], (unsigned)lseed[i], ln[i] ? lsumk[i] / (double)ln[i] : 0.0,
                   ln[i], amax[i]);
    printf("lead loop filter per step: worst %.2f us (seed %u), mean %.3f us over %ld steps\n", lmx, (unsigned)lws,
           lsteps ? lsum / (double)lsteps : 0.0, lsteps);
    return 0;
}
#endif

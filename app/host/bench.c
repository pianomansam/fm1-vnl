/* SPDX-License-Identifier: MPL-2.0 */
/* Host cost bench (tools/cost-bench.sh): how long 60 s of a VNL song takes to render, against 60 s of AMB-1's engine
   at its heaviest piece. Built only with -DVNL_BENCH, otherwise this file is empty (it sits in host/ so the
   firmware globs skip nothing; it has a main()). No audio device. Usage: bench [--heaviest | [--forced] SEED... | --parts]
   Prints `vnl <ms> amb <ms> ratio <x>` (best of 3 runs each) for the seed(s), and for --heaviest the seed chosen.
   The lo-fi chain's cost does not depend on the song, and its fields are rolled independently of the rest, so the
   heaviest seed may not carry the heaviest chain: the picked seed (and every seed with --forced) is also timed with
   its chain forced to the heaviest real one (dust 3, vinyl, rain), printed as `forced`: the upper bound.
   Each step sets the section moves (plan_moves) as the app's step() does. VNL_FXOFF=<VX_OFF_* mask> switches effects
   off, to measure what each costs. VNL_FX=ours puts our plate and chorus in place of Rings' (the default).
   VNL_RUNS=<n> takes the best of n runs everywhere (default: 3 for the songs and
   AMB-1, 7 for --parts), for a busy host.
   The AMB-1 engine's headers clash with ours (MODE_*), so its few entry points are declared here by hand. */
#include <stdint.h>
#ifdef VNL_BENCH
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "song.h"
#include "voices.h"
#include "fx.h"
#include "library.h"

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

static int runs(int dflt)
{
    const char *e = getenv("VNL_RUNS");
    return e && atoi(e) > 0 ? atoi(e) : dflt;
}

static void rings_switch(void)
{
    const char *e = getenv("VNL_FX");
    fx_rings(!(e && !strcmp(e, "ours")));
}

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
    vx_fx_off(getenv("VNL_FXOFF") ? (unsigned)atoi(getenv("VNL_FXOFF")) : 0u);
    rings_switch();
    vx_init();
    song_start(&s, &roll);
    vx_song(&s.roll);
    while (done < total) {
        int n, i;
        if (render) {
            float lp[PM_N], hp[PM_N];
            plan_moves(&s.plan, s.bar, s.step, lp, hp);
            for (i = 0; i < PM_N; i++) {
                vx_set_filter(i, lp[i]);
                vx_set_hp(i, hp[i]);
            }
        }
        n = song_tick(&s, ev, 64);
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
    for (i = 0; i < runs(3); i++) { t = run_vnl(seed, 1, &ev, force); if (t < best) best = t; }
    return best;
}

/* The heaviest roll among seeds 1..200, ranked by its forced time (the chain forced to dust 3, vinyl and rain: what
   the budget is judged on), the best of 2 full 60 s renders each (~25 s in all; the per-seed time is noisy, so the
   pick is then re-timed best-of-3 in main) */
static uint32_t heaviest(void)
{
    uint32_t seed, best = 1;
    long ev;
    double t, slowest = -1;
    for (seed = 1; seed <= 200; seed++) {
        double a = run_vnl(seed, 1, &ev, 1), b = run_vnl(seed, 1, &ev, 1);
        t = a < b ? a : b;
        if (t > slowest) { slowest = t; best = seed; }
    }
    return best;
}

/* --parts: each component rendering alone for 60 s at 80 BPM, as a ratio to AMB-1's heaviest piece. The engine is
   dry (vx_init leaves fx_init's chain: the plate shut, no bed, dust 0) unless a part opens the chain with
   fx_configure; every row is the best of PART_RUNS, net of the idle dry engine's render (the floor). */
#define PART_RUNS (runs(7))
enum { P_IDLE, P_CHORDS, P_LEAD, P_CANDY, P_BASS, P_DRUMS, P_SOURCE };

static void ev(int voice, int note, int vel, uint32_t len)
{
    song_ev_t e;
    memset(&e, 0, sizeof e);
    e.voice = (uint8_t)voice;
    e.note = (uint8_t)note;
    e.vel = (uint8_t)vel;
    e.len = len;
    vx_event(&e);
}

/* The events of one 16th step (0-15) of a bar; st is a 16th in samples. */
static void part_step(int kind, int bar, int step, uint32_t st)
{
    static const int ch[4] = {55, 59, 62, 66}, ch2[4] = {53, 57, 60, 64};
    const int *c = (bar & 1) ? ch2 : ch;
    int i;
    switch (kind) {
    case P_CHORDS: /* four voices: a chord held a bar, re-struck each bar */
        if (!step)
            for (i = 0; i < 4; i++) ev(V_CHORD, c[i], 90, st * 15);
        break;
    case P_LEAD: /* 8ths, each held a dotted 8th, so two voices overlap */
        if (!(step & 1)) ev(V_LEAD, 72 + c[(step >> 1) & 3] % 12, 85, st * 3);
        break;
    case P_CANDY:
        if (!(step & 1)) ev(V_CANDY, 84 + c[(step >> 1) & 3] % 12, 80, st * 3);
        break;
    case P_BASS: /* one voice, 8ths, legato */
        if (!(step & 1)) ev(V_BASS, 36 + c[(step >> 1) & 3] % 12, 95, st * 2);
        break;
    case P_DRUMS: /* boom-bap, all six lanes */
        if (step == 0 || step == 7 || step == 10) ev(V_KICK, 0, 110, 0);
        if (step == 4 || step == 12) ev(V_SNARE, 0, 105, 0);
        if (!(step & 1)) ev(V_HAT, 0, 80, 0);
        if (step == 14) ev(V_OHAT, 0, 85, 0);
        if (step & 1) ev(V_SHAKER, 0, 60, 0);
        if (step == 6 || step == 15) ev(V_RIM, 0, 70, 0);
        break;
    case P_SOURCE: /* the fx source: a chord, a bass and the kit (a mix to run the chain on) */
        part_step(P_CHORDS, bar, step, st);
        part_step(P_BASS, bar, step, st);
        part_step(P_DRUMS, bar, step, st);
        break;
    }
}

typedef struct { int kind, chords, bass, lead, candy, kit, dust, noise, amb, chain; unsigned off; } part_t;

static part_t part_of(int kind)
{
    part_t p;
    memset(&p, 0, sizeof p);
    p.chords = p.bass = p.lead = p.candy = INS_NONE;
    p.kit = KIT_DUSTY;
    p.kind = kind;
    return p;
}

static double run_part(const part_t *p)
{
    static float l[BLK], r[BLK];
    uint32_t st = SR * 60u / 80u / 4u, total = SR * SECS, done = 0;
    int bar = 0, step = 0;
    double t0;
    rings_switch();
    vx_fx_off(p->off);
    vx_init();
    vx_assign(V_CHORD, p->chords);
    vx_assign(V_BASS, p->bass);
    vx_assign(V_LEAD, p->lead);
    vx_assign(V_CANDY, p->candy);
    vx_set_kit(p->kit);
    if (p->chain) fx_configure(p->dust, p->noise, p->amb, FL_LOFI);
    t0 = now_ms();
    while (done < total) {
        uint32_t left;
        part_step(p->kind, bar, step, st);
        for (left = st; left; ) {
            uint32_t b = left < BLK ? left : BLK, k;
            for (k = 0; k < b; k++) l[k] = r[k] = 0.0f;
            vx_render(l, r, b);
            left -= b;
        }
        done += st;
        if (++step == 16) { step = 0; bar++; }
    }
    t0 = now_ms() - t0;
    vx_fx_off(0u);
    return t0;
}

static double best_part(const part_t *p)
{
    double b = 1e30, t;
    int i;
    for (i = 0; i < PART_RUNS; i++) { t = run_part(p); if (t < b) b = t; }
    return b;
}

/* the lo-fi chain alone, 60 s at dust 0, the plate open, the chorus at 0.7, the delay on: a steady noise on the music
   and every wet input (what move 3 of docs/cost.md targets: the always-on chain, not the voices) */
static double run_chain(void)
{
    static float l[BLK], r[BLK], ch[BLK], dl[BLK];
    uint32_t done, x = 1u, k;
    double t0;
    rings_switch();
    fx_init();
    fx_configure(0, 0, 0, FL_LOFI);
    fx_chorus(0.7f);
    fx_land();
    t0 = now_ms();
    for (done = 0; done < SR * SECS; done += BLK) {
        for (k = 0; k < BLK; k++) {
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            l[k] = (float)(int32_t)x * 1e-10f;
            r[k] = 0.3f * l[k];
            ch[k] = l[k];
            dl[k] = 0.05f * l[k];
        }
        fx_process_buses(l, r, ch, dl, BLK, 0.0f);
    }
    return now_ms() - t0;
}

static void row(const char *grp, const char *name, double t, double base, double amb)
{
    printf("%-7s %-24s %7.1f ms  net %7.1f ms  ratio %.3f\n", grp, name, t, t - base, (t - base) / amb);
}

typedef struct { const char *n; int ins; } named_t;

static void role_rows(const char *grp, int kind, int role, const named_t *l, int n, double base, double amb)
{
    int i;
    for (i = 0; i < n; i++) {
        part_t p = part_of(kind);
        if (role == V_CHORD) p.chords = l[i].ins;
        else if (role == V_BASS) p.bass = l[i].ins;
        else if (role == V_LEAD) p.lead = l[i].ins;
        else p.candy = l[i].ins;
        row(grp, l[i].n, best_part(&p), base, amb);
    }
}

#define NEL(a) ((int)(sizeof(a) / sizeof((a)[0])))
static void parts(double amb)
{
    static const named_t CH[] = {{"rhodes", INS_RHODES}, {"wurli", INS_WURLI}, {"felt piano", INS_FELT_PIANO},
                                 {"nylon", INS_NYLON}, {"pad", INS_PAD}, {"vox pad", INS_VOX_PAD},
                                 {"organ", INS_ORGAN}, {"dx ep", INS_DX_EP}};
    static const named_t BS[] = {{"upright", INS_UPRIGHT}, {"elec bass", INS_ELEC_BASS}, {"sub", INS_SUB},
                                 {"triangle bass", INS_CHIP_BASS}, {"fm bass", INS_FM_BASS}};
    static const named_t LD[] = {{"flute", INS_FLUTE}, {"muted trumpet", INS_MUTED_TPT}, {"sax", INS_SAX},
                                 {"vibes", INS_VIBES}, {"kalimba", INS_KALIMBA}, {"guitar lick", INS_GUITAR_LICK},
                                 {"saw lead", INS_SAW_LEAD}, {"synth pluck", INS_SYNTH_PLUCK}};
    static const named_t CD[] = {{"music box", INS_MUSIC_BOX}, {"bells", INS_BELLS}};
    static const named_t KT[] = {{"dusty", KIT_DUSTY}, {"soft", KIT_SOFT}, {"808", KIT_808}, {"brushes", KIT_BRUSHES}};
    static const uint32_t sd[4] = {20, 100, 128, 183};
    part_t p = part_of(P_IDLE), q;
    double base = best_part(&p), src, plate, t, b = 1e30;
    long e;
    int k, j;
    printf("floor: idle dry engine, 60 s of silence: %.1f ms = ratio %.3f (subtracted from the rows; amb %.1f ms)\n",
           base, base / amb, amb);
    for (k = 0; k < 4; k++)
        for (j = 0; j < PART_RUNS; j++) { t = run_vnl(sd[k], 0, &e, 0); if (t < b) b = t; }
    printf("floor: song engine ticking only (seeds 20/100/128/183, best, no render): %.1f ms = ratio %.3f\n", b, b / amb);
    role_rows("chords", P_CHORDS, V_CHORD, CH, NEL(CH), base, amb);
    role_rows("bass", P_BASS, V_BASS, BS, NEL(BS), base, amb);
    role_rows("lead", P_LEAD, V_LEAD, LD, NEL(LD), base, amb);
    role_rows("candy", P_CANDY, V_CANDY, CD, NEL(CD), base, amb);
    for (k = 0; k < 4; k++) {
        p = part_of(P_DRUMS);
        p.kit = KT[k].ins;
        row("drums", KT[k].n, best_part(&p), base, amb);
    }
    /* the chain on a fixed source (rhodes chord, sub bass, dusty kit); the rows are net of the source dry, except
       the "over plate" ones, which are net of the source with the plate open and dust 0 */
    q = part_of(P_SOURCE);
    q.chords = INS_RHODES;
    q.bass = INS_SUB;
    src = best_part(&q);
    row("source", "dry (no chain)", src, base, amb);
    q.off = VX_OFF_MUD;
    t = best_part(&q);
    q.off = 0u;
    row("mix", "mud high-pass (on source)", src, t, amb);
    q.chain = 1;
    plate = best_part(&q);
    row("fx", "plate + dust 0", plate, src, amb);
    q.off = VX_OFF_GLUE;
    t = best_part(&q);
    q.off = 0u;
    row("mix", "glue (over plate)", plate, t, amb);
    for (k = 1; k <= 3; k++) {
        char nm[32];
        q.dust = k;
        snprintf(nm, sizeof nm, "dust %d (over plate)", k);
        row("fx", nm, best_part(&q), plate, amb);
    }
    q.dust = 0;
    q.noise = 1;
    row("fx", "vinyl (over plate)", best_part(&q), plate, amb);
    q.noise = 2;
    row("fx", "tape (over plate)", best_part(&q), plate, amb);
    q.noise = 0;
    q.amb = 1;
    row("fx", "rain (over plate)", best_part(&q), plate, amb);
    q.dust = 3;
    q.noise = 1;
    row("fx", "full chain (3,vinyl,rain)", best_part(&q), src, amb);
    printf("(kick ducking runs inside dust_run at every level: not separable)\n");
    for (k = 0, b = 1e30; k < PART_RUNS; k++) { t = run_chain(); if (t < b) b = t; }
    row("fx", "chain alone (dust 0)", b, 0.0, amb);
}

int main(int argc, char **argv)
{
    double amb = 0, t;
    int p, heavy = 0, i, ap = 0, forced = 0, doparts = 0;
    uint32_t seeds[64];
    int ns = 0;
    for (i = 1; i < argc && ns < 64; i++) {
        if (!strcmp(argv[i], "--forced")) forced = 1;
        else if (!strcmp(argv[i], "--parts")) doparts = 1;
        else if (argv[i][0] == '-') heavy = 1;
        else if (ns < 64) seeds[ns++] = (uint32_t)strtoul(argv[i], 0, 10); /* (64 at most) */
    }
    if (!doparts && (heavy || !ns)) { seeds[0] = heaviest(); ns = 1; forced = 1; printf("heaviest seed %u\n", (unsigned)seeds[0]); }
    /* AMB-1's heaviest piece: the slowest of its eight (best of 3 each) */
    for (p = 0; p < EN_NPIECES; p++) {
        double b = 1e30;
        for (i = 0; i < runs(3); i++) { t = run_amb(p); if (t < b) b = t; }
        if (b > amb) { amb = b; ap = p; }
    }
    if (doparts) { parts(amb); return 0; }
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

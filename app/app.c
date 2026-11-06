/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal (fm1-base's template, which this started from) */
/* VNL-1 on the platform: generated songs, one after another, with a 2-4 s silence between them.
 *
 * Two contexts. The main loop (app_frame) rolls the next song (roll_song, chained seed) and hands it over through a
 * one-slot mailbox; it also turns the knobs into commands on a small queue. The render (app_render, the audio
 * interrupt) owns the song_t and the voices: at a block's start it drains the queue and, when it needs a song,
 * takes the mailbox's; then it ticks the song as each 16th's start sample arrives, feeds every event to the
 * voices at that moment, renders, and scales by MASTER. What the screen shows the render publishes as a snapshot
 * (a sequence count around it, so the main loop never reads it half written; the render never waits).
 *
 * The bed (vinyl or tape surface, rain) opens and closes each song: it fades in as the song starts and out over the
 * gap's first 1.5 s (vx_release_all closes it). The engine is cleared at a song's start only when nothing is left
 * to hear (vx_sounding: no notes, no plate tail, no bed fade), so a song change never cuts a tail; a request that
 * changes the set, the dust, the noise or the ambience reconfigures the voices where it lands (step()).
 *
 * Controls (minimal; the real UI comes later): the FM-1 boots stopped, a song rolled and ready. PLAY stops / plays (a
 * stop is true silence; play after a stop continues the same song from the start of the bar it stopped in, every
 * change waiting still landing where it would have), B_NEW (SEQ) rolls a new song (playing: after the usual gap;
 * stopped: ready for PLAY), K1 key +-1 semitone, K2 tempo +-1 BPM, K3 motion +-5, K4 mood +-1: each a request that
 * lands on the song's next 4-bar boundary (stopped, the knobs do nothing). Over USB the workbench's device mock may
 * take every control, the FM-1 silent meanwhile (the browser's remote, below). Part of the unity build (device/unity.c,
 * host/plat_host.c), after the platform: gfx.c and announce.c are in scope. */
#include <string.h>
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
#include <stdlib.h>
#endif
#include "song.h"
#include "harmony.h"
#include "override.h"
#include "theory.h"
#include "library.h"
#include "voices.h"
#include "fx.h"

#define BARRIER() __asm__ volatile("" ::: "memory")

#ifndef PLAT_HOST
/* the firmware's memmove (app/inc/string.h: the toolchain is freestanding). Built with -fno-builtin, so the loops
   stay loops (no call back into a memmove). */
void *vnl_memmove(void *d, const void *s, unsigned n)
{
    unsigned char *p = d;
    const unsigned char *q = s;
    if (p < q)
        while (n--)
            *p++ = *q++;
    else
        while (n--)
            p[n] = q[n];
    return d;
}
#endif

/* the simplest spelling of a major key and of a minor one (the fewer accidentals; a tie: F# major (Sam,
   2026-11-05), Eb minor) */
static const char *const KEY_NAME[MODE_N][12] = {{"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"},
                                                 {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"}};
static const char *const MODE_NAME[MODE_N] = {"MAJOR", "MINOR"};
static const char *const MOOD_NAME[4] = {"SAD", "WISTFUL", "WARM", "SUNNY"};
static const char *const SEC_NAME[] = {"INTRO", "GROOVE", "LEAD", "BREAK", "OUTRO"};
/* the four knobs: the field each asks for, its step a detent, its name */
static const uint8_t KNOB_FIELD[4] = {RF_KEY, RF_TEMPO, RF_MOTION, RF_MOOD};
static const int8_t KNOB_STEP[4] = {1, 1, 5, 1};
static const char *const KNOB_NAME[4] = {"KEY", "TEMPO", "MOTION", "MOOD"};

/* a field's value after d more steps from v: the key wraps, the rest stop at their ends */
static int field_step(int f, int v, int d)
{
    if (f == RF_KEY)
        return ((v + d) % 12 + 12) % 12;
    v += d;
    if (f == RF_TEMPO)
        return v < 70 ? 70 : v > 90 ? 90 : v;
    if (f == RF_MOTION)
        return v < 0 ? 0 : v > 100 ? 100 : v;
    return v < 0 ? 0 : v > 3 ? 3 : v;    /* RF_MOOD */
}

/* ------------------------------------------------------- main -> render --- */
/* the next song, rolled by the main loop: full until the render takes it */
static song_roll_t mb_roll;
static volatile uint8_t mb_full;

/* the button that rolls a new song (one constant: remap it here) */
#define B_NEW B_SEQ

/* commands, drained at a block's start (AMB-1's pattern) */
enum { C_PLAY, C_REQ, C_STOP, C_NEW };
#define QN 32u
static uint32_t q[QN];
static volatile uint32_t q_w, q_r;
static void post(uint32_t c, int a, int b)
{
    uint32_t w = q_w;
    if (w - q_r >= QN)
        return;                          /* full: dropped (never, at the UI's rate) */
    q[w % QN] = c << 24 | ((uint32_t)a & 0xFFu) << 16 | ((uint32_t)b & 0xFFFFu);
    BARRIER();
    q_w = w + 1;
}

/* ------------------------------------------------------- render -> main --- */
typedef struct {
    uint8_t playing, has_song, stopped;
    uint8_t ready, held;                 /* stopped, a song ready (roll); held: one to continue (bar, section) */
    uint8_t req_set[RF_N];               /* a knob's request waiting for the boundary, and its value */
    int16_t req_val[RF_N];
    int32_t songs, bar, section, flags, pend_field, pend_bars, voices;
    song_roll_t roll;
} snap_t;
static snap_t snap;
static volatile uint32_t snap_seq;       /* odd while the render writes */

static void snap_read(snap_t *o)        /* main loop: a consistent copy */
{
    uint32_t a, b;
    do {
        a = snap_seq;
        BARRIER();
        *o = snap;
        BARRIER();
        b = snap_seq;
    } while (a != b || (a & 1u));
}

/* What each song played, for a musician (host simulator only, app/host/score.c): the song log, MIDI and MusicXML */
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
#include "score.h"
static uint32_t slog_samples;
#define SLOG_NOW() ((float)slog_samples / 44100.0f)
#define SLOG_SONG(s) score_song(s, SLOG_NOW())
#define SLOG_STEP(s, bar, st, ev, n) score_step(s, bar, st, ev, n, SLOG_NOW())
#define SLOG_GAP(secs) score_gap(SLOG_NOW(), secs)
#define SLOG_RESUME(bar) score_resume(bar, SLOG_NOW())
#define SLOG_TIME(n) (slog_samples += (n))
#else
#define SLOG_SONG(s) ((void)0)
#define SLOG_STEP(s, bar, st, ev, n) ((void)0)
#define SLOG_GAP(secs) ((void)0)
#define SLOG_RESUME(bar) ((void)0)
#define SLOG_TIME(n) ((void)0)
#endif

/* ------------------------------------------------------------- render --- */
enum { RS_STOP, RS_WAIT, RS_SONG, RS_GAP };   /* stopped; playing: waiting for a song, in one, between two */
static song_t song;
static uint8_t rs = RS_STOP;             /* boots stopped: PLAY plays the song in the mailbox */
static uint8_t held;                     /* stopped in a song: PLAY continues it from `mark` */
/* the song as it was at the start of the bar playing (taken at the bar's first 16th, before its tick), and the
   render's place then: a stop goes back to it */
static song_t mark;
static uint32_t mark_pos, mark_tick;
static float mark_frac;
static volatile uint8_t panic_now;
static volatile uint8_t remote_mute;     /* the browser has the controls: the output ramps to silence */
static volatile int32_t songs_started;
static uint32_t r_now;                   /* samples since the song started (or the gap's left) */
static uint32_t r_pos;                   /* the next step's start sample: integer and fraction, advanced as */
static float r_frac;                     /* song.c advances its own (so the two agree on every boundary) */
static uint32_t r_tick;                  /* steps ticked this song */
static uint32_t gap_left;
static uint8_t req_set[RF_N];
static int16_t req_val[RF_N];
static float mix_l[APP_BLOCK], mix_r[APP_BLOCK], gain_now;
static song_ev_t evs[64];

static void voices_begin(void)
{
    if (vx_sounding())                   /* a tail (notes, the plate, the bed's fade) rings on: keep it */
        vx_set_gain(1.0f);
    else
        vx_init();
    vx_song(&song.roll);                 /* this song's instruments, kit and chain (the bed fades in, the dust ramps
                                            if anything still sounds; the chain's noise from its seed), before its
                                            first event */
}

static void song_begin(void)
{
    song_start(&song, &mb_roll);
    BARRIER();
    mb_full = 0;                         /* the main loop rolls the next one */
    voices_begin();
    r_now = r_pos = r_tick = 0;
    r_frac = 0.0f;
    memset(req_set, 0, sizeof req_set);
    for (int lp = 0; lp < VX_PART_N; lp++)   /* a new song: the mixer back to 0 dB (Sam, 2026-11-06; a resume keeps it) */
        vx_level_set(lp, 0);
    songs_started++;
    held = 0;
    rs = RS_SONG;
    SLOG_SONG(&song);
}

/* PLAY after a stop: the song from the start of the bar it stopped in (its first 16th's events again, on the
   downbeat), the changes waiting at the stop carried over, each landing on the bar it would have */
static void song_resume(void)
{
    song_carry_requests(&mark, &song);
    song = mark;
    r_now = r_pos = mark_pos;
    r_frac = mark_frac;
    r_tick = mark_tick;
    voices_begin();
    held = 0;
    rs = RS_SONG;
    SLOG_RESUME(song.bar);
}

/* the music stops (the voices already told): a song cut short is held to continue */
static void stop_here(void)
{
    if (rs == RS_STOP)
        return;
    held = rs == RS_SONG && song.state == SS_PLAYING;
    rs = RS_STOP;
}

/* the silence between songs: 2-4 s, from the song's seed (at its end, or cut short by B_NEW) */
static void gap_begin(void)
{
    uint32_t h = song.roll.seed * 2654435761u;
    vx_release_all();
    gap_left = (2000u + (h >> 16) % 2001u) * 441u / 10u;
    SLOG_GAP((float)gap_left / 44100.0f);
    rs = RS_GAP;
}

/* one 16th: the fade (if the song's last block fades), the section moves (each role bus's filters, plan_moves), the
   requests' slots, the step's events to the voices and the pad layer's level (song_pad_level) */
static void step(void)
{
    const plan_block_t *last = &song.plan.b[song.plan.n ? song.plan.n - 1 : 0];
    int bar = song.bar, st = (int)(r_tick & 15u), n, i;
    float adv, lp[PM_N], hp[PM_N], llp;
    uint32_t ip;
    if (st == 0) {                       /* a stop in this bar goes back here */
        mark = song;
        mark_pos = r_pos;
        mark_frac = r_frac;
        mark_tick = r_tick;
    }
    plan_moves(&song.plan, bar, st, lp, hp);   /* the engine ramps them (about 50 ms; sweeps move per step) */
    vx_organ_scan(plan_full_kit(&song.plan, bar)); /* the pad's organ's scanner: on where the full kit plays */
    for (i = 0; i < PM_N; i++) {
        if (i != PM_LEAD) vx_set_filter(i, lp[i]);   /* the lead's after the tick */
        vx_set_hp(i, hp[i]);
    }
    if (song.plan.n && (last->flags & PB_FADE) && bar >= last->start) {
        int len = last->bars * 16, at = (bar - last->start) * 16 + st + 1;
        vx_set_gain(1.0f - (float)at / (float)len);
    }
    if (st == 0 && bar % 4 == 0)
        memset(req_set, 0, sizeof req_set);   /* every waiting request lands on this boundary */
    {                                     /* a request that lands here on what the voices read reaches them */
        song_roll_t before = song.roll;
        n = song_tick(&song, evs, 64);
        llp = song_lead_lp_at(&song, bar, st);   /* the lead loop's filter, for the 16th just played */
        vx_set_filter(PM_LEAD, lp[PM_LEAD] < llp ? llp : lp[PM_LEAD]);   /* before its events */
        vx_set_pad_level(song_pad_level(&song));   /* the pad layer's fade over a section line */
        if (vx_reconfigures(&before, &song.roll)) /* sounding notes keep their patches; dust and bed changes ramp,
                                                     the delay's time glides to a new tempo */
            vx_configure(&song.roll);
    }
    SLOG_STEP(&song, bar, st, evs, n);
    for (i = 0; i < n; i++)
        vx_event(&evs[i]);
    adv = song_step_samples(&song) + r_frac;
    ip = (uint32_t)adv;
    r_frac = adv - (float)ip;
    r_pos += ip;
    r_tick++;
}

static void command(uint32_t c)
{
    uint32_t op = c >> 24;
    int a = (int)((c >> 16) & 0xFFu), b = (int)(int16_t)(c & 0xFFFFu);
    if (op == C_STOP) {                  /* the browser takes the controls: stopped (already: nothing) */
        if (rs != RS_STOP) {
            vx_stop();
            stop_here();
        }
    } else if (op == C_PLAY) {
        if (rs != RS_STOP) {
            vx_stop();                   /* the notes, the bed and the plate out in 40 ms */
            stop_here();
        } else if (held)
            song_resume();
        else
            rs = RS_WAIT;                /* the song in the mailbox */
    } else if (op == C_NEW) {
        if (rs == RS_SONG)
            gap_begin();                 /* the mailbox's song after the gap, as at a song's end */
        else if (rs == RS_STOP) {        /* stopped, a new song ready: the mailbox's, never heard, or (none held) */
            if (held)
                held = 0;
            else
                mb_full = 0;             /* the one ready dropped: the main loop rolls the next */
        }                                /* (in the gap or waiting: a new song is already coming) */
    } else if (op == C_REQ && rs == RS_SONG && song.state == SS_PLAYING && a < RF_N) {
        int v = field_step(a, req_set[a] ? req_val[a] : song.roll.v[a], b);
        if (v != (req_set[a] ? req_val[a] : song.roll.v[a])) {
            int at = (r_tick & 15u) == 0 && song.bar % 4 == 0 ? song.bar : (song.bar / 4 + 1) * 4;
            song_request(&song, a, v);
            if (at < song.roll.v[RF_BARS]) {   /* (song_request refuses one the song's end comes before) */
                req_set[a] = 1;
                req_val[a] = (int16_t)v;
            }
        }
    }
}

static void publish(void)
{
    const plan_block_t *b = rs == RS_SONG ? plan_at(&song.plan, song.bar) : 0;
    int f = -1, bars = -1;
    snap_seq++;
    BARRIER();
    snap.playing = rs != RS_STOP;
    snap.stopped = rs == RS_STOP;
    snap.has_song = rs == RS_SONG && song.state == SS_PLAYING;
    snap.songs = songs_started;
    snap.voices = vx_active();
    if (snap.has_song) {
        snap.roll = song.roll;
        snap.bar = song.bar;
        snap.section = b ? b->section : -1;
        snap.flags = b ? b->flags : 0;
        if (!song_pending(&song, &f, &bars))
            f = bars = -1;
        memcpy(snap.req_set, req_set, sizeof req_set);
        memcpy(snap.req_val, req_val, sizeof req_val);
    } else {
        snap.section = -1;
    }
    snap.held = rs == RS_STOP && held;
    snap.ready = snap.held || (rs == RS_STOP && mb_full);   /* (the main loop writes mb_roll only while it's empty) */
    if (snap.held) {                     /* the song held, at the bar it continues from */
        b = plan_at(&mark.plan, mark.bar);
        snap.roll = song.roll;
        snap.bar = mark.bar;
        snap.section = b ? b->section : -1;
        snap.flags = b ? b->flags : 0;
    } else if (snap.ready) {             /* the song in the mailbox, from its start */
        snap.roll = mb_roll;
        snap.bar = 0;
    }
    snap.pend_field = f;
    snap.pend_bars = bars;
    BARRIER();
    snap_seq++;
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i = 0, k;
    float gain = (float)master_q12 * (8388607.0f / 4096.0f);   /* MASTER, read once a block: float -> 24-bit */
    if (remote_mute)
        gain = 0.0f;                     /* (gain_now ramps there: no click) */
    if (panic_now) {
        panic_now = 0;
        vx_init();
        stop_here();
    }
    while (q_r != q_w) {
        uint32_t c = q[q_r % QN];
        BARRIER();
        q_r++;
        command(c);
    }
    if (rs == RS_WAIT && mb_full)
        song_begin();
    memset(mix_l, 0, sizeof(float) * n);
    memset(mix_r, 0, sizeof(float) * n);
    while (i < n) {
        uint32_t seg = n - i;
        if (rs == RS_SONG) {
            if (r_now >= r_pos) {        /* a step starts here (or, ended, the song's last sample is past) */
                if (song.state == SS_PLAYING)
                    step();
                else
                    gap_begin();
                continue;
            }
            if (r_pos - r_now < seg)
                seg = r_pos - r_now;
        } else if (rs == RS_GAP) {
            if (!gap_left) {
                rs = RS_WAIT;            /* the next song at the next block (it is in the mailbox by then) */
                continue;
            }
            if (gap_left < seg)
                seg = gap_left;
            gap_left -= seg;
        }
        if (rs == RS_SONG || vx_sounding())   /* true silence once the last release, the plate's tail and the
                                                  bed's fade are over */
            vx_render(mix_l + i, mix_r + i, seg);
        r_now += seg;
        SLOG_TIME(seg);
        i += seg;
    }
    for (k = 0; k < n; k++) {
        gain_now += (gain - gain_now) * 0.004f;   /* ~5 ms: turning MASTER does not click */
        float a = mix_l[k] * gain_now, b = mix_r[k] * gain_now;
        lr[2 * k] = (int32_t)(a > 8388607.0f ? 8388607.0f : a < -8388607.0f ? -8388607.0f : a);   /* 24-bit */
        lr[2 * k + 1] = (int32_t)(b > 8388607.0f ? 8388607.0f : b < -8388607.0f ? -8388607.0f : b);
    }
    publish();
}

void app_panic(void)
{
    panic_now = 1;
}

/* --------------------------------------------------------- the roller --- */
static song_roll_t last_roll;
static uint32_t next_seed;
static uint8_t rolled_any, seeded;
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
static uint32_t seed_override;           /* VNL_SEED (host): the first song's seed */
static int set_override;                 /* VNL_SET (host): the first song's instrument set (roll_set: if its mood allows) */
static int has_seed_override;
#endif
static uint32_t boot_spins;              /* main-loop passes between frames, until the first seed */

static uint32_t seed_next(uint32_t s)   /* the chain: a song's seed gives the next one's (xorshift32) */
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s ? s : 0x9E3779B9u;
}

/* The first seed, in the main loop. The simulator (and the emulator) seed from the time alone, so a script plays
   the same songs every run. The FM-1 boots in nearly the same number of milliseconds every time, so it also mixes
   in MASTER's reading, the time at its first two frames and the main loop's passes between them (USB, the timer
   and the render make those vary): power-on is a fresh roll, with nothing written to the flash. Rolled at the
   second frame, ~16 ms after the first; the render is silent until then. */
static void seed_boot(void)
{
#ifdef PLAT_HOST
    next_seed = seed_next(plat_ms() * 2654435761u + 0x5EEDu);
#ifndef PLAT_WEB
    if (has_seed_override) next_seed = seed_override;
#endif
    seeded = 1;
#else
    static uint32_t frames, mix;
    mix = (mix ^ plat_ms() ^ plat_master() << 12 ^ boot_spins << 20) * 2654435761u;
    mix ^= mix >> 15;
    if (++frames < 2u)
        return;
    next_seed = seed_next(mix + 0x5EEDu);
    seeded = 1;
#endif
}

static void roll_next(void)             /* main loop: keep the mailbox full */
{
    if (mb_full || !seeded)
        return;
    roll_song(rolled_any ? &last_roll : 0, next_seed, &mb_roll);
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
    if (set_override && !rolled_any)
        roll_set(&mb_roll, RF_SET, set_override);
#endif
    last_roll = mb_roll;
    rolled_any = 1;
    next_seed = seed_next(next_seed);
    BARRIER();
    mb_full = 1;
}

void app_init(void)
{
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
    {                                    /* exploring in renders: VNL_DROP2=1, VNL_LOOPLOCK=0, VNL_SWAP (below) */
        const char *d = getenv("VNL_DROP2"), *w = getenv("VNL_SWAP"), *l = getenv("VNL_LOOPLOCK");
        th_set_drop2(d && *d == '1');
        harmony_set_loop_lock(!(l && *l == '0'));
        {
            const char *sp = getenv("VNL_BASS");                    /* SPIKE: bass rhythm, 1-5 */
            song_set_bass_spike(sp ? *sp - '0' : 0);
        }
        {
            const char *pd = getenv("VNL_PADS");   /* VNL_PADS=double: the pad's chord part doubles the keys (A/B) */
            song_set_pads_double(pd && !strcmp(pd, "double"));
        }
        {
            const char *fx = getenv("VNL_FX");     /* VNL_FX=ours: our plate and chorus in place of Rings' (compare) */
            fx_rings(!(fx && !strcmp(fx, "ours")));
        }
        {
            const char *bd = getenv("VNL_BED");    /* VNL_BED=0: no surface noise or rain, to hear the music */
            vx_bed_off(bd && *bd == '0');
        }
        {
            const char *sd = getenv("VNL_SEED");   /* the first song's seed, as a log prints it: replay or roll anew */
            if (sd && *sd) { seed_override = (uint32_t)strtoul(sd, 0, 10); has_seed_override = 1; }
        }
        {
            const char *st = getenv("VNL_SET");    /* VNL_SET=4: the first song on that set (a render of one the roll
                                                      seldom picks first) */
            set_override = st ? atoi(st) : 0;
        }
        {
            const char *cp = getenv("VNL_COMP");   /* VNL_COMP=<CS_*>: every song comps in that style (6 held on 2,
                                                      8 held on 2 and 4; 7, removed, holds; held where the keys
                                                      can't play it) */
            if (cp && *cp >= '0' && *cp <= '9' && atoi(cp) < CS_N) {
                song_ov_t o = *song_ov();
                o.comp = (int8_t)atoi(cp);
                song_override(&o);
            }
        }
        {
            const char *cr = getenv("VNL_COMP_PAT");   /* VNL_COMP_PAT=<id>: every song's keys play that comp rhythm
                                                          (patterns/model.json's comp-rhythms id, 7 the first past the
                                                          fixed six; a candidate too), where they can */
            if (cr && atoi(cr) > 6 && atoi(cr) - 6 <= 127) {
                song_ov_t o = *song_ov();
                o.comp_pat = (int8_t)(atoi(cr) - 6);
                song_override(&o);
            }
        }
        {
            const char *bz = getenv("VNL_BUSY");   /* VNL_BUSY=0: a busy lead leaves the comping's rhythm be (the
                                                      workbench's "Busy lead thins the comping" off) */
            if (bz && bz[0] == '0' && !bz[1]) {
                song_ov_t o = *song_ov();
                o.no_busy_thin = 1;
                song_override(&o);
            }
        }
        while (w && *w) {                /* VNL_SWAP="Eb3 G3 Db4 F4=Db3 G3 Eb4 F4;...": hear an alternative voicing */
            int v[8], k = 0;
            while (*w && *w != ';' && k < 8) {
                static const int8_t LETTER[7] = {9, 11, 0, 2, 4, 5, 7};   /* A B C D E F G */
                int pc, oct = 0, neg = 0;
                while (*w == ' ' || *w == '=') w++;
                if (*w < 'A' || *w > 'G') break;
                pc = LETTER[*w++ - 'A'];
                if (*w == 'b') { pc--; w++; } else if (*w == '#') { pc++; w++; }
                if (*w == '-') { neg = 1; w++; }
                while (*w >= '0' && *w <= '9') oct = oct * 10 + *w++ - '0';
                v[k++] = (neg ? -oct : oct) * 12 + 12 + pc;
            }
            if (k == 8) th_add_swap(v, v + 4);
            while (*w && *w != ';') w++;
            if (*w == ';') w++;
        }
    }
#endif
    vx_init();                           /* the first song: app_frame, once seeded */
}

/* --------------------------------------------------------------------- UI --- */
static struct {
    char msg[2][24];                     /* a short message over the header */
    uint32_t msg_until;
    uint32_t btn;                        /* held at the last input */
    snap_t shown;                        /* what the screen shows */
    uint8_t redraw;
} ui;

/* the browser's remote (below) */
#define RM_PFX0 0x7Du
#define RM_PFX1 0x56u
#define RM_PFX2 0x4Eu
enum { RM_ON = 0x01, RM_OFF = 0x02, RM_LIGHTS = 0x03, RM_STATE = 0x11, RM_BTN = 0x20, RM_ENC = 0x21, RM_KEY = 0x22 };
#define RM_TIMEOUT 3000u
#define RM_BQ 32u
#define RM_KEY0 0x20u                    /* a key's edge in the queue: RM_KEY0 + k (a button's: B_*, under it) */
static struct {
    uint8_t on;
    uint32_t heard, link;                /* the last 01's time; the USB connection it came over */
    uint8_t bq[RM_BQ];                   /* button and key edges to send: B_* or RM_KEY0 + k, bit 7 down */
    uint32_t bq_w, bq_r;
    uint32_t keys;                       /* the keys held at the last input */
    int16_t enc[NE];                     /* detents to send */
    uint8_t ack;                         /* a state message owed (1 + its ss) */
    uint32_t lit, blink;                 /* the keys the browser lights, and those it blinks (03) */
} rm;

static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}

static void int_text(int32_t v, char *b)     /* signed, no padding */
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}

void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    ui.redraw = 1;
    if (ann_on()) {                          /* and to a screen reader (announce.c, fm1-reader) */
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            text_cat(t, " ");
            text_cat(t, ui.msg[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

/* a field's value as the screen writes it */
static void value_text(int f, int v, int mode, char *t)
{
    t[0] = 0;
    if (f == RF_KEY)
        text_cat(t, KEY_NAME[mode % MODE_N][v % 12]);
    else if (f == RF_MOOD)
        text_cat(t, MOOD_NAME[v & 3]);
    else
        int_text(v, t);
}

/* the screen's lines, as text: the header's key and tempo, the place in the song, the chord, the pending change */
static void line_key(const snap_t *s, char *t)    /* "F MINOR  78 BPM" */
{
    char n[8];
    t[0] = 0;
    text_cat(t, KEY_NAME[s->roll.v[RF_MODE] % MODE_N][s->roll.v[RF_KEY] % 12]);
    text_cat(t, " ");
    text_cat(t, MODE_NAME[s->roll.v[RF_MODE] % MODE_N]);
    text_cat(t, "  ");
    int_text(s->roll.v[RF_TEMPO], n);
    text_cat(t, n);
    text_cat(t, " BPM");
}

static void line_place(const snap_t *s, char *t)  /* "CLASSIC  GROOVE  12/48" */
{
    const lib_map_t *m = lib_map_by_id((uint8_t)s->roll.v[RF_MAP]);
    char n[8];
    uint32_t i;
    t[0] = 0;
    text_cat(t, m ? m->name : "");
    for (i = 0; t[i]; i++)
        if (t[i] >= 'a' && t[i] <= 'z')
            t[i] = (char)(t[i] - 32);
    text_cat(t, "  ");
    text_cat(t, s->section >= 0 && s->section <= SEC_OUTRO ? SEC_NAME[s->section] : "");
    text_cat(t, "  ");
    int_text(s->bar + 1, n);
    text_cat(t, n);
    text_cat(t, "/");
    int_text(s->roll.v[RF_BARS], n);
    text_cat(t, n);
}

static void line_chord(const snap_t *s, char *t)  /* the chord of the bar playing */
{
    const lib_prog_t *p = lib_prog_by_id((uint8_t)s->roll.v[RF_PROG]);
    int pcs[5], np, loop = p && p->n * p->bars_per_chord ? p->n * p->bars_per_chord : 4;
    np = harmony_chord_at(&s->roll, s->bar, s->bar / loop + 1, (s->flags & PB_DARK) != 0, pcs);
    t[0] = 0;
    if (np > 0)
        th_chord_name(pcs, np, s->roll.v[RF_KEY], s->roll.v[RF_MODE], t);
}

static void line_pending(const snap_t *s, char *t)   /* "TEMPO 78 IN 3 BARS" */
{
    char n[12];
    int k;
    t[0] = 0;
    if (s->pend_field < 0)
        return;
    for (k = 0; k < 4; k++)
        if (KNOB_FIELD[k] == s->pend_field)
            break;
    text_cat(t, k < 4 ? KNOB_NAME[k] : "CHANGE");
    if (s->req_set[s->pend_field]) {
        text_cat(t, " ");
        value_text(s->pend_field, s->req_val[s->pend_field], s->roll.v[RF_MODE], n);
        text_cat(t, n);
    }
    text_cat(t, " IN ");
    int_text(s->pend_bars, n);
    text_cat(t, n);
    text_cat(t, s->pend_bars == 1 ? " BAR" : " BARS");
}

/* a screen reader that has just started listening (or asks) hears the whole screen */
static void speak(void)
{
    char t[160], l[40];
    if (ann_on() && ann_where()) {
        t[0] = 0;
        text_cat(t, APP_NAME ". ");
        if (rm.on)
            text_cat(t, "BROWSER. CONTROLLING THE WORKBENCH");
        else if (!ui.shown.has_song) {
            text_cat(t, ui.shown.stopped ? "STOPPED" : "NEXT SONG");
            if (ui.shown.ready) {            /* the song ready: its key and tempo, and where it continues from */
                text_cat(t, ". ");
                line_key(&ui.shown, l);
                text_cat(t, l);
            }
            if (ui.shown.held) {
                text_cat(t, ". ");
                line_place(&ui.shown, l);
                text_cat(t, l);
            }
        }
        else {
            line_key(&ui.shown, l);
            text_cat(t, l);
            text_cat(t, ". ");
            line_place(&ui.shown, l);
            text_cat(t, l);
            text_cat(t, ". CHORD ");
            line_chord(&ui.shown, l);
            text_cat(t, l);
            line_pending(&ui.shown, l);
            if (l[0]) {
                text_cat(t, ". ");
                text_cat(t, l);
            }
        }
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(plat_ms());
}

/* ------------------------------------------------------- the browser's remote --- */
/* The workbench's device mock (workbench/ui/usb.js) takes the FM-1's controls over USB-MIDI SysEx, prefix
 * F0 7D 56 4E ("VN"; base's are 7D 4C 43 / 47 / 57 and 7D 53 52, the updater's and the mirror's the update's
 * framing), one message an event:
 *   browser -> FM-1   01 remote on (and its keepalive, about once a second)   02 remote off
 *   FM-1 -> browser   11 ss 01   the state, ss 1 on (the answer to each 01) or 0 off (leaving), protocol 1
 *                     20 bb ss   button bb (plat.h's B_*) down (ss 1) or up (ss 0)
 *                     21 ee dd   encoder ee (EN_*: SELECT ALGO PRESET K1..K4) turned dd detents (7-bit two's
 *                                complement, -64..63; + clockwise)
 *                     22 kk ss   key kk (0..26, plat.h's: 0 = F3) down (ss 1) or up (ss 0)
 *   browser -> FM-1   03 l0 l1 l2 l3 b0 b1 b2 b3   the keys to light (l: key k is bit k%7 of byte k/7) and those to
 *                                blink (b, 4 Hz; Sam 2026-11-06: a part on lit, a soloed one blinking)
 * While on, the song stops, the output is silent and the controls go to the browser only, the keys too (neither acts
 * here). It ends on 02, after 3 s without a 01, or at a new USB connection; the FM-1 stays stopped,
 * the song held: PLAY continues it. The messages go out on the MIDI ring when it has room (plat_midi_out_room, never
 * waiting): button edges wait in a small queue, an encoder's detents add up until they go. */
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
static char rm_sent[2048];               /* the simulator's: the messages sent since the last "remote_sent" */
#endif

/* one message, F0 7D 56 4E a b c F7, as three USB-MIDI SysEx packets (CIN 4 goes on, 6 ends with two bytes) */
static int rm_send(uint8_t a, uint8_t b, uint8_t c)
{
    uint32_t f;
    if (plat_midi_out_room(TO_USB) < 3u + 4u)   /* (some room kept for anyone else) */
        return 0;
    f = plat_lock();                     /* the three together */
    plat_midi_out(0x04u | 0xF0u << 8 | RM_PFX0 << 16 | RM_PFX1 << 24);
    plat_midi_out(0x04u | RM_PFX2 << 8 | (uint32_t)a << 16 | (uint32_t)b << 24);
    plat_midi_out(0x06u | (uint32_t)c << 8 | 0xF7u << 16);
    plat_unlock(f);
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
    {
        size_t n = strlen(rm_sent);
        if (n + 24 < sizeof rm_sent)
            snprintf(rm_sent + n, sizeof rm_sent - n, "F07D564E%02X%02X%02XF7 ", a, b, c);
    }
#endif
    return 1;
}

static void rm_flush(void)              /* what waits, while there is room */
{
    int e;
    if (rm.ack && rm_send(RM_STATE, (uint8_t)(rm.ack - 1u), 1))
        rm.ack = 0;
    while (rm.bq_r != rm.bq_w) {
        uint8_t v = rm.bq[rm.bq_r % RM_BQ], c = v & 0x7Fu;
        if (!(c >= RM_KEY0 ? rm_send(RM_KEY, (uint8_t)(c - RM_KEY0), v >> 7) : rm_send(RM_BTN, c, v >> 7)))
            return;
        rm.bq_r++;
    }
    for (e = 0; e < NE; e++)
        while (rm.enc[e]) {
            int d = rm.enc[e] > 63 ? 63 : rm.enc[e] < -64 ? -64 : rm.enc[e];
            if (!rm_send(RM_ENC, (uint8_t)e, (uint8_t)(d & 0x7F)))
                return;
            rm.enc[e] = (int16_t)(rm.enc[e] - d);
        }
}

static void rm_enter(void)
{
    int e;
    rm.on = 1;
    rm.link = plat_midi_out_link();
    rm.bq_r = rm.bq_w;
    rm.keys = plat_keys();               /* keys held from before are not the browser's */
    for (e = 0; e < NE; e++) {
        plat_enc(e);                     /* turns from before are not the browser's */
        rm.enc[e] = 0;
    }
    remote_mute = 1;
    rm.lit = rm.blink = 0;
    post(C_STOP, 0, 0);
    ui.redraw = 1;
    if (ann_on())
        ann_say("BROWSER. CONTROLLING THE WORKBENCH", 0, ANN_NOW);
}

static void rm_leave(void)
{
    rm.on = 0;
    rm.ack = 1;                          /* "off", if anyone still listens */
    rm.lit = rm.blink = 0;
    remote_mute = 0;                     /* stopped (C_STOP), so silent until PLAY continues the song */
    app_say("BROWSER", "DISCONNECTED");
}

int app_sysex(const uint8_t *p, uint32_t n)
{
    if (n < 4u || p[0] != RM_PFX0 || p[1] != RM_PFX1 || p[2] != RM_PFX2)
        return 0;
    if (p[3] == RM_ON) {
        rm.heard = plat_ms();
        if (!rm.on)
            rm_enter();
        rm.ack = 2;
    } else if (p[3] == RM_OFF && rm.on)
        rm_leave();
    else if (p[3] == RM_LIGHTS && n >= 12u && rm.on) {   /* 4 + 4 bytes of 7 bits: lit, then blinking */
        uint32_t l = 0, b = 0;
        int i;
        for (i = 3; i >= 0; i--) {
            l = l << 7 | (p[4 + i] & 0x7Fu);
            b = b << 7 | (p[8 + i] & 0x7Fu);
        }
        rm.lit = l & ((1u << NKEYS) - 1u);
        rm.blink = b & ((1u << NKEYS) - 1u);
    }
    return 1;
}

/* the controls to the browser, none acted on here; the remote let go when the browser is gone */
static void rm_input(uint32_t btn)
{
    uint32_t ch = btn ^ ui.btn, keys = plat_keys(), kch = keys ^ rm.keys;
    int b, e;
    for (b = 0; b < NB; b++)
        if ((ch >> b & 1u) && rm.bq_w - rm.bq_r < RM_BQ)
            rm.bq[rm.bq_w++ % RM_BQ] = (uint8_t)(b | (btn >> b & 1u) << 7);
    for (b = 0; b < NKEYS; b++)
        if ((kch >> b & 1u) && rm.bq_w - rm.bq_r < RM_BQ)
            rm.bq[rm.bq_w++ % RM_BQ] = (uint8_t)((RM_KEY0 + (uint32_t)b) | (keys >> b & 1u) << 7);
    rm.keys = keys;
    for (e = 0; e < NE; e++) {
        int32_t d = plat_enc(e), v = rm.enc[e] + d;
        rm.enc[e] = (int16_t)(v > 999 ? 999 : v < -999 ? -999 : v);
    }
    if (plat_ms() - rm.heard > RM_TIMEOUT || plat_midi_out_link() != rm.link)
        rm_leave();
}

static void input(void)
{
    uint32_t btn = plat_buttons();
    int32_t e;
    int k;
    if (rm.on)
        rm_input(btn);
    if (rm.on || rm.ack) {
        rm_flush();
        if (rm.on) {                         /* the browser's: the held buttons lit, the keys as it asks (03) */
            uint32_t ph = (plat_ms() / 125u) & 1u;
            ui.btn = btn;
            plat_glow(1);
            plat_leds(btn, (rm.lit & ~rm.blink) | (ph ? rm.blink : 0u));
            return;
        }
    }
    if ((btn >> B_PLAY & 1u) && !(ui.btn >> B_PLAY & 1u)) {
        post(C_PLAY, 0, 0);
        app_say(ui.shown.stopped ? "PLAY" : "STOP", "");
    }
    if ((btn >> B_NEW & 1u) && !(ui.btn >> B_NEW & 1u)) {
        post(C_NEW, 0, 0);
        app_say("NEW SONG", "");
    }
    for (k = 0; k < 4; k++) {               /* K1..K4: a request, by steps (the render adds them up) */
        int f = KNOB_FIELD[k];
        char t[12];
        if ((e = plat_enc(EN_K1 + k)) == 0)
            continue;
        post(C_REQ, f, (int)e * KNOB_STEP[k]);
        if (ui.shown.has_song) {            /* what it will be, as far as the main loop can tell */
            int v = ui.shown.req_set[f] ? ui.shown.req_val[f] : ui.shown.roll.v[f];
            value_text(f, field_step(f, v, (int)e * KNOB_STEP[k]), ui.shown.roll.v[RF_MODE], t);
            app_say(KNOB_NAME[k], t);
        }
    }
    if (btn != ui.btn)
        ui.redraw = 1;
    ui.btn = btn;
    plat_glow(1);                            /* the unlit buttons glow, readable in the dark */
    plat_leds(btn | (ui.shown.playing ? 1u << B_PLAY : 0u), 0);
}

void app_input_only(void)
{
    if (!seeded)
        boot_spins++;
    input();
}

static void draw(void)
{
    char t[48];
    const snap_t *s = &ui.shown;
    uint32_t now = plat_ms();
    int k;
    /* the header: the name, the key and the tempo, or a message for a moment */
    cv_begin(240, 32, C_BLACK);
    if (now < ui.msg_until) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], C_WHITE);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], C_HI);
    } else {
        cv_text(10, 6, &FONT_B, APP_NAME, C_WHITE);
        if ((s->has_song || s->ready) && !rm.on) {
            line_key(s, t);
            cv_text(230 - text_w(&FONT_XS, t), 10, &FONT_XS, t, C_HI);
        }
    }
    cv_blit(0, 0);
    /* the song: where it is, the chord, the change on its way */
    cv_begin(240, 120, C_BLACK);
    if (rm.on) {                          /* the browser has the controls */
        cv_text(10, 34, &FONT_B, "BROWSER", C_WHITE);
        cv_text(10, 74, &FONT_XS, "CONTROLLING THE WORKBENCH", C_GRAY);
    } else if (s->has_song) {
        line_place(s, t);
        cv_text(10, 4, &FONT_XS, t, C_GRAY);
        line_chord(s, t);
        cv_text(10, 34, &FONT_B, t, C_WHITE);
        line_pending(s, t);
        cv_text(10, 74, &FONT_XS, t, C_HI);
    } else {
        if (s->held) {                    /* where PLAY continues */
            line_place(s, t);
            cv_text(10, 4, &FONT_XS, t, C_GRAY);
        }
        cv_text(10, 34, &FONT_B, s->stopped ? "STOPPED" : "NEXT SONG", C_GRAY);
    }
    cv_blit(0, 40);
    /* the four knobs and their values */
    cv_begin(240, 40, C_BLACK);
    for (k = 0; k < 4 && !rm.on; k++) {
        int32_t x = 4 + k * 58;
        cv_text(x, 2, &FONT_XS, KNOB_NAME[k], C_GRAY);
        if (s->has_song) {
            value_text(KNOB_FIELD[k], s->roll.v[KNOB_FIELD[k]], s->roll.v[RF_MODE], t);
            cv_text(x, 18, &FONT_XS, t, C_HI);
        }
    }
    cv_blit(0, 190);
}

void app_ui_init(void)
{
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.redraw = 1;
}

void app_frame(void)
{
    snap_t s;
    uint32_t now = plat_ms();
    if (!seeded)
        seed_boot();
    roll_next();
    snap_read(&s);
    if (s.has_song != ui.shown.has_song || s.stopped != ui.shown.stopped || s.ready != ui.shown.ready
        || s.held != ui.shown.held || s.bar != ui.shown.bar
        || s.section != ui.shown.section || s.pend_field != ui.shown.pend_field || s.pend_bars != ui.shown.pend_bars
        || memcmp(&s.roll, &ui.shown.roll, sizeof s.roll) || memcmp(s.req_val, ui.shown.req_val, sizeof s.req_val))
        ui.redraw = 1;
    ui.shown = s;
    input();
    speak();
    if (ui.msg_until && now >= ui.msg_until) {
        ui.msg_until = 0;
        ui.redraw = 1;
    }
    if (ui.redraw) {
        ui.redraw = 0;
        draw();
    }
}

#ifdef PLAT_HOST
static int32_t rem_bar = -1;             /* "remember": the song shown then (its seed) and its bar */
static uint32_t rem_seed;
int app_expect(const char *what, int *got)
{
    snap_t s;
    snap_read(&s);
    if (!strcmp(what, "playing"))
        *got = s.playing;
    else if (!strcmp(what, "songs"))
        *got = s.songs;
    else if (!strcmp(what, "bar"))
        *got = s.has_song || s.held ? s.bar : -1;   /* (held: the bar PLAY continues from) */
    else if (!strcmp(what, "section"))
        *got = s.has_song ? s.section : -1;
    else if (!strcmp(what, "tempo"))
        *got = s.has_song ? s.roll.v[RF_TEMPO] : -1;
    else if (!strcmp(what, "key"))
        *got = s.has_song ? s.roll.v[RF_KEY] : -1;
    else if (!strcmp(what, "pending"))
        *got = s.pend_field >= 0 ? s.pend_bars : -1;
    else if (!strcmp(what, "voices"))
        *got = s.voices;
    else if (!strcmp(what, "remote"))
        *got = rm.on;
    else if (!strcmp(what, "same_song"))   /* the song playing or ready is the one remembered (1), another (0); none:
                                              -1 */
        *got = (s.has_song || s.ready) && rem_bar >= 0 ? s.roll.seed == rem_seed : -1;
    else if (!strcmp(what, "bars_on"))     /* the bar playing (or held), less the one remembered (the same song) */
        *got = (s.has_song || s.held) && rem_bar >= 0 && s.roll.seed == rem_seed ? s.bar - rem_bar : -99;
    else
        return 0;
    return 1;
}
#endif


#if defined(PLAT_HOST) && !defined(PLAT_WEB)
/* the simulator's commands for the browser's remote (tests/scenarios/remote.vnl):
 *   remote on | remote off | remote HEX   a SysEx message from the browser (HEX: the bytes between F0 and F7)
 *   remote_sent HEX...                    each message (F0 .. F7) was sent since the last remote_sent / remote_none
 *   remote_none                           nothing was sent since then
 * and for the transport (tests/scenarios/stop.vnl):
 *   remember                              the song playing and its bar, for "expect same_song" and "bars_on" */
static int rm_fails;
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d)
{
    (void)b, (void)c, (void)d;
    if (!strcmp(cmd, "remember")) {
        snap_t s;
        snap_read(&s);
        rem_bar = s.has_song ? s.bar : -1;
        rem_seed = s.roll.seed;
        printf("remember: %s\n", s.has_song ? "the song playing" : "no song playing");
        return 1;
    }
    if (!strcmp(cmd, "remote")) {
        uint8_t m[32] = {RM_PFX0, RM_PFX1, RM_PFX2, RM_ON};
        uint32_t n = 4;
        if (!strcmp(a, "off"))
            m[3] = RM_OFF;
        else if (strcmp(a, "on"))
            for (n = 0; a[2 * n] && a[2 * n + 1] && n < sizeof m; n++) {
                unsigned v;
                sscanf(a + 2 * n, "%2x", &v);
                m[n] = (uint8_t)v;
            }
        if (!app_sysex(m, n))
            printf("remote: %s not taken\n", a);
        return 1;
    }
    if (!strcmp(cmd, "remote_sent") || !strcmp(cmd, "remote_none")) {
        const char *w = plat_host_line + strlen(cmd);
        char one[64];
        int k, bad = 0;
        if (cmd[7] == 'n' && rm_sent[0])
            bad = 1;
        while (cmd[7] == 's' && sscanf(w, "%63s%n", one, &k) == 1) {
            if (!strstr(rm_sent, one))
                bad = 1;
            w += k;
        }
        printf("%s %s: sent %s\n", bad ? "FAIL" : "  ok", cmd, rm_sent[0] ? rm_sent : "nothing");
        rm_fails += bad;
        rm_sent[0] = 0;
        return 1;
    }
    return 0;
}
int app_host_fails(void) { return rm_fails; }
#endif

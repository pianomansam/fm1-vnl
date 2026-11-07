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
#include <time.h>
#endif
#include "song.h"
#include "harmony.h"
#include "override.h"
#include "theory.h"
#include "library.h"
#include "voices.h"
#include "fx.h"
#include "samples.h"
#include "wb_drums.h"
#include "snap.h"
/* The screens' runtime (app/ui: the workbench's screens as tables, their values, their drawing, the navigator): the
   device's UI with VNL_UI_RUNTIME (app.h; Task 8 turns it on), else today's hand-drawn screen below; the simulator
   has it either way (uiscreen: workbench/tools/parity.mjs) */
#if VNL_UI_RUNTIME || defined(PLAT_HOST)
#define UI_RT 1
#include "values.h"
#include "ui/choices_gen.c"
#include "ui/screens_gen.c"
#include "ui/values.c"
#include "ui/kitpick.c"
#include "ui/nav.c"
#include "render.h"
#include "ui/render.c"
#else
#define UI_RT 0
#endif

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

#if !VNL_UI_RUNTIME
/* each mode's keys spelled the simplest way: by the mode's parent major, the fewer accidentals (Db Lydian, not C#);
   on the F#/Gb tie, the tonic's simplest name (F# major (Sam, 2026-11-05), B Lydian; Ab Dorian, Bb Phrygian, Db
   Mixolydian, Eb minor). theory.c's spelling() spells the chords the same way. */
static const char *const KEY_NAME[MODE_N][12] = {
    {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"},    /* major (Ionian) */
    {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"},    /* Dorian */
    {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "Bb", "B"},    /* Phrygian */
    {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"},    /* Lydian */
    {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"},    /* Mixolydian */
    {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"}};   /* minor (Aeolian) */
static const char *const MODE_NAME[MODE_N] = {"MAJOR", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN", "MINOR"};
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
#endif

/* ------------------------------------------------------- main -> render --- */
/* the next song, rolled by the main loop: full until the render takes it */
static song_roll_t mb_roll;
static volatile uint8_t mb_full;

/* the button that rolls a new song (one constant: remap it here) */
#define B_NEW B_SEQ

/* commands, drained at a block's start (AMB-1's pattern) */
/* C_ROW a row's request (the UI runtime's: song_request_row), C_JUMP a section jump, C_MUTE / C_SOLO / C_LEVEL the
   listening and the mixer (the voices' state the render reads: set here, between blocks) */
enum { C_PLAY, C_REQ, C_STOP, C_NEW, C_ROW, C_JUMP, C_MUTE, C_SOLO, C_LEVEL };
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
static snap_t snap;                     /* (ui/snap.h) */
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

#if VNL_UI_RUNTIME
/* What it costs, readable on the device (SAVE shows it in the header): the longest redraw (a frame's drawing), the
   longest render of an audio block and the blocks whose render took longer than the block lasts (late) */
static uint32_t m_draw_max, m_render_max, m_late;
#ifdef PLAT_HOST
#define M_US() 0u
#else
#define M_US() fm1_micros()
#endif
#endif

#if UI_RT
static int16_t j_target;                 /* (below: the jumps and the roll's notes) */
static void roll_clear(void);
#endif

/* the drum lanes (app/drums, as the workbench's): a shipped kit's (song.kit_preset), or none (the song's own kit) */
static uint8_t drums_wait;               /* a shipped kit whose voices the main loop is still rendering */
static uint32_t kit_wait;                /* samples a song has waited for its kit's voices */
#define KIT_WAIT_MAX (44100u * 3u)       /* then it starts on the set's own kit until they are there */

/* the drum lanes: the shipped kit's (song.kit_preset), or none (the song's own kit); a modeled machine's whose voices
   aren't rendered yet: none (the set's own kit under it: Soft brushes) until they are (step) */
static void drums_apply(void)
{
    wb_drums_t d;
    int on = song.kit_preset >= 0 && wb_drums_preset(&d, song.kit_preset);
    drums_wait = (uint8_t)(on && !wb_drums_ready(&d));
    wb_drums_set(on && !drums_wait ? &d : 0);
    wb_drums_kit(lib_song_kit(&song.roll));
}

/* the next song's shipped kit (its seeded pick) is ready to play, or it has waited long enough */
static int kit_ready_next(uint32_t n)
{
#if UI_RT
    wb_drums_t d;
    int k = ui_kit_pick(&mb_roll, smp_ready());
    if (k < SONG_KIT_PRESET || !wb_drums_preset(&d, k - SONG_KIT_PRESET) || wb_drums_ready(&d) || kit_wait >= KIT_WAIT_MAX)
        return 1;
    kit_wait += n;
    return 0;
#else
    (void)n;
    return 1;
#endif
}

static void voices_begin(void)
{
    if (vx_sounding())                   /* a tail (notes, the plate, the bed's fade) rings on: keep it */
        vx_set_gain(1.0f);
    else
        vx_init();
    vx_song(&song.roll);                 /* this song's instruments, kit and chain (the bed fades in, the dust ramps
                                            if anything still sounds; the chain's noise from its seed), before its
                                            first event */
    wb_drums_song(song.roll.seed);
    drums_apply();
}

static void song_begin(void)
{
#if UI_RT
    song_start_kit(&song, &mb_roll, ui_kit_pick(&mb_roll, smp_ready()));   /* no retired kit (desk.js's approvedKit) */
#else
    song_start(&song, &mb_roll);
#endif
    BARRIER();
    mb_full = 0;                         /* the main loop rolls the next one */
    voices_begin();
    r_now = r_pos = r_tick = 0;
    r_frac = 0.0f;
    memset(req_set, 0, sizeof req_set);
#if UI_RT
    j_target = -1;
    roll_clear();
#endif
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

#if UI_RT
#ifdef PLAT_HOST
#define UI_POOL
#else
#define UI_POOL __attribute__((section(".pool")))
#endif
/* Section jumps (the Sections screen, Loop, Stay on song): the song started again and fast-forwarded silently, a few
   16ths a block (the workbench's landing, spread over the bar before), then put in at the next bar line with the
   changes still waiting carried over. Loop: the section playing, from its last bar, back to its start; Stay on song:
   the bar before the last section (or inside it, at a 4-bar line) back to the section after the intro (index.html's) */
static song_t jsong UI_POOL;
static int16_t j_target = -1, j_from;    /* the bar the song jumps to (-1: none), the bar it was asked in */
static uint8_t j_ready, loop_on, endless_on;
static song_ev_t j_evs[64];
enum { J_TICKS = 24 };                    /* 16ths fast-forwarded a block */

static void jump_ask(int bar)
{
    if (rs != RS_SONG || bar < 0 || bar >= song.roll.v[RF_BARS])
        return;
    song_start(&jsong, &song.roll);
    jsong.own = song.own;                /* the song's own overrides and set (song_request_row's) */
    jsong.ov = song.ov;
    jsong.set = song.set;
    jsong.set_base = song.set_base;
    jsong.changes = song.changes;
    j_target = (int16_t)bar;
    j_from = (int16_t)song.bar;
    j_ready = 0;
}

static void jump_prepare(void)
{
    int k;
    for (k = 0; k < J_TICKS && j_target >= 0 && !j_ready; k++) {
        if (jsong.bar >= j_target || jsong.state != SS_PLAYING)
            j_ready = 1;
        else
            song_tick(&jsong, j_evs, 64);
    }
    if (j_target >= 0 && jsong.bar >= j_target)
        j_ready = 1;
}

/* a carried request's bar in the song jumped to: the next 4-bar boundary there if its own has gone by */
static int16_t jump_rebar(int bar)
{
    int b = jsong.bar;
    return (int16_t)(bar >= b ? bar : b % 4 == 0 ? b : (b / 4 + 1) * 4);
}

static void jump_land(void)
{
    int f, i;
    song_carry_requests(&jsong, &song);
    for (f = 0; f < RF_N; f++)
        if (jsong.pend[f])
            jsong.pend_bar[f] = jump_rebar(jsong.pend_bar[f]);
    for (i = 0; i < jsong.rq_n; i++)
        jsong.rq[i].bar = jump_rebar(jsong.rq[i].bar);
    song = jsong;
    j_target = -1;
    vx_release_all();
    vx_configure(&song.roll);            /* (the bed reopens, as the workbench's landing) */
    drums_apply();
}

/* the bar line's own jumps (Loop, Stay on song), from the plan's sections; bar1 the bar beginning, from 1 */
static void jump_rules(void)
{
    int bar1 = song.bar + 1, i, n = song.plan.n;
    if (j_target >= 0)
        return;                          /* (a jump asked already wins) */
    if (loop_on) {
        for (i = 0; i < n; i++)
            if (bar1 > song.plan.b[i].start && bar1 <= song.plan.b[i].start + song.plan.b[i].bars) {
                if (bar1 == song.plan.b[i].start + song.plan.b[i].bars)
                    jump_ask(song.plan.b[i].start);
                return;
            }
    } else if (endless_on && n > 2) {
        int last = song.plan.b[n - 1].start;
        if (bar1 == last || (bar1 > last && (bar1 - last) % 4 == 0))
            jump_ask(song.plan.b[1].start);
    }
}

/* The Parts roll's notes: the song around the bar playing (the lanes' window), from the song itself. At each bar
   line a copy of the song (as it starts the bar) is ticked silently ahead to the window's end, a few 16ths a block,
   into the buffer the main loop doesn't read; the bars before keep the notes they had; then the buffers swap. */
enum { UI_ROLL_MAX = 1024, R_TICKS = 8 };
static struct { uint16_t n; ui_note_t note[UI_ROLL_MAX]; } roll_buf[2] UI_POOL;
static volatile uint8_t roll_front;
static song_t rsong UI_POOL;
static int16_t roll_to = -1;
static song_ev_t r_evs[64];

static int roll_part(int voice)
{
    return voice == V_CHORD ? UI_PART_KEYS : voice == V_BASS ? UI_PART_BASS : voice == V_LEAD ? UI_PART_LEAD
         : voice == V_PAD ? UI_PART_PAD : voice >= V_KICK && voice <= V_RIDE ? UI_PART_DRUMS : -1;
}

static void roll_bar_line(void)
{
    const ui_lanes_t *L = &UI_LANE_ITEMS[0];
    int bars = song.roll.v[RF_BARS], span = L->window < bars ? L->window : bars, from = song.bar - span / 4, i;
    uint8_t back = (uint8_t)!roll_front;
    if (from > bars - span) from = bars - span;
    if (from < 0) from = 0;
    roll_buf[back].n = 0;
    for (i = 0; i < roll_buf[roll_front].n; i++) {   /* the bars played keep their notes */
        const ui_note_t *e = &roll_buf[roll_front].note[i];
        if (e->bar >= from && e->bar < song.bar)
            roll_buf[back].note[roll_buf[back].n++] = *e;
    }
    rsong = song;
    roll_to = (int16_t)(from + span < bars ? from + span : bars);
}

static void roll_generate(void)
{
    uint8_t back = (uint8_t)!roll_front;
    int k, i;
    for (k = 0; k < R_TICKS && roll_to >= 0; k++) {
        int bar = rsong.bar, st = rsong.step, n;
        float ss;
        if (rsong.state != SS_PLAYING || bar >= roll_to) {
            BARRIER();
            roll_front = back;           /* done: the main loop reads it */
            roll_to = -1;
            break;
        }
        n = song_tick(&rsong, r_evs, 64);
        ss = song_step_samples(&rsong);
        for (i = 0; i < n && roll_buf[back].n < UI_ROLL_MAX; i++) {
            int p = roll_part(r_evs[i].voice), len = (int)((float)r_evs[i].len / ss + 0.5f);
            ui_note_t *e = &roll_buf[back].note[roll_buf[back].n];
            if (p < 0) continue;
            e->bar = (uint8_t)bar;
            e->step = (uint8_t)st;
            e->len = (uint8_t)(len > 255 ? 255 : len);
            e->note = (uint8_t)(r_evs[i].voice >= V_KICK ? r_evs[i].voice - V_KICK : r_evs[i].note);
            e->vel = r_evs[i].vel;
            e->part = (uint8_t)p;
            roll_buf[back].n++;
        }
    }
}

static void roll_clear(void)
{
    roll_to = -1;
    roll_buf[0].n = roll_buf[1].n = 0;
}
#endif

/* one 16th: the fade (if the song's last block fades), the section moves (each role bus's filters, plan_moves), the
   requests' slots, the step's events to the voices and the pad layer's level (song_pad_level) */
static void step(void)
{
    const plan_block_t *last;
    int bar, st = (int)(r_tick & 15u), n, i;
    float adv, lp[PM_N], hp[PM_N], llp;
    uint32_t ip;
    if (drums_wait)                      /* the shipped kit's voices rendered since: its lanes now */
        drums_apply();
#if UI_RT
    if (st == 0) {                       /* a bar line: a jump ready lands; the Loop's and Stay's; the roll's notes */
        if (j_target >= 0 && j_ready && song.bar > j_from)
            jump_land();
        jump_rules();
        roll_bar_line();
    }
#endif
    last = &song.plan.b[song.plan.n ? song.plan.n - 1 : 0];
    bar = song.bar;
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
        uint16_t changes = song.changes;  /* a row request (an instrument, the kit) landing: song_request_row */
        n = song_tick(&song, evs, 64);
        llp = song_lead_lp_at(&song, bar, st);   /* the lead loop's filter, for the 16th just played */
        vx_set_filter(PM_LEAD, lp[PM_LEAD] < llp ? llp : lp[PM_LEAD]);   /* before its events */
        vx_set_pad_level(song_pad_level(&song));   /* the pad layer's fade over a section line */
        /* sounding notes keep their patches; dust and bed changes ramp, the delay's time glides to a new tempo */
        if (vx_reconfigures(&before, &song.roll) || song.changes != changes) {
            vx_configure(&song.roll);
            drums_apply();
        }
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

#if UI_RT
/* rows.js's conflicts, on the song's requests: what a part's new instrument forces elsewhere, landing with it. A comp
   instrument that can't play the comp rhythm set: the rhythm back to as rolled; the felt piano, the piano and the
   vibes play the comp or the lead, never both: the other part takes another approved instrument (from the song's
   seed, as the workbench's dice would) */
static int one_part(int ins) { return ins == INS_KNIGHT || ins == INS_FELT_PIANO || ins == INS_VIBES; }

static int row_other(int row, int avoid)
{
    int i, n = ch_count(row), k = 0, pick;
    for (i = 0; i < n; i++) {
        int v = ch_value(row, i), f = ch_flags(row, v);
        if (v != avoid && (f & 15) == 0) k++;   /* approved, not hidden, not the engine's own choice */
    }
    if (!k) return avoid;
    pick = (int)(song.roll.seed % (uint32_t)k);
    for (i = 0; i < n; i++) {
        int v = ch_value(row, i), f = ch_flags(row, v);
        if (v != avoid && (f & 15) == 0 && !pick--) return v;
    }
    return avoid;
}

static void row_rules(int row, int v, const int16_t *rows)
{
    if (row == ROW_KEYS) {
        int comp = rows[ROW_COMP];
        if (comp >= 0 && ch_flags(ROW_COMP, comp) >= 0 && !ch_fits(ROW_COMP, comp, v)) {
            song_request_row(&song, SR_OV + OV_COMP, -1);
            song_request_row(&song, SR_OV + OV_COMP_PAT, -1);
        }
        if (one_part(v) && rows[ROW_LEAD] == v)
            song_request_row(&song, CH_SONG_ROW[ROW_LEAD], row_other(ROW_LEAD, v));
    } else if (row == ROW_LEAD && one_part(v) && rows[ROW_KEYS] == v)
        song_request_row(&song, CH_SONG_ROW[ROW_KEYS], row_other(ROW_KEYS, v));
}
#endif

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
#if UI_RT
    } else if (op == C_ROW && (a == ROW_LOOP || a == ROW_ENDLESS)) {   /* the switches: at once */
        if (a == ROW_LOOP)
            loop_on = (uint8_t)(b == 1);
        else
            endless_on = (uint8_t)(b == 1);
    } else if (op == C_JUMP) {
        jump_ask(b);
    } else if (op == C_MUTE) {
        vx_mute((unsigned)b);
    } else if (op == C_SOLO) {
        vx_solo(b);
    } else if (op == C_LEVEL) {
        vx_level_set(a, b);
    } else if (op == C_ROW && rs == RS_SONG && song.state == SS_PLAYING && a < ROW_N) {
        int16_t rows[ROW_N];             /* the rhythm row is the comp's or the strum patterns' */
        ui_song_rows(&song.roll, rows);
        if (song.kit_preset >= 0)
            rows[ROW_KIT] = (int16_t)(SONG_KIT_PRESET + song.kit_preset);
        if (a == ROW_RHYTHM)
            a = ch_rhythm_row(rows[ROW_KEYS], rows[ROW_COMP]);
        row_rules(a, b, rows);           /* what the new value forces elsewhere (rows.js's conflicts) */
        if (a == ROW_COMP) {             /* a comp rhythm (from 100): the COMP_PAT override; else COMP (desk.js) */
            song_request_row(&song, SR_OV + OV_COMP, b >= 100 ? -1 : b);
            song_request_row(&song, SR_OV + OV_COMP_PAT, b >= 100 ? b - 100 : -1);
        } else if (CH_SONG_ROW[a] >= 0)
            song_request_row(&song, CH_SONG_ROW[a], b);
#endif
#if !VNL_UI_RUNTIME
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
#endif
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
#if UI_RT
    if (snap.has_song || snap.ready) {   /* the screens' values: each row's, the plan's sections */
        const song_plan_t *p = snap.held ? &mark.plan : snap.has_song ? &song.plan : 0;
        int i;
        ui_song_rows(&snap.roll, snap.rows);
        if (snap.has_song || snap.held) {   /* the kit row: a shipped kit's value; the next song's, as it will pick */
            if (song.kit_preset >= 0)
                snap.rows[ROW_KIT] = (int16_t)(SONG_KIT_PRESET + song.kit_preset);
        } else if ((i = ui_kit_pick(&snap.roll, smp_ready())) >= 0) {
            snap.rows[ROW_KIT] = (int16_t)i;
        }
        snap.n_secs = 0;
        for (i = 0; p && i < p->n && i < SNAP_SECS; i++) {
            snap.secs[i].start = p->b[i].start;
            snap.secs[i].bars = p->b[i].bars;
            snap.secs[i].section = p->b[i].section;
        }
        snap.n_secs = (uint8_t)i;
        snap.changes = song.changes;
    }
    snap.rows[ROW_LOOP] = loop_on;
    snap.rows[ROW_ENDLESS] = endless_on;
#endif
    BARRIER();
    snap_seq++;
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i = 0, k;
#if VNL_UI_RUNTIME
    uint32_t m_t0 = M_US();
#endif
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
    if (rs == RS_WAIT && mb_full && kit_ready_next(n)) {
        kit_wait = 0;
        song_begin();
    }
#if UI_RT
    if (rs == RS_SONG) {                 /* the jump's fast-forward, the roll's look-ahead: a few 16ths a block */
        jump_prepare();
        roll_generate();
    }
#endif
    memset(mix_l, 0, sizeof(float) * n);
    memset(mix_r, 0, sizeof(float) * n);
    while (i < n) {
        uint32_t seg = n - i;
        if (rs == RS_SONG) {
            if (r_now >= r_pos) {        /* a step starts here (or, ended, the song's last sample is past) */
                if (song.state == SS_PLAYING)
                    step();
#if UI_RT
                else if (loop_on || endless_on) {   /* looping or staying: the song again, no next one */
                    jump_ask(0);
                    jump_prepare();
                    if (j_ready)
                        jump_land();
                    else
                        gap_begin();
                }
#endif
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
#if VNL_UI_RUNTIME
    {
        uint32_t dt = M_US() - m_t0;
        if (dt > m_render_max)
            m_render_max = dt;
        if (dt > n * 1000000u / 44100u)
            m_late++;
    }
#endif
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
#ifdef PLAT_HOST
    smp_mount(smp_blob, SMP_HEAD + smp_bytes(), 0, 0);   /* the sample bank: the host's own copy, checked as the FM-1's */
#else
    smp_mount(plat_data(APP_BANK_LO), APP_BANK_HI - APP_BANK_LO, plat_data(APP_BANK1_LO), APP_BANK1_HI - APP_BANK1_LO);
#endif
    lib_set_samples(smp_ready());        /* none (or another build's): synth instruments only, NO SAMPLES */
    vx_init();                           /* the first song: app_frame, once seeded */
    wb_drums_init();
}

#ifdef APP_USB_CMD
/* The sample bank from the host (tools/fm1_bank.py), in the update's framing: the host's request (BANK_CMD, the bank's
   length and CRC-32), then the FM-1 pulls it with read requests, as a restore does, and reports with a read of
   BANK_OK or BANK_FAIL + an error. Stopped only. Its head first, checked against the tables (another build's: refused,
   nothing changed); then the bank unmounted, the voices cleared, its two areas erased, the payload written (refused
   at an "AT" on an XDATA sector's 0xF06: app.h), the head last (cut short, there is no bank: never half of one), and
   the whole checked and mounted as at boot. */
#define BANK_CMD 0x60u
#define BANK_OK 0xC0000000u
#define BANK_FAIL 0xCF000000u
enum { BK_E_READ = 2, BK_E_HEAD = 3, BK_E_ERASE = 7, BK_E_WRITE = 8, BK_E_CHECK = 9, BK_E_PLAYING = 10, BK_E_MARK = 11 };

/* the bank file's byte off: its place in flash (the head and first part in one area, the rest in the other) */
static uint32_t bank_at(uint32_t off)
{
    return off < SMP_HEAD + smp_split() ? APP_BANK_LO + off : APP_BANK1_LO + (off - SMP_HEAD - smp_split());
}

static uint32_t bank_rx(uint32_t len)
{
    static uint8_t b[512];
    uint32_t off, k, t0;
    if (rs != RS_STOP)
        return BK_E_PLAYING;
    if (len != SMP_HEAD + smp_bytes() || SMP_HEAD + smp_split() > APP_BANK_HI - APP_BANK_LO ||
        smp_bytes() - smp_split() > APP_BANK1_HI - APP_BANK1_LO)
        return BK_E_HEAD;
    if (plat_host_read(0, b, SMP_HEAD))
        return BK_E_READ;
    if (!smp_head_ok(b))
        return BK_E_HEAD;
    smp_mount(0, 0, 0, 0);                     /* no new sampled note; the sounding ones cleared (the render's vx_init) */
    lib_set_samples(0);
    panic_now = 1;
    for (t0 = plat_ms(); panic_now && plat_ms() - t0 < 100u;)
        plat_yield();
    for (off = 0; off < SMP_HEAD + smp_split(); off += 0x1000u)
        if (plat_data_erase(APP_BANK_LO + off))
            return BK_E_ERASE;
    for (off = 0; off < smp_bytes() - smp_split(); off += 0x1000u)
        if (plat_data_erase(APP_BANK1_LO + off))
            return BK_E_ERASE;
    for (off = SMP_HEAD; off < len; off += k) {   /* (pieces 512-aligned in flash: a sector's 0xF06..0xF07 in one) */
        uint32_t j, a = bank_at(off), edge = off < SMP_HEAD + smp_split() ? SMP_HEAD + smp_split() : len;
        k = sizeof b - (a & (sizeof b - 1u));
        k = edge - off < k ? edge - off : k;
        if (plat_host_read(off, b, k))
            return BK_E_READ;
        for (j = 0; j + 1u < k; j++)
            if (((a + j) & 0xFFFu) == 0xF06u && (a + j < PLAT_XDATA0_HI || a + j >= PLAT_XDATA1_LO) &&
                b[j] == 'A' && b[j + 1u] == 'T')
                return BK_E_MARK;
        if (plat_data_write(a, b, k))
            return BK_E_WRITE;
    }
    if (plat_host_read(0, b, SMP_HEAD) || !smp_head_ok(b))
        return BK_E_READ;
    if (plat_data_write(APP_BANK_LO, b, SMP_HEAD))
        return BK_E_WRITE;
    return smp_mount(plat_data(APP_BANK_LO), APP_BANK_HI - APP_BANK_LO, plat_data(APP_BANK1_LO), APP_BANK1_HI - APP_BANK1_LO) ? BK_E_CHECK : 0u;
}

void app_usb_cmd(const uint8_t *d, uint32_t n)
{
    uint8_t ack[8];
    uint32_t rc;
    if (n != 15u || d[2] != BANK_CMD)
        return;
    rc = bank_rx(d[6] | (uint32_t)d[7] << 8 | (uint32_t)d[8] << 16 | (uint32_t)d[9] << 24);
    lib_set_samples(smp_ready());
    plat_host_read(rc ? BANK_FAIL | rc : BANK_OK, ack, sizeof ack);
    app_say(rc == BK_E_PLAYING ? "STOP FIRST" : "SAMPLES", !rc ? "WRITTEN" : rc == BK_E_PLAYING ? "then write" :
            smp_ready() ? "NOTHING CHANGED" : "NO SAMPLES");
}
#endif

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

#if UI_RT
static int ui_host;                      /* (below: the runtime's screens, and its navigator) */
static ui_nav_t ui_hnav;
#endif

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
#if UI_RT
    if (ui_host >= 0) {                  /* the runtime's header says it */
        memcpy(ui_hnav.msg, ui.msg, sizeof ui_hnav.msg);
        ui_hnav.msg_until = ui.msg_until;
    }
#endif
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

#if !VNL_UI_RUNTIME
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
static void line_key(const snap_t *s, char *t)    /* "F DORIAN  78 BPM" */
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
#endif

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

#if UI_RT
/* the UI runtime shows its screens and takes the controls: always with VNL_UI_RUNTIME, in the simulator once uiscreen
   asks (else -1) */
static int ui_host = VNL_UI_RUNTIME ? 0 : -1;
static void ui_host_input(uint32_t btn);
#endif

static void input(void)
{
    uint32_t btn = plat_buttons();
#if !VNL_UI_RUNTIME
    int32_t e;
    int k;
#endif
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
#if UI_RT
    if (ui_host >= 0) {
        ui_host_input(btn);
        return;
    }
#endif
#if !VNL_UI_RUNTIME
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
#endif
}

void app_input_only(void)
{
    if (!seeded)
        boot_spins++;
    input();
}

#if !VNL_UI_RUNTIME
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
        if (!smp_ready())
            cv_text(10, 74, &FONT_XS, "NO SAMPLES", C_HI);   /* no sample bank: synth instruments only */
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
#endif

#if UI_RT
/* The UI runtime's screens (app/ui): the device's UI with VNL_UI_RUNTIME, in the simulator over today's screen once
   uiscreen asks (uistate, uirow, uibench: workbench/tools/parity.mjs): its navigator, the snapshot it last drew from,
   rows forced (the simulator's) */
static ui_nav_t ui_hnav;
static snap_t ui_hsnap;
static int16_t ui_hrow[ROW_N];
static uint8_t ui_hrow_set[ROW_N];
static uint32_t ui_hkeys;                /* the keys down, as last read */
static unsigned ui_hmute;                /* the engine's listening: the parts muted (bit UI_PART_*), soloed */
static int ui_hsolo = -1;
static int8_t ui_hlevel[VX_PART_N];      /* the mixer's levels, dB (the render's, as last asked) */
static ui_activity_t ui_hact;

/* the mock's engine, as the simulator's app has it: a row's change to the render (C_ROW: the song's request), play,
   a new song, a part's re-roll (its salt drawn anew; the drums' pattern too), the listening and the mixer at once */
static const uint8_t UI_PART_BUS[] = {V_KICK, V_BASS, V_CHORD, V_PAD, V_LEAD, 0};
static void ui_e_set(int row, int v) { post(C_ROW, row, v); }
static void ui_e_play(void) { post(C_PLAY, 0, 0); }
static void ui_e_reroll(void) { post(C_NEW, 0, 0); }
static void ui_e_reroll_part(int p)
{
    static const int ROW[] = {ROW_DRUMS_VAR, ROW_BASS_LINE, ROW_VOICINGS, ROW_PAD_NOTES, ROW_PHRASE};
    if (p < 0 || p > UI_PART_LEAD) return;
    post(C_ROW, ROW[p], 1 + (int)(plat_ms() % 99u));
    if (p == UI_PART_DRUMS)              /* the drums' re-roll draws a new pattern too (desk.js) */
        post(C_ROW, ROW_PATTERN, ui_hsnap.rows[ROW_PATTERN] % LIB_NPATTERNS + 1);
}
/* the listening to the render (C_MUTE: vx_mute's bits; a solo mutes every other bus, the mutes set aside while it
   lasts, as wb.c's listen()): the voices' state is the render's, so it changes between blocks, never during one */
static void ui_e_listen(void)
{
    unsigned m = 0;
    int p;
    for (p = 0; p < UI_PART_TEXTURE; p++)
        if (ui_hsolo >= 0 ? p != ui_hsolo : (ui_hmute >> p & 1u)) m |= 1u << UI_PART_BUS[p];
    post(C_MUTE, 0, (int)m);
}
static void ui_e_mute(int p, int on) { ui_hmute = on ? ui_hmute | 1u << p : ui_hmute & ~(1u << p); ui_e_listen(); }
static void ui_e_solo(int p) { ui_hsolo = p; ui_e_listen(); }
static int ui_e_muted(int p) { return (int)(ui_hmute >> p & 1u); }
static int ui_e_soloed(void) { return ui_hsolo; }
static void ui_e_level(int p, int db)
{
    if (p < 0 || p >= VX_PART_N) return;
    ui_hlevel[p] = (int8_t)db;
    post(C_LEVEL, p, db);
}
static int ui_e_level_get(int p) { return p >= 0 && p < VX_PART_N ? ui_hlevel[p] : 0; }
static void ui_e_jump(int start, int i) { (void)i; post(C_JUMP, 0, start); }
static const ui_engine_t UI_HENG = {ui_e_set, ui_e_play, ui_e_reroll, ui_e_reroll_part, ui_e_jump, ui_e_mute,
                                    ui_e_solo, ui_e_muted, ui_e_soloed, ui_e_level, ui_e_level_get};

/* what the Parts roll and the Mixer show: the render's notes (the buffer it isn't writing), the listening, levels */
static const ui_activity_t *ui_activity(void)
{
    int p;
    uint8_t f = roll_front;
    ui_hact.notes = roll_buf[f].note;
    ui_hact.n = ui.shown.has_song ? roll_buf[f].n : 0;
    ui_hact.mute = ui_hmute;
    ui_hact.solo = ui_hsolo;
    for (p = 0; p < VX_PART_N && p < (int)sizeof ui_hact.level; p++)
        ui_hact.level[p] = ui_hlevel[p];
    return &ui_hact;
}

/* the snapshot the runtime sees: the render's, rows forced (uirow), a ready song's sections */
static void ui_host_snap(void)
{
    int i;
    if (ui.shown.roll.seed != ui_hsnap.roll.seed) {   /* a new song: no mutes, no solo (desk.js); the render sets the
                                                        levels back to 0 dB itself (song_begin) */
        if (ui_hsolo + (int)ui_hmute != -1) {
            ui_hmute = 0;
            ui_hsolo = -1;
            ui_e_listen();
        }
        memset(ui_hlevel, 0, sizeof ui_hlevel);
    }
    ui_hsnap = ui.shown;
    for (i = 0; i < ROW_N; i++)
        if (ui_hrow_set[i])
            ui_hsnap.rows[i] = ui_hrow[i];
    ui_snap_sections(&ui_hsnap);
    ui_update(&ui_hnav, &ui_hsnap, plat_ms());
}

static void ui_host_draw(void)
{
    static int last = -1;
    int id;
    ui_bag_t b;
    ui_host_snap();
    id = ui_hnav.stack[ui_hnav.depth - 1];
    if (id != last)                      /* another screen: what its bands leave uncovered, cleared */
        lcd_fill(0, 0, 240, 240, C_BLACK);
    last = id;
    ui_values(&ui_hsnap, &ui_hnav, &b);
    b.act = ui_activity();
    ui_draw(&UI_SCREENS[id], &b, 0xFFFFFFFFu);
}

/* the controls to the navigator: a button's press, an encoder's detents, a key down or up; the lights */
static void ui_host_input(uint32_t btn)
{
    uint32_t keys = plat_keys(), now = plat_ms(), blink, on;
    int b, e, k;
    ui_host_snap();
    for (b = 0; b < NB; b++)
        if ((btn >> b & 1u) && !(ui.btn >> b & 1u))
            ui_press(&ui_hnav, UI_CTL_BUTTON(b), 1);
#if VNL_UI_RUNTIME
    if ((btn >> B_SAVE & 1u) && !(ui.btn >> B_SAVE & 1u)) {   /* SAVE: the measures (no screen maps it) */
        char a[UI_MSG], c[UI_MSG], v[12];
        a[0] = c[0] = 0;
        text_cat(a, "REDRAW ");
        int_text((int32_t)m_draw_max, v);
        text_cat(a, v);
        text_cat(a, " US");
        text_cat(c, "LATE ");
        int_text((int32_t)m_late, v);
        text_cat(c, v);
        text_cat(c, " MAX ");
        int_text((int32_t)m_render_max, v);
        text_cat(c, v);
        app_say(a, c);
    }
#endif
    for (e = 0; e < NE; e++) {
        int32_t d = plat_enc(e);
        if (d) {
            ui_press(&ui_hnav, UI_CTL_ENCODER(e), (int)d);
            ui.redraw = 1;
        }
    }
    for (k = 0; k < UI_KEYS; k++)
        if ((keys ^ ui_hkeys) >> k & 1u)
            ui_key(&ui_hnav, k, (int)(keys >> k & 1u), now);
    if (keys != ui_hkeys || btn != ui.btn)   /* (a key or a button: the screen may change) */
        ui.redraw = 1;
    ui_hkeys = keys;
    if (keys)                            /* a key held: its long press may act */
        ui.redraw = 1;
    ui_tick(&ui_hnav, now);
    if (ui_hnav.msg_until && now >= ui_hnav.msg_until) {   /* the header's message gone */
        ui_hnav.msg_until = 0;
        ui.redraw = 1;
    }
    ui.btn = btn;
#ifdef PLAT_HOST
    ui.redraw = 1;                       /* (the simulator: every frame) */
#endif
    on = ui_key_lights(&ui_hnav, &ui_hsnap, &blink);
    plat_glow(1);
    plat_leds(btn | (ui.shown.playing ? 1u << B_PLAY : 0u), on | ((now / 250u) & 1u ? blink : 0u));
}

#if defined(PLAT_HOST) && !defined(PLAT_WEB)
/* a JSON string */
static void ui_json_str(const char *s)
{
    putchar('"');
    for (; *s; s++)
        if (*s == '"' || *s == '\\')
            printf("\\%c", *s);
        else if ((unsigned char)*s < 32)
            printf("\\u%04x", *s);
        else
            putchar(*s);
    putchar('"');
}

/* uistate: the bag the screen was last drawn from, as the mock's (name: text, true/false or a number), and the stack */
static void ui_host_state(void)
{
    ui_bag_t b;
    char t[UI_VALUE_MAX];
    int i, j, first = 1;
    ui_values(&ui_hsnap, &ui_hnav, &b);
    printf("uistate {\"said\":");
    ui_json_str(ui_hnav.said);
    printf(",\"msg\":");
    ui_json_str(ui_hnav.msg_until && ui_hnav.now < ui_hnav.msg_until ? ui_hnav.msg[0] : "");
    printf(",\"screen\":");
    ui_json_str(UI_SCREEN_NAME[b.screen]);
    printf(",\"stack\":[");
    for (i = 0; i < ui_hnav.depth; i++) {
        printf("%s", i ? "," : "");
        ui_json_str(UI_SCREEN_NAME[ui_hnav.stack[i]]);
    }
    printf("],\"values\":{");
    for (i = 1; i < UI_N_VALUES; i++) {
        const char *n = UI_VALUE_STR(i);
        for (j = 1; j < i && strcmp(UI_VALUE_STR(j), n); j++) {}
        if (j < i)
            continue;                    /* a name in two families: the first's */
        printf("%s", first ? "" : ",");
        first = 0;
        ui_json_str(n);
        putchar(':');
        if (ui_value_kind(i) == UI_VK_BOOL)
            printf("%s", ui_true(&b, i) ? "true" : "false");
        else if (ui_value_kind(i) == UI_VK_NUM) {
            int den, num = ui_value_num(&b, i, &den);
            printf("%.17g", (double)num / den);
        } else
            ui_json_str(ui_value(&b, i, 0, t));
    }
    printf(",\"secList\":[");
    for (i = 0; i < ui_hsnap.n_secs; i++) {
        printf("%s{\"start\":%d,\"bars\":%d,\"name\":", i ? "," : "", ui_hsnap.secs[i].start, ui_hsnap.secs[i].bars);
        ui_json_str(plan_section_name(ui_hsnap.secs[i].section));
        putchar('}');
    }
    {                                    /* the Parts roll's and the Mixer's: the mock's activity and levels */
        static const char *const PART[] = {"drums", "bass", "keys", "pad", "lead", "texture"};
        const ui_activity_t *act = ui_activity();
        int p, k, any;
        printf("],\"activity\":{\"bars\":%d,\"sections\":[", ui_hsnap.roll.v[RF_BARS]);
        for (i = 0; i < ui_hsnap.n_secs; i++)
            printf("%s%d", i ? "," : "", ui_hsnap.secs[i].start);
        printf("],\"mute\":[");
        for (p = 0, any = 0; p < UI_PART_TEXTURE; p++)
            if (act->mute >> p & 1u) printf("%s\"%s\"", any++ ? "," : "", PART[p]);
        printf("],\"solo\":");
        if (act->solo >= 0) printf("\"%s\"", PART[act->solo]); else printf("null");
        printf(",\"notes\":{");
        for (p = 0; p < UI_PART_TEXTURE; p++) {
            int lo = 127, hi = 0, span;
            for (k = 0; k < act->n; k++)
                if (act->notes[k].part == p) {
                    if (act->notes[k].note < lo) lo = act->notes[k].note;
                    if (act->notes[k].note > hi) hi = act->notes[k].note;
                }
            span = (hi - lo > 11 ? hi - lo : 11) + 1;
            printf("%s\"%s\":[", p ? "," : "", PART[p]);
            for (k = 0, any = 0; k < act->n; k++) {
                const ui_note_t *e = &act->notes[k];
                int rows = V_RIDE - V_KICK + 1;
                if (e->part != p) continue;
                printf("%s[%.17g,%.17g,%.17g,%.17g,%.17g]", any++ ? "," : "", e->bar + e->step / 16.0,
                       (e->len ? e->len : 1) / 16.0, p == UI_PART_DRUMS ? (double)e->note / rows : (double)(hi - e->note) / span,
                       p == UI_PART_DRUMS ? 1.0 / rows : 1.0 / span, e->vel / 127.0);
            }
            printf("]");
        }
        printf("}},\"levels\":{");
        for (p = 0; p < VX_PART_N; p++)
            printf("%s\"%s\":%d", p ? "," : "", PART[p], act->level[p]);
        printf("},\"barPos\":%d}}\n", (int)ui_hsnap.bar);
    }
}
#endif
#endif

#if VNL_UI_RUNTIME
/* the screen: the runtime's, or while the browser has the controls, that */
static void draw(void)
{
    if (rm.on) {
        lcd_fill(0, 0, 240, 240, C_BLACK);
        cv_begin(240, 120, C_BLACK);
        cv_text(10, 34, &FONT_B, "BROWSER", C_WHITE);
        cv_text(10, 74, &FONT_XS, "CONTROLLING THE WORKBENCH", C_GRAY);
        cv_blit(0, 40);
        return;
    }
    ui_host_draw();
}

/* a screen reader that has just started listening (or asks) hears the whole screen; what the navigator said (a
   screen opened, a change asked) it hears as it is said */
static void speak(void)
{
    static char last[UI_SAID];
    if (ann_on()) {
        if (ann_where()) {
            ui_speak(&ui_hnav);
            ann_say(rm.on ? APP_NAME ". BROWSER. CONTROLLING THE WORKBENCH" : ui_hnav.said, 0, ANN_NOW);
        } else {
            int i;
            for (i = 0; i < UI_SAID && last[i] == ui_hnav.said[i] && last[i]; i++) {}
            if (i < UI_SAID && last[i] != ui_hnav.said[i])
                ann_say(ui_hnav.said, 0, ANN_NOW);
        }
    }
    memcpy(last, ui_hnav.said, sizeof last);
    ann_tick(plat_ms());
}
#endif

void app_ui_init(void)
{
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.redraw = 1;
#if VNL_UI_RUNTIME
    ui_nav_init(&ui_hnav);               /* the main screen (the runtime's) */
    ui_hnav.eng = &UI_HENG;
#endif
    if (!smp_ready())
        app_say("NO SAMPLES", "SYNTHS ONLY");   /* no sample bank (tools/fm1_bank.py writes it) */
}

/* main loop: the modeled machine kit's voices (wb_drums_want, wb_drums_work) the song playing needs, else the next
   one, a slice at a time (never in the audio interrupt): ready before the next song starts */
#define KIT_SLICE 2048u                  /* frames a main-loop pass (the host: ~25 us; the FM-1: a few ms) */
static void kit_render(const snap_t *s)
{
    wb_drums_t d;
    int k = (s->has_song || s->held) ? s->rows[ROW_KIT] : -1;
    if (!(k >= SONG_KIT_PRESET && wb_drums_preset(&d, k - SONG_KIT_PRESET) && !wb_drums_ready(&d))) {
        int nx = mb_full ? ui_kit_pick(&mb_roll, smp_ready()) : -1;
        if (!(nx >= SONG_KIT_PRESET && wb_drums_preset(&d, nx - SONG_KIT_PRESET)))
            if (!(k >= SONG_KIT_PRESET && wb_drums_preset(&d, k - SONG_KIT_PRESET)))
                return;
    }
    wb_drums_want(&d);
    wb_drums_work(KIT_SLICE, 0);
}

void app_frame(void)
{
    snap_t s;
    uint32_t now = plat_ms();
    if (!seeded)
        seed_boot();
    roll_next();
    snap_read(&s);
#if UI_RT
    kit_render(&s);
#endif
    if (s.has_song != ui.shown.has_song || s.stopped != ui.shown.stopped || s.ready != ui.shown.ready
        || s.held != ui.shown.held || s.bar != ui.shown.bar
        || s.section != ui.shown.section || s.pend_field != ui.shown.pend_field || s.pend_bars != ui.shown.pend_bars
        || memcmp(&s.roll, &ui.shown.roll, sizeof s.roll) || memcmp(s.req_val, ui.shown.req_val, sizeof s.req_val))
        ui.redraw = 1;
#if VNL_UI_RUNTIME
    {                                    /* no sample bank: said at boot and as each song starts */
        static int32_t said_songs = -1;
        if (!smp_ready() && s.songs != said_songs) {
            said_songs = s.songs;
            app_say("NO SAMPLES", "SYNTHS ONLY");
        }
    }
#endif
    ui.shown = s;
    input();
    speak();
    if (ui.msg_until && now >= ui.msg_until) {
        ui.msg_until = 0;
        ui.redraw = 1;
    }
    if (ui.redraw) {
        ui.redraw = 0;
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
        if (ui_host >= 0) {
#if VNL_UI_RUNTIME
            uint32_t m_t0 = M_US(), dt;
            ui_host_draw();
            dt = M_US() - m_t0;
            if (dt > m_draw_max)
                m_draw_max = dt;
#else
            ui_host_draw();
#endif
        } else
#endif
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
    else if (!strcmp(what, "kit_lanes"))   /* the song's shipped kit: 1 its lanes play, 0 its voices still rendering
                                              (the set's own kit meanwhile); -1 none */
        *got = rs == RS_SONG && song.kit_preset >= 0 ? !drums_wait : -1;
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
    else if (!strcmp(what, "pending")) {
        *got = s.pend_field >= 0 ? s.pend_bars : -1;
#if VNL_UI_RUNTIME
        {                                /* the navigator's: a change waits there until the bar before it lands */
            ui_bag_t bag;
            ui_values(&s, &ui_hnav, &bag);
            if (bag.pend_field >= 0 && (*got < 0 || bag.pend_bars < *got))
                *got = bag.pend_bars;
        }
#endif
    }
#if VNL_UI_RUNTIME
    else if (!strcmp(what, "home"))     /* the main screen shown (1) or another (0) */
        *got = ui_hnav.stack[ui_hnav.depth - 1] == UI_HOME;
    else if (!strcmp(what, "key_lit")) {   /* the keys lit, bit k key k (blinking ones not counted) */
        uint32_t blink;
        *got = (int)ui_key_lights(&ui_hnav, &s, &blink);
    }
#endif
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
    (void)c, (void)d;
    if (!strcmp(cmd, "uiscreen")) {     /* uiscreen ID: the UI runtime shows that screen (workbench/ui/screens.js's id) */
        int i;
        for (i = 0; i < UI_N_SCREENS && strcmp(UI_SCREEN_NAME[i], a); i++) {}
        if (i == UI_N_SCREENS) {
            printf("uiscreen: no screen %s\n", a);
            rm_fails++;
            return 1;
        }
        ui_host = i;
        ui_nav_init(&ui_hnav);
        ui_hnav.eng = &UI_HENG;
        ui_hnav.stack[0] = (uint8_t)i;
        ui_hkeys = plat_keys();
        lcd_fill(0, 0, 240, 240, C_BLACK);
        ui_host_draw();
        return 1;
    }
    if (!strcmp(cmd, "uibench")) {       /* uibench: each screen's full draw (values and bands), the worst in us */
        int i, k, keep = ui_hnav.stack[ui_hnav.depth - 1], worst = 0, at = 0;
        if (ui_host < 0) {
            printf("uibench: uiscreen first\n");
            return 1;
        }
        for (i = 0; i < UI_N_SCREENS; i++) {
            clock_t t0;
            int us;
            ui_hnav.stack[ui_hnav.depth - 1] = (uint8_t)i;
            t0 = clock();
            for (k = 0; k < 50; k++)
                ui_host_draw();
            us = (int)((clock() - t0) * 1000000.0 / CLOCKS_PER_SEC / 50);
            printf("uibench %s %d us\n", UI_SCREEN_NAME[i], us);
            if (us > worst)
                worst = us, at = i;
        }
        printf("uibench worst %s %d us (the host's)\n", UI_SCREEN_NAME[at], worst);
        ui_hnav.stack[ui_hnav.depth - 1] = (uint8_t)keep;
        return 1;
    }
    if (!strcmp(cmd, "uistate")) {       /* uistate: the bag and the stack the runtime's screen was drawn from, JSON */
        if (ui_host < 0)
            printf("uistate null\n");
        else
            ui_host_state();
        return 1;
    }
    if (!strcmp(cmd, "uirow")) {         /* uirow ROW VALUE: a row's value forced in what the runtime shows */
        int r;
        for (r = 0; r < ROW_N && strcmp(UI_VALUE_STR(UV_ROW(r, UVK_ROW_VALUE)), a); r++) {}
        if (r == ROW_N) {
            printf("uirow: no row %s\n", a);
            rm_fails++;
            return 1;
        }
        ui_hrow[r] = (int16_t)atoi(b);
        ui_hrow_set[r] = 1;
        if (ui_host >= 0)
            ui_host_draw();
        return 1;
    }
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

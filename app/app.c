/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal (fm1-base's template, which this started from)
 * Copyright (C) 2026 pianomansam (VNL-1's changes) */
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
#include "patterns_gen.h"
#include "voices.h"
#include "fx.h"
#include "samples.h"
#include "wb_drums.h"
#include "cpuguard.h"
#include "prof.h"
#include "snap.h"
/* The screens' runtime (app/ui: the workbench's screens as tables, their values, their drawing, the navigator): the
   device's UI with VNL_UI_RUNTIME (app.h; Task 8 turns it on), else today's hand-drawn screen below; the simulator
   has it either way (uiscreen: workbench/tools/parity.mjs) */
#if VNL_UI_RUNTIME || defined(PLAT_HOST)
#define UI_RT 1
#undef UA_JUMP                  /* felucca-bsp's usb.c, earlier in the unity, has a macro of that name; ours: an enum */
#include "values.h"
#include "ui/choices_gen.c"
#include "ui/screens_gen.c"
#include "ui/values.c"
#include "ui/kitpick.c"
#include "ui/nav.c"
#include "ui/saved.c"                    /* the saved songs: their records and slots (OBJ_SONGS) */
#include "render.h"
#include "ui/font_mark.h"                /* the splash's wordmark face (FONT_MARK: workbench/tools/export_font.py) */
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
static volatile uint8_t mb_gen;          /* +1 each time the main loop fills it (song_prepare: is it still the one) */
#if UI_RT
/* and what the locks keep in it (written with mb_roll, while the mailbox is empty): its kit row (-1: the seeded
   pick), the rows song_start_rows sets from its start (SR_ROLE, SR_OV), and the locked rows as the screens show them
   (ROW_*: the song ready shows them before it plays) */
enum { LK_MAX = 28 };
typedef struct {
    int16_t kit;
    uint8_t n, nshow;
    int16_t row[LK_MAX], val[LK_MAX];
    int8_t show[LK_MAX];
    int16_t show_val[LK_MAX];
} lk_box_t;
static lk_box_t mb_lk = {-1, 0, 0, {0}, {0}, {0}, {0}};
#endif

/* the button that rolls a new song (one constant: remap it here) */
#define B_NEW B_SEQ

/* commands, drained at a block's start (AMB-1's pattern) */
/* C_ROW a row's request (the UI runtime's: song_request_row), C_JUMP a section jump, C_MUTE / C_SOLO / C_LEVEL the
   listening and the mixer (the voices' state the render reads: set here, between blocks) */
enum { C_PLAY, C_REQ, C_STOP, C_NEW, C_ROW, C_JUMP, C_MUTE, C_SOLO, C_LEVEL, C_AB };
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
static uint32_t m_spin_max;              /* the record's turn: its label's redraw, the longest (us) */
static uint32_t m_fade_max;              /* a fade's step: the text's columns' redraw, the longest (us) */
static uint32_t m_load_now, m_load_peak; /* the render's load, % of its block: the last block's, the highest */
/* the CPU meter's main-loop side (prof.h has the render's): the redraws' and the kit voices' time, less the render
   that interrupted them, in us since the reset (m_prof_ms: plat_ms then) */
static uint32_t m_ui_us, m_kit_us, m_prof_ms;
#define M_OWN(t0, r0) (M_DT(t0) - (pf_render_us() - (r0)))   /* a main-loop span's own time (us) */
/* what a late block had going on (its first cause in this order counts it, one each; none: other), for the live state: the render's own
   (a new song, its first bar's planning or its end, a stop's resume; a row's request landing; a jump's fast-forward
   or landing; the Parts roll's look-ahead; the downbeat; a piece of the next bar's planning (song_plans_at); a
   command: a mute, a solo, a level, a jump, the A/B
   switches' plate and bed; the guard stepping; a section's filters or the fade ramping; a 16th ticked; notes
   starting) and the main loop's around it (a redraw, the kit's voices rendering, a flash write, a SysEx: the live
   state's answer; the next song rolled); else the voices and effects sounding, nothing else going on */
enum { MC_SONG, MC_ROW, MC_JUMP, MC_ROLL, MC_BAR, MC_PLAN, MC_CMD, MC_GUARD, MC_RAMP, MC_STEP, MC_NOTES, MC_DRAW,
       MC_KIT, MC_FLASH, MC_USB, MC_NEXT, MC_VOICES, MC_OTHER, MC_N };   /* (MC_BAR: the song generated its next bar in
                                            the block) */
static uint32_t m_late_by[MC_N];
/* the last late blocks (a ring, M_LATE_AT of them, m_late_n since the reset), each the 16th the block ticked (its bar
   and step; step 255: none, the bar the song is in), the piece of the next bar's planning that tick made
   (song_plans_at, 0 none), the block's render (us) and its causes (MC_* bits): the live state's late_at */
enum { M_LATE_AT = 4 };
static struct {
    uint16_t bar, us;
    uint8_t step, piece;
    uint32_t causes;
} m_late_at[M_LATE_AT];
static uint32_t m_late_n;
static uint16_t m_tick_bar = 0xFFFFu;    /* this block's 16th (step: its bar, step and piece; none yet: 0xFFFF) */
static uint8_t m_tick_step, m_tick_piece;
static uint32_t m_shed0;                 /* vx_shed at the last reset: the voices the guard cut since then */
static volatile uint32_t m_over90;       /* blocks the platform found over 90 % of their time (APP_OVERLOAD: its whole
                                            render, USB audio's included; a cross-check of the render's own late) */
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
static uint32_t heavy_us;                /* the simulator's "heavy N": N us of busy work a sounding voice, each block,
                                            the guard on (the FM-1's settings): an overload for the guard's test */
#endif
static uint32_t m_cause;                 /* the render's, this block */
static volatile uint32_t m_cause_main;   /* the main loop's, while it lasts */
static volatile uint32_t m_cause_seen;   /* the main loop's brief ones, since the last block (the render clears it) */
#define M_SEEN(mc) (m_cause_seen |= 1u << (mc))
#ifdef PLAT_HOST
#include <time.h>
static uint32_t m_us_host(void)          /* (the simulator: the host's clock, for the live state's figures) */
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)((uint64_t)t.tv_sec * 1000000u + (uint64_t)t.tv_nsec / 1000u);
}
#define M_T() m_us_host()
#define M_DT(t0) (m_us_host() - (t0))
#else
/* (TIMER4's ticks: their difference, unsigned, is right across the counter's wrap; fm1_micros() wraps at 178.9 s of
   microseconds, not at 2^32, so a difference of two of those across it read as minus 178 953 402 us) */
#define M_T() fm1_ticks()
#define M_DT(t0) ((uint32_t)(fm1_ticks() - (t0)) / FM1_TICKS_PER_US)
#endif
#define M_SANE_US 1000000u               /* a block or a redraw measured longer than this: not a reading (dropped) */
#endif

#if UI_RT
static int16_t j_target;                 /* (below: the jumps and the roll's notes) */
static void roll_clear(void);
#endif

/* the drum lanes (app/drums, as the workbench's): a shipped kit's (song.kit_preset), or none (the song's own kit) */
static uint8_t drums_wait;               /* a shipped kit whose voices the main loop is still rendering */
/* A/B switches for finding a drop-out on the device without a reflash (the live state's 04 02 ff; 0, the default:
   all as built): AB_GUARD the CPU guard off, AB_ROLL the Parts roll's look-ahead off, AB_KIT the main loop's kit
   voice render paused, AB_LANES the shipped kits' lanes off (the set's own kit under them plays) */
enum { AB_GUARD = 1, AB_ROLL = 2, AB_KIT = 4, AB_LANES = 8, AB_CHODLY = 16, AB_PLATE = 32, AB_FULLRATE = 64 };
/* and a second byte (04 02 ff gg), the mix's stages one at a time, for a sound the device makes and the workbench
   doesn't: AB2_BED the bed off, AB2_FILTERS the buses' filters off, AB2_COMP the drums' compressor off, AB2_GLUE the
   glue off, AB2_MUD the mud high-pass off, AB2_MONO the output summed to mono (a channel out of phase cancels) */
enum { AB2_BED = 1, AB2_FILTERS = 2, AB2_COMP = 4, AB2_GLUE = 8, AB2_MUD = 16, AB2_MONO = 64 };
static volatile uint8_t ab_flags, ab_flags2;
/* A new song whose shipped kit's voices the main loop is still rendering starts on time anyway, its comp and the
   texture alone (Sam, 2026-11-08: a short comp-only intro, not silence): the drums, the bass, the lead and the pad
   held out of the mix (vx_mute, under the listener's own mutes) until the first bar line after the voices are there,
   or after KIT_WAIT_MAX (then on the set's own kit until they are). The song itself plays as ever, only unheard. */
#if UI_RT
static uint32_t kit_wait;                /* samples the song has held them */
#define KIT_WAIT_MAX (44100u * 3u)
#define HOLD_BUSES (1u << V_KICK | 1u << V_BASS | 1u << V_LEAD | 1u << V_PAD)
static uint8_t kit_hold;
static unsigned mute_user;               /* the listener's mutes (C_MUTE) */
static void mutes_apply(void) { vx_mute(mute_user | (kit_hold ? HOLD_BUSES : 0u)); }
#endif

/* the drum lanes: the shipped kit's (song.kit_preset), or none (the song's own kit); a modeled machine's whose voices
   aren't rendered yet: none (the set's own kit under it: Soft brushes) until they are (step) */
static void drums_apply(void)
{
    wb_drums_t d;
    int on = song.kit_preset >= 0 && !(ab_flags & AB_LANES) && wb_drums_preset(&d, song.kit_preset);
    drums_wait = (uint8_t)(on && !wb_drums_ready(&d));
    wb_drums_set(on && !drums_wait ? &d : 0);
    wb_drums_kit(lib_song_kit(&song.roll));
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

#if UI_RT
static int kit_override = -1;            /* VNL_KIT (host): every song's kit row value (bench.c's), else the pick */
/* a song's kit: the seeded pick (ui_kit_pick: a retired rolled kit swapped), or VNL_KIT's on the host */
static int kit_pick(const song_roll_t *r)
{
    return kit_override >= 0 ? kit_override : ui_kit_pick(r, smp_ready());
}
#endif

static uint8_t first_part = SONG_FIRST_BAR_PARTS;   /* the new song's first bar's planning: the next part (a block
                                            each, after its start's) */

#if UI_RT
#ifdef PLAT_HOST
#define UI_POOL
#else
#define UI_POOL __attribute__((section(".pool")))   /* (the big zeroed buffers' RAM, not the app's own) */
#endif
/* The next song started early, in the gap (song_prepare): `song` (the last one's, unread in the gap) started from the
   mailbox, then its first bar's planning a piece a block (song_first_bar_piece), in the order song_begin would have;
   the song's start then only starts the voices. Around each piece the song engine's state outside a song (song_ctx)
   is the prepared song's; after it, the last song's again, as the screens read it in the gap (that song's own set
   from a copy: its `song` prepared over). The mailbox's song as prepared: its generation (mb_gen) and kit; either
   changed since (a lock, a saved song recalled, the samples ready), it starts again from the mailbox, from the
   engine as the last song left it */
static uint8_t pre_n;                    /* pieces done: 0 none, 1 started, 1 + k its first bar's first k pieces */
static uint8_t pre_gen;
static int16_t pre_kit;
static uint8_t pre_had;                  /* gap_was taken (this gap's) */
static uint8_t pre_wait;                 /* blocks until the next piece: one every PRE_EVERY, so no two pieces (each up
                                            to a voicing or guitar-shape search, as the bar's planning ahead mid-song)
                                            in blocks one after the other */
#define PRE_EVERY 4
static song_ctx_t gap_was UI_POOL, gap_pre UI_POOL;
static lib_set_t gap_set UI_POOL;

/* the mailbox's song's kit: the locks', else the seeded pick (no retired kit: desk.js's approvedKit) */
static int mb_kit(void) { return mb_lk.kit >= 0 ? mb_lk.kit : kit_pick(&mb_roll); }

static void song_prepare(void)
{
    if (!pre_had) {
        song_ctx_get(&gap_was);
        gap_set = song.set;
        pre_had = 1;
    }
    if (pre_n && pre_gen != mb_gen)
        pre_n = 0;                       /* (the mailbox's song changed: the new one from its start) */
    if (!pre_n) {
        pre_gen = mb_gen;
        pre_kit = (int16_t)mb_kit();
        song_ctx_set(&gap_was);
        song_start_rows(&song, &mb_roll, pre_kit, mb_lk.row, mb_lk.val, mb_lk.n);   /* (a locked part's rows from
                                            the start) */
    } else {
        song_ctx_set(&gap_pre);
        song_first_bar_piece(&song, pre_n - 1);
    }
    pre_n++;
    pre_wait = PRE_EVERY - 1;
    song_ctx_get(&gap_pre);
    song_ctx_set(&gap_was);              /* (the last song's set: its copy) */
    if (gap_was.wb == &song.set)
        lib_set_wb(&gap_set, gap_was.wb_base);
#if VNL_UI_RUNTIME
    m_cause |= 1u << MC_SONG;
#endif
}
#endif

static void song_begin(uint32_t n)
{
#if UI_RT
    int kit = mb_kit();
    if (pre_n && pre_gen == mb_gen && pre_kit == kit) {   /* prepared in the gap: the parts left (one some of whose
                                            pieces are made makes the rest) */
        static const uint8_t LEFT[1 + SONG_FIRST_BAR_PIECES] = {0, 1, 1, 2, 3, 3, 3, 3, SONG_FIRST_BAR_PARTS};
        song_ctx_set(&gap_pre);
        first_part = LEFT[pre_n - 1];
    } else {
        if (pre_had)
            song_ctx_set(&gap_was);
        song_start_rows(&song, &mb_roll, kit, mb_lk.row, mb_lk.val, mb_lk.n);
        first_part = 0;
    }
    pre_n = pre_had = pre_wait = 0;
#else
    song_start(&song, &mb_roll);
    first_part = 0;
#endif
    BARRIER();
    mb_full = 0;                         /* the main loop rolls the next one */
    voices_begin();
#if UI_RT
    kit_hold = drums_wait;               /* (its kit's voices still rendering: comp and texture first) */
    kit_wait = 0;
    mutes_apply();
#endif
    r_now = r_tick = 0;
    r_pos = n * (1u + SONG_FIRST_BAR_PARTS);   /* (its first 16th after its first bar's planning, a part a block:
                                            the song's setup and that planning not in one block's render; the
                                            same when the gap prepared it: these blocks then light) */
    r_frac = 0.0f;
    memset(req_set, 0, sizeof req_set);
#if VNL_UI_RUNTIME
    m_cause |= 1u << MC_SONG;
#endif
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
    song.voicing.ant = 0;                /* (the harp's anticipated notes went before the stop: its downbeat whole) */
    r_now = r_pos = mark_pos;
    r_frac = mark_frac;
    r_tick = mark_tick;
    voices_begin();
#if VNL_UI_RUNTIME
    m_cause |= 1u << MC_SONG;
#endif
    held = 0;
    rs = RS_SONG;
    SLOG_RESUME(song.bar);
}

/* the music stops (the voices already told): a song cut short is held to continue */
static void stop_here(void)
{
    if (rs == RS_STOP)
        return;
    held = rs == RS_SONG && song.state == SS_PLAYING && r_tick;   /* (before its first 16th: no mark of its own) */
    rs = RS_STOP;
}

/* the silence between songs: 2-4 s, from the song's seed (at its end, or cut short by B_NEW); the next song started
   in it (song_prepare) */
static void gap_begin(void)
{
    uint32_t h = song.roll.seed * 2654435761u;
    vx_release_all();
#if VNL_UI_RUNTIME
    m_cause |= 1u << MC_SONG;
#endif
    gap_left = (2000u + (h >> 16) % 2001u) * 441u / 10u;
    SLOG_GAP((float)gap_left / 44100.0f);
    rs = RS_GAP;
}

#if UI_RT
/* Section jumps (the Sections screen, Loop, Stay on song): the song started again, its first bar planned a part a
   block (as a new song's: song_begin), and fast-forwarded silently, a few 16ths a block (the workbench's landing,
   spread over the bar before), then put in at the next bar line with the changes still waiting carried over. Loop:
   the section playing, from its last bar, back to its start; Stay on song: the bar before the last section (or
   inside it, at a 4-bar line) back to the section after the intro (index.html's) */
static song_t jsong UI_POOL;
static int16_t j_target = -1, j_from;    /* the bar the song jumps to (-1: none), the bar it was asked in */
static uint8_t j_ready, loop_on, endless_on;
static uint8_t j_first;                  /* the restarted song's first bar's planning: the next part (a block each) */
static uint8_t j_part;                   /* its target bar's planning again (requests changed): the next part, 0 none */
static uint32_t j_req;                   /* the song's requests as last carried into it (req_hash) */
static song_ev_t j_evs[64];
enum { J_TICKS = 24 };                    /* 16ths fast-forwarded a block */
static uint8_t bar_ticked;               /* the song ticked a costly 16th in this block (step): a bar's first (its
                                            events, a request landing) or one making a piece of the next bar's
                                            planning (song_plans_at) */
static uint8_t roll_line_due;            /* a bar line passed: the Parts roll's copy waits for a light block */
/* a song's 16th that costs (its downbeat, or a piece of its planning): the 16th it ticks, st of bar */
#define HEAVY_16TH(bar, st) ((st) == 0 || song_plans_at(bar, st))

/* the requests waiting in s (a hash: song_carry_requests carries the same from the same) */
static uint32_t req_hash(const song_t *s)
{
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < RF_N; i++)
        h = (h ^ (uint32_t)(s->pend[i] ? 1 + s->pend_bar[i] * 31 + s->pend_val[i] * 65599 : 0)) * 16777619u;
    for (i = 0; i < s->rq_n; i++)
        h = (h ^ (uint32_t)(s->rq[i].row * 65599 + s->rq[i].val * 31 + s->rq[i].bar)) * 16777619u;
    return (h ^ (uint32_t)s->rq_n) * 16777619u;
}

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
    memcpy(jsong.voicing.loop, song.voicing.loop, sizeof jsong.voicing.loop);   /* the loop's voicings: kept where
                                            its harmony is the same (their sig), not worked out again (its first
                                            bar's costliest part); else worked out as before */
    jsong.voicing.nloop = song.voicing.nloop;
    jsong.voicing.sig = song.voicing.sig;
    j_target = (int16_t)bar;
    j_from = (int16_t)song.bar;
    j_ready = 0;
    j_first = 0;
    j_part = 0;
    j_req = ~req_hash(&song);            /* (none carried yet) */
}

static void jump_carry(int b);

static void jump_prepare(void)
{
    int k;
    if (j_target >= 0 && !j_ready && j_first < SONG_FIRST_BAR_PARTS) {
        if (j_first == 0 && j_target == 0)
            jump_carry(0);               /* (the requests landing there, carried ahead: its first bar planned with
                                            them, not on its downbeat; jump_land carries them again as they are) */
        song_first_bar(&jsong, j_first++);   /* (all in its first tick, a block's render 2-3 times its usual) */
#if VNL_UI_RUNTIME
        m_cause |= 1u << MC_JUMP;
#endif
        return;
    }
    for (k = 0; k < J_TICKS && j_target >= 0 && !j_ready; k++) {
        if (jsong.bar >= j_target || jsong.state != SS_PLAYING) {
            j_ready = 1;
        } else {
            int heavy = HEAVY_16TH(jsong.bar, jsong.step);
            if (jsong.bar == j_target - 1 && song_plans_at(jsong.bar, jsong.step) == 1)
                jump_carry(j_target);    /* (likewise: the target bar's planning ahead, from here, with them; they
                                            land past the fast-forward, so its ticks keep theirs) */
            song_tick(&jsong, j_evs, 64);
#if VNL_UI_RUNTIME
            m_cause |= 1u << MC_JUMP;
#endif
            if (heavy)
                break;                   /* (a bar's generation or planning: one a block) */
        }
    }
    if (j_target >= 0 && jsong.bar >= j_target)
        j_ready = 1;
    if (j_target >= 0 && j_ready && k == 0 && jsong.state == SS_PLAYING && !jsong.step) {
        if (req_hash(&song) != j_req) {   /* waiting to land, the requests changed since carried: carried again, */
            jump_carry(jsong.bar);       /* its bar planned again with them, a part a block (song_first_bar) */
            j_part = 1;
        } else if (j_part) {
            song_first_bar(&jsong, j_part);
            j_part = (uint8_t)(j_part + 1 < SONG_FIRST_BAR_PARTS ? j_part + 1 : 0);
#if VNL_UI_RUNTIME
            m_cause |= 1u << MC_JUMP;
#endif
        }
    }
}

/* a carried request's bar in the song jumped to, landing from its bar b: the next 4-bar boundary there if its own has
   gone by; a jump back (Loop, Stay on song, an earlier section) lands it at once, as the song restarts there (its own
   bar, at or past the bar the jump left, may never come: a loop goes back before it) */
static int16_t jump_rebar(int bar, int b)
{
    if (b < song.bar)
        return (int16_t)b;
    return (int16_t)(bar >= b ? bar : b % 4 == 0 ? b : (b / 4 + 1) * 4);
}

/* the song's requests waiting, carried into the song jumped to (its own: none), landing from its bar b there */
static void jump_carry(int b)
{
    int f, i;
    jsong.rq_n = 0;
    memset(jsong.rq, 0, sizeof jsong.rq);
    memset(jsong.pend, 0, sizeof jsong.pend);
    memset(jsong.pend_val, 0, sizeof jsong.pend_val);
    memset(jsong.pend_bar, 0, sizeof jsong.pend_bar);
    song_carry_requests(&jsong, &song);
    for (f = 0; f < RF_N; f++)
        if (jsong.pend[f])
            jsong.pend_bar[f] = jump_rebar(jsong.pend_bar[f], b);
    for (i = 0; i < jsong.rq_n; i++)
        jsong.rq[i].bar = jump_rebar(jsong.rq[i].bar, b);
    j_req = req_hash(&song);
}

static void jump_land(void)
{
    jump_carry(jsong.bar);               /* (as they are now: any carried ahead put back first) */
    song = jsong;
    song.voicing.ant = 0;                /* (fast-forwarded silently: the harp's downbeat whole, on time) */
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

/* The Parts roll's notes: the song around the bar playing (the lanes' window), from the song itself. A copy of the
   song is ticked silently ahead to a bar past the window's end, a few 16ths a block, into the buffer the main loop
   doesn't read; then the buffers swap. At each bar line the copy goes on from where it was (one bar more: the song's
   own cost), the notes it had kept; only when the song changed what it can't foresee (a request, a jump: song_key)
   does it start again from the song as the bar began. */
enum { UI_ROLL_MAX = 1024, R_TICKS = 4 };
static struct { uint16_t n; ui_note_t note[UI_ROLL_MAX]; } roll_buf[2] UI_POOL;
static volatile uint8_t roll_front;
static song_t rsong UI_POOL;
static int16_t roll_to = -1;
static int16_t roll_bar = -1;            /* the bar line the copy last went on from (-1: none; it starts again) */
static uint32_t roll_key;                /* the song's song_key then */
static volatile uint8_t roll_shown;      /* the screen shown has the Parts roll (the main loop's): the look-ahead runs
                                            only then (its song ticks cost the render; nothing else reads its notes) */
static song_ev_t r_evs[64];

static int roll_part(int voice)
{
    return voice == V_CHORD ? UI_PART_KEYS : voice == V_BASS ? UI_PART_BASS : voice == V_LEAD ? UI_PART_LEAD
         : voice == V_PAD ? UI_PART_PAD : voice >= V_KICK && voice <= V_RIDE ? UI_PART_DRUMS : -1;
}

static void roll_bar_line(void)
{
    const ui_lanes_t *L = &UI_LANE_ITEMS[0];
    if (!roll_shown) {
        roll_to = roll_bar = -1;
        return;
    }
    int bars = song.roll.v[RF_BARS], span = L->window < bars ? L->window : bars, from = song.bar - span / 4, i;
    uint8_t back = (uint8_t)!roll_front;
    uint32_t key = song_key(&mark);
    int on = roll_to < 0 && roll_bar >= 0 && mark.bar == roll_bar + 1 && key == roll_key && rsong.bar >= mark.bar;
    if (from > bars - span) from = bars - span;
    if (from < 0) from = 0;
    roll_buf[back].n = 0;
    for (i = 0; i < roll_buf[roll_front].n; i++) {   /* the bars played keep their notes; going on, those ahead too */
        const ui_note_t *e = &roll_buf[roll_front].note[i];
        if (e->bar >= from && (on || e->bar < mark.bar))
            roll_buf[back].note[roll_buf[back].n++] = *e;
    }
    if (!on)
        rsong = mark;                    /* (the song as the bar began: mark, step's copy at the bar line) */
    roll_bar = (int16_t)mark.bar;
    roll_key = key;
    roll_to = (int16_t)(from + span + 1 < bars ? from + span + 1 : bars);   /* (a bar past: the next bar line's) */
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
#if VNL_UI_RUNTIME
        m_cause |= 1u << MC_ROLL;
#endif
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
        if (HEAVY_16TH(bar, st))
            break;                       /* (a bar's generation or planning: one a block) */
    }
}

static void roll_clear(void)
{
    roll_to = roll_bar = -1;
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
    if (drums_wait) {                    /* the shipped kit's voices rendered since: its lanes now */
        drums_apply();
#if VNL_UI_RUNTIME
        m_cause |= 1u << MC_ROW;
#endif
    }
#if UI_RT
    if (kit_hold && st == 0 && (!drums_wait || kit_wait >= KIT_WAIT_MAX)) {   /* the held parts in at this bar line */
        kit_hold = 0;
        mutes_apply();
    }
#endif
#if VNL_UI_RUNTIME
    m_cause |= 1u << MC_STEP;
#endif
#if UI_RT
    if (st == 0) {                       /* a bar line: a jump ready lands; the Loop's and Stay's; the roll's notes */
        bar_ticked = 1;
#if VNL_UI_RUNTIME
        m_cause |= 1u << MC_BAR;
#endif
        roll_line_due = 1;               /* (the roll's copy at the next light block: roll_bar_line) */
        if (j_target >= 0 && j_ready && song.bar > j_from)
            jump_land();
        jump_rules();
    }
    if (song_plans_at(song.bar, st))
        bar_ticked = 1;                  /* (a piece of the next bar's planning in this 16th's tick: song.c) */
#endif
#if VNL_UI_RUNTIME
    m_tick_bar = (uint16_t)song.bar;
    m_tick_step = (uint8_t)st;
    m_tick_piece = (uint8_t)song_plans_at(song.bar, st);
    if (m_tick_piece)
        m_cause |= 1u << MC_PLAN;
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
#if UI_RT
        harmony_antic(j_target < 0);      /* (no anticipation over a bar line a jump may leave from) */
#endif
        n = song_tick(&song, evs, 64);
        llp = song_lead_lp_at(&song, bar, st);   /* the lead loop's filter, for the 16th just played */
        vx_set_filter(PM_LEAD, lp[PM_LEAD] < llp ? llp : lp[PM_LEAD]);   /* before its events */
        {                                 /* the whole-mix stutter on a bar the lead loop marks, in its figure
                                             (lead.h's sfig): per figure {step, vx_stutter mode, length in 32nds};
                                             the next downbeat live */
            static const uint8_t FIG[5][3][3] = {
                {{14, 1, 2}, {15, 2, 0}},              /* 1: the bar's last 16th, twice */
                {{12, 1, 4}, {14, 2, 0}},              /* 2: beat 4's first 8th, again on its "and" */
                {{12, 1, 2}, {13, 2, 0}},              /* 3: beat 4's first 16th, 4 times */
                {{12, 1, 4}, {14, 3, 2}, {15, 3, 1}},  /* 4: beat 4 as an 8th, its 16th, then 32nds */
                {{8, 1, 4}, {10, 2, 0}, {14, 3, 2}}};  /* 5: an 8th on beat 3, repeated, then its 16th on beat 4 */
            static int stut;
            float h = 0.5f * song_step_samples(&song);
            int f = song.lead.loop.sfig - 1, k;
            if (st == 0 && stut) { vx_stutter(0, 0); stut = 0; }
            if (song.lead.loop.sbar == bar + 1 && f >= 0 && f < 5)
                for (k = 0; k < 3; k++)
                    if (FIG[f][k][1] && FIG[f][k][0] == st && (stut || FIG[f][k][1] == 1)) {
                        vx_stutter(FIG[f][k][1], (uint32_t)((float)FIG[f][k][2] * h));
                        stut = 1;
                    }
        }
        vx_set_pad_level(song_pad_level(&song));   /* the pad layer's fade over a section line */
        /* sounding notes keep their patches; dust and bed changes ramp, the delay's time glides to a new tempo */
        if (vx_reconfigures(&before, &song.roll) || song.changes != changes) {
            vx_configure(&song.roll);
            drums_apply();
#if VNL_UI_RUNTIME
            m_cause |= 1u << MC_ROW;
#endif
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
static int one_part(int ins)
{
    return ins == INS_KNIGHT || ins == INS_FELT_PIANO || ins == INS_VIBES || ins == INS_HARP;
}

static int row_other(int row, int avoid, uint32_t seed)
{
    int i, n = ch_count(row), k = 0, pick;
    for (i = 0; i < n; i++) {
        int v = ch_value(row, i), f = ch_flags(row, v);
        if (v != avoid && (f & 15) == 0) k++;   /* approved, not hidden, not the engine's own choice */
    }
    if (!k) return avoid;
    pick = (int)(seed % (uint32_t)k);
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
            song_request_row(&song, CH_SONG_ROW[ROW_LEAD], row_other(ROW_LEAD, v, song.roll.seed));
    } else if (row == ROW_LEAD && one_part(v) && rows[ROW_KEYS] == v)
        song_request_row(&song, CH_SONG_ROW[ROW_KEYS], row_other(ROW_KEYS, v, song.roll.seed));
}
#endif

static void command(uint32_t c)
{
    uint32_t op = c >> 24;
    int a = (int)((c >> 16) & 0xFFu), b = (int)(int16_t)(c & 0xFFFFu);
#if VNL_UI_RUNTIME
    m_cause |= 1u << MC_CMD;
#endif
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
        mute_user = (unsigned)b;
        mutes_apply();
    } else if (op == C_SOLO) {
        vx_solo(b);
    } else if (op == C_LEVEL) {
        vx_level_set(a, b);
    } else if (op == C_AB) {            /* the A/B switches' mix stages (the render's: vx_fx_off reassigns voices) */
        vx_fx_off((b & AB_CHODLY ? VX_OFF_CHORUS | VX_OFF_DELAY : 0u) | (b & AB_FULLRATE ? VX_OFF_HALF : 0u) |
                  (a & AB2_FILTERS ? VX_OFF_FILTERS : 0u) | (a & AB2_COMP ? VX_OFF_COMP : 0u) |
                  (a & AB2_GLUE ? VX_OFF_GLUE : 0u) | (a & AB2_MUD ? VX_OFF_MUD : 0u));
        fx_ab((b & AB_PLATE) != 0, (a & AB2_BED) != 0);
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
    snap.silent = rs != RS_SONG && !vx_sounding();
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
    snap.set_base = (uint8_t)((snap.has_song || snap.held) && (song.own & SONG_OWN_SET) ? song.set_base
                                                                                       : snap.roll.v[RF_SET]);
#if UI_RT
    if (snap.has_song || snap.ready) {   /* the screens' values: each row's, the plan's sections */
        const song_plan_t *p = snap.held ? &mark.plan : snap.has_song ? &song.plan : 0;
        int i;
        ui_song_rows(&snap.roll, snap.rows);
        if (snap.has_song || snap.held) {   /* the kit row: a shipped kit's value; the next song's, as it will pick */
            if (song.kit_preset >= 0)
                snap.rows[ROW_KIT] = (int16_t)(SONG_KIT_PRESET + song.kit_preset);
        } else {
            if ((i = mb_lk.kit >= 0 ? mb_lk.kit : kit_pick(&snap.roll)) >= 0)
                snap.rows[ROW_KIT] = (int16_t)i;
            for (i = 0; i < mb_lk.nshow; i++)    /* (the song ready: its locked parts as it will play them) */
                snap.rows[mb_lk.show[i]] = mb_lk.show_val[i];
            if (mb_lk.nshow)
                snap.rows[ROW_RHYTHM] = snap.rows[ch_rhythm_row(snap.rows[ROW_KEYS], snap.rows[ROW_COMP]) == ROW_COMP
                                                  ? ROW_COMP : ROW_STRUM_PAT];
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
    uint32_t m_t0 = M_T();
    uint32_t m_cause_t0 = m_cause_main;  /* (what the main loop was doing as the block began) */
    uint32_t m_fires = vx_fires();
    static int m_guard;
    pf_block_begin();
    cg_pre((uint32_t)vx_active());       /* the CPU guard (the FM-1's: app_init turns it on) */
    if (cg_level() != m_guard) {         /* (a step: since the last block's end, or now) */
        m_guard = cg_level();
        m_cause |= 1u << MC_GUARD;
    }
#endif
    float gain = (float)master_q12 * (8388607.0f / 4096.0f);   /* MASTER, read once a block: float -> 24-bit */
    if (remote_mute)
        gain = 0.0f;                     /* (gain_now ramps there: no click) */
    if (panic_now) {
        panic_now = 0;
        vx_init();
        stop_here();
#if VNL_UI_RUNTIME
        m_cause |= 1u << MC_CMD;
#endif
    }
    while (q_r != q_w) {
        uint32_t c = q[q_r % QN];
        BARRIER();
        q_r++;
        command(c);
    }
    if (rs == RS_SONG && first_part < SONG_FIRST_BAR_PARTS) {   /* the new song's first bar, a part a block */
        song_first_bar(&song, first_part++);
#if VNL_UI_RUNTIME
        m_cause |= 1u << MC_SONG;
#endif
    }
#if UI_RT
    if (rs == RS_GAP && mb_full && pre_n < 1 + SONG_FIRST_BAR_PIECES && !vx_active()) {   /* (the notes' releases
                                            over: the effects' tails alone render meanwhile) */
        if (pre_wait)
            pre_wait--;
        else
            song_prepare();
    }
#endif
    if (rs == RS_WAIT && mb_full)
        song_begin(n);
#if UI_RT
    if (kit_hold && rs == RS_SONG)
        kit_wait += n;
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
                    j_first = SONG_FIRST_BAR_PARTS;   /* (at once: its downbeat plans its first bar itself) */
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
#if UI_RT
    if (rs == RS_SONG && !bar_ticked) {  /* the jump's fast-forward, the roll's look-ahead: a few 16ths a block, at
                                            most one bar's first 16th (the costly one: its generation) each, and none
                                            in a block whose song generated a bar of its own */
        static uint8_t was_shown;
        jump_prepare();
        if (roll_line_due || (roll_shown && !was_shown && roll_to < 0)) {
            roll_line_due = 0;           /* (a bar line passed, or the roll's screen opened: its copy, this block's
                                            only work for it) */
            roll_bar_line();
        } else if (!(ab_flags & AB_ROLL) && roll_shown) {
            roll_generate();
        }
        was_shown = roll_shown;
    }
    bar_ticked = 0;
#endif
    for (k = 0; k < n; k++) {
        gain_now += (gain - gain_now) * 0.004f;   /* ~5 ms: turning MASTER does not click */
        float a = mix_l[k] * gain_now, b = mix_r[k] * gain_now;
        if (ab_flags2 & AB2_MONO)
            a = b = 0.5f * (a + b);
        lr[2 * k] = (int32_t)(a > 8388607.0f ? 8388607.0f : a < -8388607.0f ? -8388607.0f : a);   /* 24-bit */
        lr[2 * k + 1] = (int32_t)(b > 8388607.0f ? 8388607.0f : b < -8388607.0f ? -8388607.0f : b);
    }
    publish();
#if VNL_UI_RUNTIME
    pf_block_end();
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
    if (heavy_us) {                      /* (the host's block made as slow as the voices sounding: what the guard cuts
                                            lightens it) */
        uint32_t until = heavy_us * (uint32_t)vx_active();
        while (M_DT(m_t0) < until) {}
    }
#endif
    {
        uint32_t dt = M_DT(m_t0), blk = n * 1000000u / 44100u, c;
        if (dt >= M_SANE_US)
            dt = 0;                      /* (no reading: none of the figures below, nor the guard, take it) */
        if (dt > m_render_max)
            m_render_max = dt;
        if (vx_fires() != m_fires)
            m_cause |= 1u << MC_NOTES;
        if (vx_ramping())
            m_cause |= 1u << MC_RAMP;
        if (vx_sounding())
            m_cause |= 1u << MC_VOICES;
        if (dt > blk) {
            m_late++;
            c = m_cause | m_cause_main | m_cause_t0 | m_cause_seen;
            for (k = 0; k < MC_OTHER && !(c >> k & 1u); k++) {}
            m_late_by[k]++;              /* (the first cause, in MC_*'s order: the render's own before the main
                                            loop's around it; none: other) */
            k = m_late_n++ % M_LATE_AT;
            m_late_at[k].bar = m_tick_bar != 0xFFFFu ? m_tick_bar : (uint16_t)song.bar;
            m_late_at[k].step = m_tick_bar != 0xFFFFu ? m_tick_step : 255u;
            m_late_at[k].piece = m_tick_bar != 0xFFFFu ? m_tick_piece : 0u;
            m_late_at[k].us = (uint16_t)(dt > 65535u ? 65535u : dt);
            m_late_at[k].causes = c;
        }
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
        {                                /* VNL_SLOW=x (the simulator): each block over x times the running mean, its
                                            causes (the host's clock: what makes a block heavy, not how late) */
            static float mean;
            static const char *sl = (const char *)1;
            if (sl == (const char *)1)
                sl = getenv("VNL_SLOW");
            if (sl && mean > 0.0f && (float)dt > (float)atof(sl) * mean)
                printf("slow block: %u us (mean %.0f) bar %d step %u causes %05X main %05X\n", (unsigned)dt, mean,
                       song.bar, (unsigned)(r_tick & 15u), (unsigned)m_cause,
                       (unsigned)(m_cause_main | m_cause_t0 | m_cause_seen));
            mean = mean > 0.0f ? mean + ((float)dt - mean) * 0.01f : (float)dt;
        }
#endif
        m_cause = 0;
        m_cause_seen = 0;
        m_tick_bar = 0xFFFFu;
        m_load_now = blk ? dt * 100u / blk : 0u;
        if (m_load_now > m_load_peak)
            m_load_peak = m_load_now;
        if (blk && dt)
            cg_post(dt * 256u / blk);    /* (its load: 1/256 of the block's time) */
    }
#endif
}

void app_panic(void)
{
    panic_now = 1;
}

#ifdef APP_OVERLOAD
void app_overload(void)
{
#if VNL_UI_RUNTIME
    m_over90++;
#endif
}
#endif

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

#if UI_RT
/* ---- the locks (SEQ held on a part's screen: the navigator's ui_hnav.locks): a new song keeps a locked part ----
   A locked part keeps, into each new song (SEQ's, or a song's end), what it plays now: its instrument, its rhythm or
   style, its register and its re-rolls' salts (desk.js's locks, by part); the drums their kit, pattern, kick phrase,
   ride and variation; the texture its dust, noise and room. The rest rolls as ever. The main loop reads them off the
   song playing (or held), "as rolled" resolved to what plays, so a change made to a locked part is kept too, and
   hands them to the render with the next song (the mailbox; song_start_rows sets them from its first bar); a change
   to them after the next song was rolled rolls it again, from the same seed. A song that can't hold them (a felt
   piano, Knight or vibes the comp keeps while the new lead plays it too: they play one part a song; organ keys under
   an organ pad) is passed over for the chain's next seed, LK_TRIES at most (desk.js's SEED_TRIES); none: the last,
   its unlocked part moved off the clash, or with both parts locked the second's instrument not kept, and said as the
   song starts. Never saved: power-off forgets them (no flash write while playing). */
static ui_nav_t ui_hnav;
enum { LK_TRIES = 20 };
/* each part's rows, in UI_PART_* order (drums, bass, keys, pad, lead, texture, then the song line), -1 ending each.
   The song line's: its mood first (roll_set fits the scale and chords to it), then the scale (one the mood allows:
   it was), the key, tempo and motion; a locked key without its scale would be another key's song */
static const int8_t LK_ROWS[] = {
    ROW_KIT, ROW_PATTERN, ROW_KICK, ROW_RIDE, ROW_DRUMS_VAR, -1,
    ROW_BASS, ROW_BASS_LINE, -1,
    ROW_KEYS, ROW_COMP, ROW_STRUM_PAT, ROW_STRUM_FILL, ROW_VOICINGS, ROW_KEYS_OCT, ROW_BUSY_THIN, -1,
    ROW_PAD, ROW_PAD_ON, ROW_PAD_PLACE, ROW_PAD_NOTES, -1,
    ROW_LEAD, ROW_LEAD_STYLE, ROW_PHRASE, ROW_LEAD_OCT, ROW_LEAD_LOOP, -1,
    ROW_DUST, ROW_NOISE, ROW_AMBIENCE, -1,
    ROW_MOOD, ROW_MODE, ROW_KEY, ROW_TEMPO, ROW_MOTION, -1,
};
static int16_t lk_val[ROW_N];           /* each locked row's value, read off the song playing */
static uint8_t lk_have;                 /* the parts read (bit UI_PART_*): one locked before any song played keeps
                                           nothing until one does */
static uint32_t mb_sig;                 /* the locks the mailbox's song was rolled under (lk_sig) */
static uint32_t mb_seed;                /* the first seed it tried, and the roll before it: to roll it again */
static song_roll_t mb_prev;
static uint8_t mb_prev_any;
static uint8_t mb_msg;                  /* a lock the mailbox's song can't keep: 1 the lead's instrument, 2 the pad's */
static uint8_t mb_recalled;             /* the mailbox holds a saved song recalled (sv_recall): the locks leave it be */
static int32_t lk_songs;                /* the songs started, as last seen */

static uint8_t lk_mask(void) { return (uint8_t)(ui_hnav.locks & lk_have); }

static int lk_part(int row)             /* the part a row belongs to (UI_PART_*), -1 none */
{
    int i, p = 0;
    for (i = 0; i < (int)sizeof LK_ROWS; i++) {
        if (LK_ROWS[i] < 0) p++;
        else if (LK_ROWS[i] == row) return p;
    }
    return -1;
}

static int lk_locked(int row) { int p = lk_part(row); return p >= 0 && (lk_mask() >> p & 1u); }

/* the rows of parts m (bit UI_PART_*) from the song shown, into out: the row's value, "as rolled" as it plays */
static void lk_resolve(const snap_t *s, uint8_t m, int16_t *out)
{
    song_rolled_t r;
    int i, p = 0;
    song_rolled(&s->roll, &r);
    for (i = 0; i < (int)sizeof LK_ROWS; i++) {
        int row = LK_ROWS[i], v;
        if (row < 0) {
            p++;
            continue;
        }
        if (!(m >> p & 1u)) continue;
        v = s->rows[row];
        switch (row) {
        case ROW_COMP: if (v < 0) v = r.comp_pat >= 0 ? 100 + r.comp_pat : r.comp; break;
        case ROW_STRUM_PAT: if (v < 0) v = r.strum_pat; break;
        case ROW_STRUM_FILL: if (v < 0) v = r.strum_fill; break;
        case ROW_LEAD_STYLE: if (v < 2) v = 2 + r.lead_style; break;
        case ROW_PAD_ON: if (v < 0) v = r.pad_on; break;
        case ROW_PAD_PLACE: if (v < 0) v = r.pad_place; break;
        case ROW_RIDE: if (v < 0) v = r.ride; break;
        default: break;
        }
        out[row] = (int16_t)v;
    }
}

/* the locked parts' rows from the song playing (or held) */
static void lk_read(const snap_t *s, uint8_t m)
{
    lk_resolve(s, m, lk_val);
    lk_have |= m;
}

static uint32_t lk_sig(void)            /* the locks and what they keep, hashed */
{
    uint32_t h = 2166136261u ^ lk_mask();
    int i;
    for (i = 0; i < (int)sizeof LK_ROWS; i++)
        if (LK_ROWS[i] >= 0 && lk_locked(LK_ROWS[i]))
            h = (h ^ (uint16_t)lk_val[LK_ROWS[i]]) * 16777619u;
    return h;
}

/* the instrument a song rolled r plays on a role's row (ROW_KEYS, ROW_LEAD, ROW_PAD): a locked one's, else its set's */
static int lk_ins(const song_roll_t *r, int row)
{
    const lib_set_t *set;
    if (lk_locked(row)) return lk_val[row];
    set = lib_set_by_id((uint8_t)r->v[RF_SET]);
    if (!set) return -1;
    return row == ROW_KEYS ? lib_keys_approved(set->chords) : row == ROW_LEAD ? lib_lead_approved(set->lead)
                                                                                : lib_song_pad(r);
}

/* the song rolled r against the locks: 0 it holds them, 1 the comp and the lead on one instrument that plays one part
   a song, 2 organ keys under an organ pad (lib_song_pad's rule) */
static int lk_clash(const song_roll_t *r)
{
    int k;
    if (!lk_mask()) return 0;
    k = lk_ins(r, ROW_KEYS);
    if ((lk_locked(ROW_KEYS) || lk_locked(ROW_LEAD)) && one_part(k) && k == lk_ins(r, ROW_LEAD)) return 1;
    if ((lk_locked(ROW_KEYS) || lk_locked(ROW_PAD)) && k == INS_ORGAN && lk_ins(r, ROW_PAD) == INS_ORGAN) return 2;
    return 0;
}

static void lk_add(lk_box_t *b, int row, int sr, int v)   /* a row the song starts with, and shows when ready */
{
    if (sr >= 0 && b->n < LK_MAX) {
        b->row[b->n] = (int16_t)sr;
        b->val[b->n++] = (int16_t)v;
    }
    if (row >= 0 && b->nshow < LK_MAX) {
        b->show[b->nshow] = (int8_t)row;
        b->show_val[b->nshow++] = (int16_t)v;
    }
}

/* the mailbox's locks for the song rolled r (its roll fields set in r itself): a clash still there moves the unlocked
   part off it (another approved instrument, from the song's seed: row_rules' way), or drops the second lock */
static void lk_fill(song_roll_t *r)
{
    lk_box_t *b = &mb_lk;
    int i, p = 0, clash = lk_clash(r), drop = -1;
    uint8_t m = lk_mask();
    b->kit = -1;
    b->n = b->nshow = 0;
    mb_msg = 0;
    if (clash == 1 && !lk_locked(ROW_LEAD))
        lk_add(b, ROW_LEAD, SR_ROLE + V_LEAD, row_other(ROW_LEAD, lk_val[ROW_KEYS], r->seed));
    else if (clash == 1 && !lk_locked(ROW_KEYS))
        lk_add(b, ROW_KEYS, SR_ROLE + V_CHORD, row_other(ROW_KEYS, lk_val[ROW_LEAD], r->seed));
    else if (clash == 2 && !lk_locked(ROW_PAD))
        lk_add(b, ROW_PAD, SR_ROLE + V_PAD, row_other(ROW_PAD, INS_ORGAN, r->seed));
    else if (clash == 2 && !lk_locked(ROW_KEYS))
        lk_add(b, ROW_KEYS, SR_ROLE + V_CHORD, row_other(ROW_KEYS, INS_ORGAN, r->seed));
    else if (clash) {
        drop = clash == 1 ? ROW_LEAD : ROW_PAD;
        mb_msg = (uint8_t)clash;
    }
    for (i = 0; i < (int)sizeof LK_ROWS; i++) {
        int row = LK_ROWS[i], v, sr;
        if (row < 0) {
            p++;
            continue;
        }
        if (!(m >> p & 1u) || row == drop) continue;
        v = lk_val[row];
        sr = CH_SONG_ROW[row];
        if (row == ROW_KIT) {
            b->kit = (int16_t)v;
            lk_add(b, row, -1, v);
        } else if (sr >= SR_ROLL && sr < SR_ROLL + RF_N) {   /* the pattern, the texture, the song line: the roll's
                                                                own (lk_rolls) */
        } else if (row == ROW_COMP) {    /* a comp rhythm (from 100): the COMP_PAT override; else COMP (command()) */
            lk_add(b, row, SR_OV + OV_COMP, v >= 100 ? -1 : v);
            lk_add(b, -1, SR_OV + OV_COMP_PAT, v >= 100 ? v - 100 : -1);
        } else
            lk_add(b, row, sr, v);
    }
}

/* the locked roll fields (the drums' pattern, the texture, the song line) set in the song rolled r, in LK_ROWS'
   order: 1 each holds (roll_set took it), 0 one didn't (another seed is tried) */
static int lk_rolls(song_roll_t *r)
{
    int i, p = 0, held = 1;
    uint8_t m = lk_mask();
    for (i = 0; i < (int)sizeof LK_ROWS; i++) {
        int row = LK_ROWS[i], sr;
        if (row < 0) {
            p++;
            continue;
        }
        sr = CH_SONG_ROW[row];
        if ((m >> p & 1u) && sr >= SR_ROLL && sr < SR_ROLL + RF_N &&
            roll_set(r, sr - SR_ROLL, lk_val[row]) != lk_val[row])
            held = 0;
    }
    return held;
}

/* each frame, before the mailbox is filled: the locks read off the song playing; a song started that couldn't keep
   one, said; the mailbox's song rolled again when the locks changed since it was rolled */
static void lk_frame(void)
{
    snap_t s;
    int32_t taken;
    uint8_t m = ui_hnav.locks;
    if (!m && !lk_have && !mb_msg)
        return;
    snap_read(&s);
    if (s.songs != lk_songs) {
        if (mb_msg && !mb_full)
            app_say(mb_msg == 1 ? "LEAD LOCK" : "PAD LOCK", "INSTRUMENT NOT KEPT");
        if (!mb_full)
            mb_msg = 0;
        lk_songs = s.songs;
    }
    lk_have &= m;                        /* (a part unlocked keeps nothing: read again when locked again) */
    if (m && (s.has_song || s.held))
        lk_read(&s, m);
    taken = songs_started;
    if (mb_full && !mb_recalled && lk_sig() != mb_sig) {   /* rolled again, from the same seed, unless the render took
                                            it meanwhile (a saved song recalled: never) */
        mb_full = 0;
        BARRIER();
        if (songs_started == taken) {
            last_roll = mb_prev;
            rolled_any = mb_prev_any;
            next_seed = mb_seed;
        }
    }
}
#endif

static void roll_next(void)             /* main loop: keep the mailbox full */
{
    int t;
    if (mb_full || !seeded)
        return;
#if VNL_UI_RUNTIME
    M_SEEN(MC_NEXT);
#endif
#if UI_RT
    mb_prev = last_roll;
    mb_prev_any = rolled_any;
    mb_seed = next_seed;
#endif
    for (t = 0;; t++) {                  /* (with locks: a seed whose song can't hold them passed over) */
        roll_song(rolled_any ? &last_roll : 0, next_seed, &mb_roll);
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
        if (set_override && !rolled_any)
            roll_set(&mb_roll, RF_SET, set_override);
#endif
#if UI_RT
        if (!lk_rolls(&mb_roll) && t + 1 < LK_TRIES) {   /* (a roll field it can't take) */
            next_seed = seed_next(next_seed);
            continue;
        }
        if (t + 1 < LK_TRIES && lk_clash(&mb_roll)) {
            next_seed = seed_next(next_seed);
            continue;
        }
        lk_fill(&mb_roll);
        mb_sig = lk_sig();
#endif
        break;
    }
    last_roll = mb_roll;
    rolled_any = 1;
    next_seed = seed_next(next_seed);
#if UI_RT
    mb_recalled = 0;
#endif
    mb_gen++;
    BARRIER();
    mb_full = 1;
}

#if UI_RT
/* a saved song recalled (the Saved list's EDIT): the next song, in the mailbox in place of the one rolled; its roll
   as saved, and the rows that differ from what that roll plays handed to the render as the locks' are (song_start_rows
   from its first bar: an instrument or the kit then makes its set its own, as a change made while it played did).
   Playing (or held), it comes in as a new song does (C_NEW: the song playing gives way, the gap, then it); stopped,
   PLAY starts it. The chain of seeds goes on after it as before */
static void sv_recall(const sv_song_t *sg, int playing)   /* playing: or held */
{
    lk_box_t *b = &mb_lk;
    int i, kit_rolled;
    mb_full = 0;                         /* (the render takes no song meanwhile) */
    BARRIER();
    mb_roll = sg->roll;
    kit_rolled = kit_pick(&mb_roll);
    if (kit_rolled < 0) kit_rolled = lib_song_kit(&mb_roll);
    b->kit = -1;
    b->n = b->nshow = 0;
    mb_msg = 0;
    for (i = 0; i < SV_NROWS; i++) {
        int row = SV_ROWS[i], v = sg->rows[i];
        if (v == sv_rolled(sg, i, kit_rolled)) continue;
        if (row == ROW_KIT) {
            b->kit = (int16_t)v;
            lk_add(b, row, -1, v);
        } else if (row == ROW_COMP) {    /* a comp rhythm (from 100): the COMP_PAT override; else COMP (command()) */
            lk_add(b, row, SR_OV + OV_COMP, v >= 100 ? -1 : v);
            lk_add(b, -1, SR_OV + OV_COMP_PAT, v >= 100 ? v - 100 : -1);
        } else
            lk_add(b, row, CH_SONG_ROW[row], v);
    }
    last_roll = mb_roll;                 /* (the next song's novelty: against this one) */
    rolled_any = 1;
    mb_sig = lk_sig();
    mb_recalled = 1;
    mb_gen++;
    BARRIER();
    mb_full = 1;
    if (playing)
        post(C_NEW, 0, 0);
}
#endif

/* Settings' BASS SATURATION: on (1, the default) or off, the engine's (vx_bass_sat); host renders may force it
   (VNL_BASS_SAT=0 or 1: bass_sat_env, over the setting kept) */
static uint8_t bass_sat = 1;
static int8_t bass_sat_env = -1;
static inline void bass_sat_engine(void) { vx_bass_sat(bass_sat_env >= 0 ? bass_sat_env : bass_sat); }

void app_init(void)
{
    song_first_bar_later(1);             /* (a new song's first bar planned a part a block: song_begin, app_render) */
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
            const char *bs = getenv("VNL_BASS_SAT");   /* VNL_BASS_SAT=0: the bass's harmonics off (Settings' BASS
                                                          SATURATION), 1 on, whatever the setting kept */
            bass_sat_env = (int8_t)(bs && (*bs == '0' || *bs == '1') && !bs[1] ? *bs - '0' : -1);
            bass_sat_engine();
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
            const char *kt = getenv("VNL_KIT");    /* VNL_KIT=22: every song on that kit (the kit row's value: 16 +
                                                      wb_drums.h's WB_KIT_*), as bench.c's */
            kit_override = kt && *kt ? atoi(kt) : -1;
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
        {
            const char *kw = getenv("VNL_KEYS_WITH");   /* VNL_KEYS_WITH=1: the keys with a low lead, 0 lifted
                                                           over it (the workbench's keys_with); unset, the draw */
            if (kw && (kw[0] == '0' || kw[0] == '1') && !kw[1]) {
                song_ov_t o = *song_ov();
                song_ov_set(&o, OV_KEYS_WITH, kw[0] - '0');
                song_override(&o);
            }
        }
        {
            const char *mt = getenv("VNL_MUTE");   /* VNL_MUTE=<mask>: those buses out of the mix (vx_mute's bits, as
                                                      C_MUTE's: 1 << V_CHORD the keys), to isolate a part by
                                                      subtracting renders */
            mute_user = mt ? (unsigned)strtoul(mt, 0, 0) : 0u;
        }
        {
            const char *sf = getenv("VNL_STUT");   /* VNL_STUT=1: every group's last bar a simple stutter may take
                                                      stutters, the figures 1-5 in turn (a listening demo) */
            leads_loop_force_stutter(sf && *sf == '1');
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
#if VNL_UI_RUNTIME && !defined(PLAT_HOST)
    cg_on(1, 240u, 204u, 256u);          /* the CPU guard: over 94 % of a block with blocks late among them it steps
                                            quality down (cpuguard.h), back under 80 %; the host simulator and the
                                            workbench never (their time isn't the FM-1's) */
    cg_need_late(2);
#endif
#if VNL_UI_RUNTIME
    pf_init(plat_ticks24, PLAT_TICKS_PER_US);   /* the CPU meter by stage (prof.h; the live state's "prof") */
#endif
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
#if VNL_UI_RUNTIME
    m_cause_main |= 1u << MC_FLASH;      /* (until the write ends: app_usb_cmd) */
#endif
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
#if VNL_UI_RUNTIME
    m_cause_main &= ~(1u << MC_FLASH);   /* (the write over: before, the cause stayed set until a reset) */
#endif
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
    uint8_t redraw_input;                /* an encoder turned (input: the splash ends) */
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

#if UI_RT
static void live_reply(void);
static void live_reset(void);

/* the saved songs over USB (tools/fm1_songs.py), each record as 2 * SV_BYTES nibbles, high first (saved.h's format):
 *   05 ss         slot ss's record -> 15 ss st <nibbles>   st: 0 empty (the record all zero), 1 written to the flash,
 *                                                          2 in RAM only, its save waiting for silence, 3 in RAM
 *                                                          only, its flash write failed (until the next save)
 *   06 ss <nib>   into slot ss (a record all zero: the slot deleted) -> 16 ss rc   rc: 0 written to the flash, 1 in RAM
 *                 only, waiting for silence (the music playing), 2 refused: another save waits, 3 refused: not a
 *                 record, 4 in RAM only: the flash write failed
 * The flash is written only while all is silent, as a save on the device is. Main loop (ota_frame_get) */
#define SONG_GET 0x05u
#define SONG_PUT 0x06u
static uint8_t backup_held;             /* a backup streams: no flash writes (app_backup_hold) */
/* the flash may be written now: all silent (a write stalls the audio), no backup streaming */
static int may_write(void) { return ui.shown.silent && !backup_held; }
static void song_slot_cmd(const uint8_t *p, uint32_t n)
{
    uint8_t rec[SV_BYTES], f[8 + 2 * SV_BYTES];
    int slot = p[4], i, rc;
    f[0] = 0xF0u;
    f[1] = RM_PFX0;
    f[2] = RM_PFX1;
    f[3] = RM_PFX2;
    f[5] = (uint8_t)slot;
    if (p[3] == SONG_GET) {
        f[4] = SONG_GET + 0x10u;
        f[6] = (uint8_t)(sv_get_bytes(slot, rec) ? 0 : sv_slots()[slot].state);
        if (f[6] == UI_SLOT_WAITING && sv_failed())
            f[6] = 3u;
        for (i = 0; i < SV_BYTES; i++) {
            f[7 + 2 * i] = (uint8_t)(rec[i] >> 4);
            f[8 + 2 * i] = (uint8_t)(rec[i] & 15u);
        }
        f[7 + 2 * SV_BYTES] = 0xF7u;
        plat_sysex_send(f, 8u + 2u * SV_BYTES);
        return;
    }
    if (n < 5u + 2u * SV_BYTES)
        rc = 3;
    else {
        for (i = 0; i < SV_BYTES; i++)
            rec[i] = (uint8_t)((p[5 + 2 * i] & 15u) << 4 | (p[6 + 2 * i] & 15u));
        rc = sv_put_bytes(slot, rec, may_write());
        rc = rc == UI_SAVE_BUSY ? 2 : rc < 0 && rc != UI_SAVE_EMPTY ? 3 : sv_failed() ? 4 : sv_waiting() ? 1 : 0;
        ui.redraw = 1;
    }
    f[4] = SONG_PUT + 0x10u;
    f[6] = (uint8_t)rc;
    f[7] = 0xF7u;
    plat_sysex_send(f, 8u);
}
#endif

int app_sysex(const uint8_t *p, uint32_t n)
{
#if VNL_UI_RUNTIME
    M_SEEN(MC_USB);
#endif
    if (n < 4u || p[0] != RM_PFX0 || p[1] != RM_PFX1 || p[2] != RM_PFX2)
        return 0;
#if UI_RT
    if (p[3] == 0x04u) {                 /* the live state (below): answered at once, the remote on or not; 04 01:
                                            then its measures back to zero */
        if (n >= 6u && p[4] == 0x02u) {   /* 04 02 ff [gg]: the A/B switches (AB_*, AB2_*), then the answer */
            ab_flags = (uint8_t)(p[5] & 0x7Fu);
            ab_flags2 = (uint8_t)(n >= 7u ? p[6] & 0x7Fu : 0u);
            post(C_AB, ab_flags2, ab_flags);
#if VNL_UI_RUNTIME && !defined(PLAT_HOST)
            cg_on(!(ab_flags & AB_GUARD), 240u, 204u, 256u);
            cg_need_late(2);
#endif
            drums_wait = 1;              /* (the lanes again at the next 16th: drums_apply) */
        }
        live_reply();
        if (n >= 5u && p[4] == 0x01u)
            live_reset();
        return 1;
    }
    if ((p[3] == SONG_GET || p[3] == SONG_PUT) && n >= 5u && p[4] < UI_SLOTS) {   /* the saved songs (below) */
        song_slot_cmd(p, n);
        return 1;
    }
#endif
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
    if (p == UI_PART_DRUMS) {            /* the drums' re-roll draws a new pattern too (desk.js): the next approved one
                                            in the table (ids have gaps: the bossa's 8, removed) */
        int i, at = 0;
        for (i = 0; i < LIB_NPATTERNS; i++)
            if (LIB_PATTERNS[i].id == ui_hsnap.rows[ROW_PATTERN]) at = i;
        for (i = 1; i <= LIB_NPATTERNS; i++)
            if (LIB_PATTERNS_STATUS[(at + i) % LIB_NPATTERNS] == 1) break;
        post(C_ROW, ROW_PATTERN, LIB_PATTERNS[(at + i) % LIB_NPATTERNS].id);
    }
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
/* the saved songs (app/ui/saved.c): the song shown (ui.shown, the navigator's changes waiting landed in it) into a
   slot, the flash written now only while all is silent; a slot's song the next one; a slot deleted */
static int ui_e_save(int slot)
{
    sv_song_t sg;
    if (sv_capture(&ui.shown, &ui_hnav, &sg)) return UI_SAVE_NONE;
    return sv_put(slot, &sg, may_write());
}
static int ui_e_recall(int slot)
{
    sv_song_t sg;
    if (sv_load(slot, &sg)) return UI_SAVE_EMPTY;
    sv_recall(&sg, ui.shown.playing || ui.shown.held);
    return 0;
}
static int ui_e_erase(int slot) { return sv_erase(slot, may_write()); }
static void m_readout(void);

/* the theme (Settings' THEME, SELECT or ALGO there): any of base's gfx.c palettes, switched at once (palette_set,
   the screens' theme colors and the record's disc, the whole screen redrawn) and kept with the home (set_keep).
   APP_PALETTE until one is picked */
static uint8_t theme = APP_PALETTE % NPALETTES;
static void theme_apply(uint32_t i)
{
    uint32_t k;
    uint16_t bg;
    theme = (uint8_t)(i % NPALETTES);
    palette_set(theme);
    UI_COLORS[UC_BLACK] = pal_bg;
    UI_COLORS[UC_WHITE] = pal_white;
    for (k = 0; k < 5u; k++)
        UI_COLORS[UC_LINE + k] = pal[k];
    bg = pal_bg;                         /* the disc: about 55% of the background, each RGB565 channel (device.js's) */
    UI_COLORS[UC_VINYL] = (uint16_t)((((bg >> 11) * 55u + 50u) / 100u) << 11 |
                                     ((((bg >> 5) & 63u) * 55u + 50u) / 100u) << 5 | ((bg & 31u) * 55u + 50u) / 100u);
}
static const char *ui_e_theme(int detents)
{
    if (detents) {
        theme_apply((uint32_t)(((int)theme + detents) % (int)NPALETTES + (int)NPALETTES));
        lcd_fill(0, 0, 240, 240, C_BLACK);
        ui_draw_all();
        ui.redraw = 1;
    }
    return PALETTES[theme].name;
}
/* the bass's harmonics (Settings' BASS SATURATION, PRESET there: Sam, 2026-11-12, buzzy on nice speakers): on (the
   default) or off, each detent flipping it; the engine takes it at the next fresh bass note (vx_bass_sat), the device
   keeps it with the home (set_keep) */
static const char *ui_e_bass_sat(int detents)
{
    if (detents & 1) {
        bass_sat = (uint8_t)!bass_sat;
        bass_sat_engine();
    }
    return bass_sat ? "ON" : "OFF";
}
/* the home (Settings' HOME SCREEN: Sam, 2026-11-13, "just let the user set one"): SIMPLE, the now-playing screen
   (the default), or DETAILED, the main screen, each detent flipping it; where boot, a new song and HOME from
   elsewhere land (HOME on those two only switches between them). The screen shown stays, the breadcrumb under it
   now from the new home (GLO back lands there); kept by set_keep */
static const char *ui_e_home_screen(int detents)
{
    if (detents & 1) {
        ui_hnav.home = (uint8_t)(ui_hnav.home == US_NOW ? US_MAIN : US_NOW);
        if (ui_hnav.depth > 1) ui_hnav.stack[0] = ui_hnav.home;
    }
    return ui_hnav.home == US_NOW ? "SIMPLE" : "DETAILED";
}
static const ui_engine_t UI_HENG = {ui_e_set, ui_e_play, ui_e_reroll, ui_e_reroll_part, ui_e_jump, ui_e_mute,
                                    ui_e_solo, ui_e_muted, ui_e_soloed, ui_e_level, ui_e_level_get,
                                    ui_e_save, ui_e_recall, ui_e_erase, m_readout, ui_e_theme, ui_e_bass_sat,
                                    ui_e_home_screen};

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

/* the boot splash (screens.js's splash): shown from app_splash, its record still, then from app_ui_init spinning up,
   slowing and settling at its first pose after two turns (SPLASH_TURN_MS); the engine, USB and the input run all along
   (a control pressed ends it, the home as it is). Then the home: the main screen at SPLASH_MS (a cut); now playing,
   which shares the record (Sam, 2026-11-11), a fade: the splash's text out into the background, the song's in
   (SPLASH_FADE_MS each, 4 steps of {fade}), only the text's columns redrawn, the record staying */
#define SPLASH_MS 1300u
#define SPLASH_TURN_MS 1150u
#define SPLASH_FADE_MS 150u
static uint32_t splash_t0;
static uint8_t splash_on, splash_fading;  /* the splash shown; now playing's text fading in after it */

/* the screen shown: the splash, or the navigator's */
static int ui_shown_id(void) { return splash_on ? US_SPLASH : ui_hnav.stack[ui_hnav.depth - 1]; }

static void ui_host_draw(void)
{
    static int last = -1;
    int id, bi, cover = 0;
    ui_bag_t b;
    ui_host_snap();
    id = ui_shown_id();
    for (bi = 0; bi < UI_SCREENS[id].n_bands; bi++) cover += UI_BANDS[UI_SCREENS[id].bands + bi].h;
    if (id != last) {                    /* another screen: what its bands leave uncovered, cleared (none: every row is
                                            drawn over, so a record two screens share stays put) */
        if (cover < 240) lcd_fill(0, 0, 240, 240, C_BLACK);
        ui_draw_all();
    }
    last = id;
    ui_values(&ui_hsnap, &ui_hnav, &b);
    b.screen = id;
    b.act = ui_activity();
    ui_draw(&UI_SCREENS[id], &b, 0xFFFFFFFFu);
}

/* the screen's text alone (a fade's step: right of its records) */
static void ui_host_text(void)
{
    int id = ui_shown_id();
    ui_bag_t b;
    ui_values(&ui_hsnap, &ui_hnav, &b);
    b.screen = id;
    b.act = ui_activity();
    ui_draw_text(&UI_SCREENS[id], &b);
}

/* the record's turn (ui_hnav.spin, 1/256 turns): the splash's; else 33 1/3 turns a minute while a song plays, easing
   to a stop when it stops and back up on play. Only a screen with a record redraws for it, its label alone */
static float spin_turns, spin_speed;      /* turns, turns a ms */
static uint32_t spin_at;
/* the fade's step at u ms into a SPLASH_FADE_MS half: 1..4 */
static int fade_step(uint32_t u) { return (int)(u * 4u / SPLASH_FADE_MS) + 1 > 4 ? 4 : (int)(u * 4u / SPLASH_FADE_MS) + 1; }

/* the splash's end, a control pressed: the home as it is */
static void splash_end(void)
{
    splash_on = splash_fading = 0;
    ui_hnav.fade = 0;
    ui.redraw = 1;
}

static void spin_tick(uint32_t now)
{
    float dt = spin_at ? (float)(now - spin_at < 100u ? now - spin_at : 100u) : 0.0f, want;
    uint32_t t = now - splash_t0;
    int fading = ui_hnav.home == US_NOW && UI_SCREENS[US_NOW].fade;
    spin_at = now;
    if (splash_on && fading && t >= SPLASH_TURN_MS + SPLASH_FADE_MS) {
        splash_on = 0;                   /* now playing, its text gone into the background: fading in */
        splash_fading = 1;
    }
    if (splash_fading) {
        uint32_t u = t - SPLASH_TURN_MS - SPLASH_FADE_MS;
        if (u >= SPLASH_FADE_MS) splash_fading = 0;
        ui_hnav.fade = (uint8_t)(splash_fading ? 16 - 4 * fade_step(u) : 0);
    }
    if (splash_on) {
        float u = (float)t / (float)SPLASH_TURN_MS;
        if (fading)                      /* the splash's text fading out */
            ui_hnav.fade = (uint8_t)(t < SPLASH_TURN_MS ? 0 : 4 * fade_step(t - SPLASH_TURN_MS));
        else if (t >= SPLASH_MS) {
            splash_on = 0;
            ui.redraw = 1;
        }
        if (u > 1.0f) u = 1.0f;
        spin_turns = 0.0f;
        spin_speed = 0.0f;
        ui_hnav.spin = (uint8_t)((int)(512.0f * (u * u * (3.0f - 2.0f * u))) & 255);
        return;
    }
    want = ui.shown.has_song ? 100.0f / 3.0f / 60000.0f : 0.0f;
    spin_speed += (want - spin_speed) * (dt < 400.0f ? dt / 400.0f : 1.0f);
    if (want == 0.0f && spin_speed < 1e-6f) spin_speed = 0.0f;
    spin_turns += spin_speed * dt;
    spin_turns -= (float)(int)spin_turns;
    ui_hnav.spin = (uint8_t)((int)(spin_turns * 256.0f) & 255);
}

/* the measures in the header: the longest redraw, the CPU guard's level and steps, the late blocks, the longest
   render (SAVE where no screen maps it; SAVE held on the Song screen) */
static void m_readout(void)
{
#if VNL_UI_RUNTIME
    char a[UI_MSG], c[UI_MSG], v[12];
    a[0] = c[0] = 0;
    text_cat(a, "REDRAW ");
    int_text((int32_t)m_draw_max, v);
    text_cat(a, v);
    text_cat(a, " US G");                /* the CPU guard: its level now (0 off .. 5 thin), its steps */
    int_text((int32_t)cg_level(), v);
    text_cat(a, v);
    text_cat(a, " X");
    int_text((int32_t)cg_stats()->steps, v);
    text_cat(a, v);
    text_cat(c, "LATE ");
    int_text((int32_t)m_late, v);
    text_cat(c, v);
    text_cat(c, " MAX ");
    int_text((int32_t)m_render_max, v);
    text_cat(c, v);
    app_say(a, c);
#endif
}

/* the controls to the navigator: a button's press, an encoder's detents, a key down or up; the lights */
static void ui_host_input(uint32_t btn)
{
    uint32_t keys = plat_keys(), now = plat_ms(), blink, on;
    int b, e, k, save_free;
    ui_host_snap();
    save_free = !ui_control(&UI_SCREENS[ui_shown_id()], UI_CTL_BUTTON(B_SAVE));   /* (the screen it was pressed on) */
    for (b = 0; b < NB; b++)             /* down and up: a button whose action has a long press (SEQ on a part's
                                            screen) acts on release, or once held (ui_tick) */
        if ((btn ^ ui.btn) >> b & 1u)
            ui_button(&ui_hnav, b, (int)(btn >> b & 1u), now);
    if (save_free && (btn >> B_SAVE & 1u) && !(ui.btn >> B_SAVE & 1u))   /* SAVE where no screen maps it (the Song
                                            screen saves; held there, these) */
        m_readout();
    for (e = 0; e < NE; e++) {
        int32_t d = plat_enc(e);
        if (d) {
            ui_press(&ui_hnav, UI_CTL_ENCODER(e), (int)d);
            ui.redraw = ui.redraw_input = 1;
        }
    }
    for (k = 0; k < UI_KEYS; k++)
        if ((keys ^ ui_hkeys) >> k & 1u)
            ui_key(&ui_hnav, k, (int)(keys >> k & 1u), now);
    if (keys != ui_hkeys || btn != ui.btn)   /* (a key or a button: the screen may change) */
        ui.redraw = 1;
    ui_hkeys = keys;
    if (keys || btn)                     /* a key or a button held: its long press may act */
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
    b.screen = ui_shown_id();            /* (the splash while it shows) */
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

#if UI_RT
/* ---- the live state (docs/superpowers/plans/2026-11-06-device-screens.md "Live state"): the browser-remote prefix's
   04 asks, and the FM-1 answers with JSON in frames F0 7D 56 4E 14 pp nn <text> F7 (pp this frame, nn of them,
   from 0; the text 7-bit, LIVE_CHUNK bytes a frame at most), the fields of the mock's Device.state() it has, and its
   CPU. Built in the main loop when asked (app_sysex), from the snapshot the render publishes: never in the audio
   interrupt, safe while it plays. tools/fm1_state.py asks and reads it; the simulator's "live" prints the frames. */
#define LIVE_CMD 0x04u
#define LIVE_REPLY 0x14u
#define LIVE_CHUNK 100u
uint8_t live_buf[6144] UI_POOL __attribute__((aligned(4)));   /* (lent to a restore too: app.h APP_BACKUP_BUF) */
typedef struct { char *p; uint32_t n, cap; } jb_t;
static void jb(jb_t *b, const char *s)
{
    while (*s && b->n + 1u < b->cap)
        b->p[b->n++] = *s++;
    b->p[b->n] = 0;
}
static void jbi(jb_t *b, int32_t v)
{
    char t[12];
    int_text(v, t);
    jb(b, t);
}
static void jbu(jb_t *b, uint32_t v)
{
    char t[12];
    int k = 11;
    t[k] = 0;
    do
        t[--k] = (char)('0' + v % 10u);
    while (v /= 10u);
    jb(b, t + k);
}
static void jbs(jb_t *b, const char *s)  /* a JSON string: quotes and backslashes escaped, 7-bit only */
{
    char c[3] = {0, 0, 0};
    jb(b, "\"");
    for (; s && *s; s++) {
        c[0] = (char)((unsigned char)*s >= 0x20u && (unsigned char)*s < 0x7Fu ? *s : '?');
        c[1] = 0;
        if (c[0] == '"' || c[0] == '\\') {
            c[1] = c[0];
            c[0] = '\\';
        }
        jb(b, c);
    }
    jb(b, "\"");
}
static void jbk(jb_t *b, const char *k)   /* a key: ,"k": */
{
    if (b->n && b->p[b->n - 1] != '{' && b->p[b->n - 1] != '[')
        jb(b, ",");
    jbs(b, k);
    jb(b, ":");
}

static uint32_t live_state(char *o, uint32_t cap)
{
    static const char *const PART[] = {"drums", "bass", "keys", "pad", "lead", "texture"};
    static const char *const KEY[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    static const char *const MODE[6] = {"Ionian", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Aeolian"};
    jb_t b = {o, 0, cap};
    snap_t s;
    const ui_activity_t *act = ui_activity();
    const cg_stats_t *g = cg_stats();
    int i, scr = ui_hnav.depth ? ui_hnav.stack[ui_hnav.depth - 1] : UI_HOME;
    snap_read(&s);
    jb(&b, "{");
    jbk(&b, "source"); jbs(&b, "device");
    jbk(&b, "seed"); jbu(&b, s.roll.seed);
    jbk(&b, "playing"); jb(&b, s.playing ? "true" : "false");
    jbk(&b, "held"); jb(&b, s.held ? "true" : "false");
    jbk(&b, "bar"); jbi(&b, s.has_song || s.held ? s.bar : -1);
    jbk(&b, "bars"); jbi(&b, s.roll.v[RF_BARS]);
    jbk(&b, "section"); jbs(&b, s.section >= 0 ? plan_section_name(s.section) : "");
    jbk(&b, "key"); jbs(&b, KEY[(unsigned)s.roll.v[RF_KEY] % 12u]);
    jbk(&b, "mode"); jbs(&b, MODE[(unsigned)s.roll.v[RF_MODE] % 6u]);
    jbk(&b, "tempo"); jbi(&b, s.roll.v[RF_TEMPO]);
    /* the roll's values no row shows (a saved song needs them: tools/fm1_songs.py): the library set (its own copy's
       origin), the length, the motion's live move */
    jbk(&b, "set"); jbi(&b, s.set_base);
    jbk(&b, "motion_moved"); jbi(&b, s.roll.motion_moved);
    jbk(&b, "loop"); jb(&b, s.rows[ROW_LOOP] ? "true" : "false");
    jbk(&b, "stay"); jb(&b, s.rows[ROW_ENDLESS] ? "true" : "false");
    jbk(&b, "screen"); jbs(&b, UI_SCREEN_NAME[scr]);
    jbk(&b, "stack"); jb(&b, "[");
    for (i = 0; i < ui_hnav.depth; i++) {
        if (i) jb(&b, ",");
        jbs(&b, UI_SCREEN_NAME[ui_hnav.stack[i]]);
    }
    jb(&b, "]");
    jbk(&b, "cursor"); jbi(&b, ui_hnav.cursor[scr]);
    jbk(&b, "rows"); jb(&b, "{");      /* each row: its value and its name (none: a number, or the engine's own) */
    for (i = 0; i < ROW_N; i++) {
        const char *n = ch_name(i, s.rows[i]);
        jbk(&b, CH_ROW_ID[i]);
        jb(&b, "[");
        jbi(&b, s.rows[i]);
        if (n) {
            jb(&b, ",");
            jbs(&b, n);
        }
        jb(&b, "]");
    }
    jb(&b, "}");
    jbk(&b, "waiting"); jb(&b, "{");   /* the changes asked for, the bar each lands on (from 0); a jump, a re-roll */
    jbk(&b, "changes"); jb(&b, "{");
    for (i = 0; i < UI_N_FIELDS; i++)
        if (ui_hnav.req_at[i] >= 0) {
            jbk(&b, UI_STR(UI_FIELDS[i].name));
            jb(&b, "{\"value\":");
            jbi(&b, ui_hnav.req_val[i]);
            jb(&b, ",\"at\":");
            jbi(&b, ui_hnav.req_at[i]);
            jb(&b, "}");
        }
    jb(&b, "}");
    jbk(&b, "jump"); if (ui_hnav.jump_i >= 0) jbi(&b, ui_hnav.jump_start); else jb(&b, "null");
    jbk(&b, "reroll"); if (ui_hnav.roll_part >= 0) jbs(&b, PART[ui_hnav.roll_part % 6]); else jb(&b, "null");
    jb(&b, "}");
    jbk(&b, "mute"); jb(&b, "[");
    for (i = 0; i < UI_PART_TEXTURE; i++)
        if (act->mute >> i & 1u) {
            if (b.p[b.n - 1] != '[') jb(&b, ",");
            jbs(&b, PART[i]);
        }
    jb(&b, "]");
    jbk(&b, "solo"); if (act->solo >= 0) jbs(&b, PART[act->solo % 6]); else jb(&b, "null");
    jbk(&b, "locks"); jb(&b, "[");     /* the parts locked (a new song keeps them), the song line's too */
    for (i = 0; i < 7; i++)
        if (ui_hnav.locks >> i & 1u) {
            if (b.p[b.n - 1] != '[') jb(&b, ",");
            jbs(&b, i < 6 ? PART[i] : "song");
        }
    jb(&b, "]");
    jbk(&b, "levels"); jb(&b, "{");
    for (i = 0; i < VX_PART_N && i < 6; i++) {
        jbk(&b, PART[i]);
        jbi(&b, act->level[i]);
    }
    jb(&b, "}");
    jbk(&b, "cpu"); jb(&b, "{");       /* % of a block; microseconds; the guard (cpuguard.h: 0 off .. 5 thin) */
    jbk(&b, "load"); jbi(&b, (int32_t)m_load_now);
    jbk(&b, "peak"); jbi(&b, (int32_t)m_load_peak);
    jbk(&b, "late"); jbi(&b, (int32_t)m_late);
    jbk(&b, "xruns"); jbi(&b, (int32_t)plat_xruns());   /* halves the audio actually dropped (with audio_ahead, a late
                                                           block alone is caught up; two in a row drop a half) */
    jbk(&b, "redraw_us"); jbi(&b, (int32_t)m_draw_max);
    jbk(&b, "spin_us"); jbi(&b, (int32_t)m_spin_max);   /* the record's turn: a frame's label redraw, the longest */
    jbk(&b, "fade_us"); jbi(&b, (int32_t)m_fade_max);   /* a fade's step: the text's redraw, the longest */
    jbk(&b, "render_us"); jbi(&b, (int32_t)m_render_max);
    jbk(&b, "ab"); jbi(&b, ab_flags);
    jbk(&b, "ab2"); jbi(&b, ab_flags2);    /* the A/B switches (app.c AB_*: 1 guard off, 2 roll off, 4 kit render off,
                                            8 lanes off) */
    jbk(&b, "guard"); jbi(&b, cg_level());
    jbk(&b, "guard_steps"); jbi(&b, (int32_t)g->steps);   /* (since the reset: its steps up, its levels come off,
                                                              the highest, the voices it cut; the platform's blocks
                                                              over 90 %) */
    jbk(&b, "guard_recov"); jbi(&b, (int32_t)g->recov);
    jbk(&b, "guard_top"); jbi(&b, g->top);
    jbk(&b, "shed"); jbi(&b, (int32_t)(vx_shed() - m_shed0));
    jbk(&b, "over90"); jbi(&b, (int32_t)m_over90);
    jbk(&b, "late_by"); jb(&b, "{");   /* what the late blocks had going on (live_state's causes, app.c MC_*) */
    {
        static const char *const MC[MC_N] = {"song", "row", "jump", "roll", "bar", "plan", "cmd", "guard", "ramp",
                                             "step", "notes", "draw", "kit", "flash", "usb", "next", "voices",
                                             "other"};
        for (i = 0; i < MC_N; i++) {
            jbk(&b, MC[i]);
            jbi(&b, (int32_t)m_late_by[i]);
        }
    }
    jb(&b, "}");
    jbk(&b, "late_at"); jb(&b, "[");   /* the last late blocks, oldest first: [bar, step (255 none), the planning's
                                          piece (song_plans_at), us, causes (bits in late_by's order)] */
    {
        uint32_t n = m_late_n < M_LATE_AT ? m_late_n : M_LATE_AT, j;
        for (j = 0; j < n; j++) {
            uint32_t k = (m_late_n - n + j) % M_LATE_AT;
            if (j) jb(&b, ",");
            jb(&b, "[");
            jbu(&b, m_late_at[k].bar); jb(&b, ",");
            jbu(&b, m_late_at[k].step); jb(&b, ",");
            jbu(&b, m_late_at[k].piece); jb(&b, ",");
            jbu(&b, m_late_at[k].us); jb(&b, ",");
            jbu(&b, m_late_at[k].causes);
            jb(&b, "]");
        }
    }
    jb(&b, "]}");
    {                                  /* the CPU by stage since the reset (prof.h; tools/fm1_state.py's table): the
                                          time (ms), the blocks rendered and a block's time (us), each stage's us in
                                          all and in the heaviest block, that block's us, the main loop's redraws and
                                          kit voices (us in all, the render that interrupted them taken out) */
        static const char *const PFN[PF_N] = PF_NAMES;
        pf_stats_t p;
        pf_read(&p);
        jbk(&b, "prof"); jb(&b, "{");
        jbk(&b, "ms"); jbu(&b, plat_ms() - m_prof_ms);
        jbk(&b, "blocks"); jbu(&b, p.blocks);
        jbk(&b, "block_us"); jbu(&b, APP_BLOCK * 1000000u / 44100u);
        jbk(&b, "stages"); jb(&b, "[");
        for (i = 0; i < PF_N; i++) {
            if (i) jb(&b, ",");
            jbs(&b, PFN[i]);
        }
        jb(&b, "]");
        jbk(&b, "us"); jb(&b, "[");
        for (i = 0; i < PF_N; i++) {
            if (i) jb(&b, ",");
            jbu(&b, p.sum_us[i]);
        }
        jb(&b, "]");
        jbk(&b, "worst"); jb(&b, "[");
        for (i = 0; i < PF_N; i++) {
            if (i) jb(&b, ",");
            jbu(&b, p.worst_us[i]);
        }
        jb(&b, "]");
        jbk(&b, "worst_us"); jbu(&b, p.worst_total_us);
        jbk(&b, "ui_us"); jbu(&b, m_ui_us);
        jbk(&b, "kit_us"); jbu(&b, m_kit_us);
        jb(&b, "}");
    }
    jb(&b, "}");
    return b.n;
}

#if defined(PLAT_HOST) && !defined(PLAT_WEB)
static char live_sent[16384];            /* the simulator's: the frames, hex ("live" prints them) */
#endif

/* the measures back to zero (the request's 04 01: measure one event at a time): the peak, the late blocks and
   their causes, the slowest render and redraw, the guard's steps */
static void live_reset(void)
{
    m_load_peak = m_late = m_render_max = m_draw_max = m_spin_max = m_fade_max = 0;
    memset(m_late_by, 0, sizeof m_late_by);
    m_late_n = 0;
    pf_reset();
    m_ui_us = m_kit_us = 0;
    m_prof_ms = plat_ms();
    m_shed0 = vx_shed();
    m_over90 = 0;
    cg_stats_reset();
}

/* the answer, LIVE_CHUNK bytes a frame */
static void live_reply(void)
{
    uint32_t n = live_state((char *)live_buf, sizeof live_buf), parts = (n + LIVE_CHUNK - 1u) / LIVE_CHUNK, k, i;
    uint8_t f[8 + LIVE_CHUNK + 1];
    parts = parts ? parts : 1u;
    for (k = 0; k < parts && k < 128u; k++) {
        uint32_t a = k * LIVE_CHUNK, m = n - a < LIVE_CHUNK ? n - a : LIVE_CHUNK;
        f[0] = 0xF0u;
        f[1] = RM_PFX0;
        f[2] = RM_PFX1;
        f[3] = RM_PFX2;
        f[4] = LIVE_REPLY;
        f[5] = (uint8_t)k;
        f[6] = (uint8_t)parts;
        for (i = 0; i < m; i++)
            f[7 + i] = (uint8_t)(live_buf[a + i] & 0x7F);
        f[7 + m] = 0xF7u;
        plat_sysex_send(f, 8u + m);
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
        for (i = 0; i < 8u + m; i++) {
            size_t l = strlen(live_sent);
            if (l + 3 < sizeof live_sent)
                snprintf(live_sent + l, sizeof live_sent - l, "%02X", f[i]);
        }
        if (strlen(live_sent) + 2 < sizeof live_sent)
            strcat(live_sent, " ");
#endif
    }
}
#endif

#if VNL_UI_RUNTIME
/* the screen: the runtime's, or while the browser has the controls, that */
static void draw(void)
{
    if (rm.on) {
        lcd_fill(0, 0, 240, 240, C_BLACK);
        ui_draw_all();                   /* (the runtime's screen again in full once the browser lets go) */
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

#if VNL_UI_RUNTIME
static void set_theme_boot(void);
/* power-on (fm1-base's APP_SPLASH, before the engine starts): the splash, still, in the theme kept (its bands cover
   the screen); app_ui_init sets it turning */
void app_splash(void)
{
    ui_bag_t b;
    memset(&b, 0, sizeof b);
    b.s = &ui_hsnap;                     /* (empty yet: the splash's items read no song) */
    b.screen = US_SPLASH;
    set_theme_boot();
    ui_draw(&UI_SCREENS[US_SPLASH], &b, 0xFFFFFFFFu);
}
#endif

#if VNL_UI_RUNTIME
/* the settings kept in the flash (OBJ_SETTINGS): the home (Settings' HOME SCREEN: the now-playing screen, the
   default, or the main screen; a record from before it keeps the home used last), where boot lands, the theme
   (Settings' THEME: its palette + 1, 0 none; a record from before it: pad, 0, so APP_PALETTE) and the bass's
   harmonics (Settings' BASS SATURATION: bsat_off 1 off; a record from before it: pad, 0, so on). Saved when it
   changes, but only while nothing sounds (stopped or between songs, the tails over): a save erases a 4 KB sector
   with the interrupts off and the audio buffer zeroed (base's st_erase), tens of ms of silence that a playing song
   hears as a crackle (Sam, 2026-11-11, switching the homes while playing) */
#define SET_MAGIC 0x564E4C31u            /* "VNL1" */
enum { SET_HOME_MAIN, SET_HOME_NOW };
typedef struct { uint32_t magic; uint8_t home, theme, bsat_off, pad; } vnl_set_t;
static uint8_t set_home = SET_HOME_NOW, set_theme = APP_PALETTE % NPALETTES, set_bsat = 1;

static int set_read(vnl_set_t *v)
{
    return plat_store_load(OBJ_SETTINGS, v, sizeof *v) == (int)sizeof *v && v->magic == SET_MAGIC &&
           v->home <= SET_HOME_NOW;
}

static void set_theme_boot(void)         /* the theme kept (at power-on: the splash's colors) */
{
    vnl_set_t v;
    set_theme = APP_PALETTE % NPALETTES;
    if (set_read(&v) && v.theme && v.theme <= NPALETTES)
        set_theme = (uint8_t)(v.theme - 1u);
    theme_apply(set_theme);
}

static void set_load(void)
{
    vnl_set_t v;
    set_home = SET_HOME_NOW;
    set_bsat = 1;
    if (set_read(&v)) {
        set_home = v.home;
        set_bsat = v.bsat_off != 1u;
    }
    set_theme_boot();
    bass_sat = set_bsat;
    bass_sat_engine();
    ui_hnav.stack[0] = ui_hnav.home = (uint8_t)(set_home == SET_HOME_NOW ? US_NOW : US_MAIN);
}

static uint32_t set_tried = 0xFFFFFFFFu; /* the settings a failed write was for: tried again at the next change */

static void set_keep(void)               /* the home, the theme or the bass's harmonics changed: kept once all is silent */
{
    uint8_t h = ui_hnav.home == US_NOW ? SET_HOME_NOW : SET_HOME_MAIN;
    uint32_t want = (uint32_t)h << 16 | (uint32_t)theme << 8 | bass_sat;
    int same = h == set_home && theme == set_theme && bass_sat == set_bsat;
    vnl_set_t v;
    if (same) set_tried = 0xFFFFFFFFu;
    if (same || want == set_tried || !may_write()) return;
    memset(&v, 0, sizeof v);
    v.magic = SET_MAGIC;
    v.home = h;
    v.theme = (uint8_t)(theme + 1u);
    v.bsat_off = (uint8_t)!bass_sat;
    if (plat_store_save(OBJ_SETTINGS, &v, sizeof v) == 0) set_home = h, set_theme = theme, set_bsat = bass_sat;
    else set_tried = want;
}

/* backup and restore over USB (app.h): the home and the saved songs. Nothing is restored while anything sounds (a
   flash write stalls the audio); a backup first writes what waits (stopped), and nothing is written while it streams */
int app_backup_playing(void) { return ui.shown.playing || !ui.shown.silent; }
void app_backup_flush(void)
{
    set_keep();
    sv_flush(may_write());
}
void app_backup_hold(int on) { backup_held = (uint8_t)(on != 0); }
void app_backup_restored(void)
{
    set_load();
    sv_boot();
    ui_hnav.slots = sv_slots();
    ui.redraw = 1;
}
#else
int app_backup_playing(void) { return 1; }   /* (the old screen: no saved songs; no restore) */
void app_backup_flush(void) {}
void app_backup_hold(int on) { (void)on; }
void app_backup_restored(void) {}
#endif

void app_ui_init(void)
{
    ui.redraw = 1;
#if VNL_UI_RUNTIME
    ui_nav_init(&ui_hnav);               /* the home (the runtime's): now playing, until set_load reads Settings' */
    ui_hnav.eng = &UI_HENG;
    set_load();
    sv_boot();                           /* the saved songs' slots, from the flash */
    ui_hnav.slots = sv_slots();
    splash_on = 1;                       /* (app_splash drew it: the screen stays, its record starts turning) */
    splash_t0 = plat_ms();
#else
    lcd_fill(0, 0, 240, 240, C_BLACK);
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
    if (ab_flags & AB_KIT)
        return;
    int k = (s->has_song || s->held) ? s->rows[ROW_KIT] : -1;
    if (!(k >= SONG_KIT_PRESET && wb_drums_preset(&d, k - SONG_KIT_PRESET) && !wb_drums_ready(&d))) {
        int nx = mb_full ? (mb_lk.kit >= 0 ? mb_lk.kit : kit_pick(&mb_roll)) : -1;
        if (!(nx >= SONG_KIT_PRESET && wb_drums_preset(&d, nx - SONG_KIT_PRESET)))
            if (!(k >= SONG_KIT_PRESET && wb_drums_preset(&d, k - SONG_KIT_PRESET)))
                return;
    }
    wb_drums_want(&d);
    m_cause_main |= 1u << MC_KIT;
    M_SEEN(MC_KIT);
#if VNL_UI_RUNTIME
    {
        uint32_t m_t0 = M_T(), m_r0 = pf_render_us();
        wb_drums_work(KIT_SLICE, 0);
        m_kit_us += M_OWN(m_t0, m_r0);
    }
#else
    wb_drums_work(KIT_SLICE, 0);
#endif
    m_cause_main &= ~(1u << MC_KIT);
}

void app_frame(void)
{
    snap_t s;
    uint32_t now = plat_ms();
    if (!seeded)
        seed_boot();
#if UI_RT
    lk_frame();
#endif
    roll_next();
    snap_read(&s);
#if UI_RT
    kit_render(&s);
    {                                    /* the Parts roll shown: a screen with lanes */
        int scr = ui_hnav.depth ? ui_hnav.stack[ui_hnav.depth - 1] : UI_HOME, bi, ii, on = 0;
        const ui_screen_t *sc = &UI_SCREENS[scr];
        for (bi = 0; bi < sc->n_bands && !on; bi++)
            for (ii = 0; ii < UI_BANDS[sc->bands + bi].n_items; ii++)
                on |= UI_ITEMS[UI_BANDS[sc->bands + bi].first_item + ii].kind == UI_LANES;
        roll_shown = (uint8_t)on;
    }
#endif
    if (s.has_song != ui.shown.has_song || s.stopped != ui.shown.stopped || s.ready != ui.shown.ready
        || s.held != ui.shown.held || s.bar != ui.shown.bar
        || s.section != ui.shown.section || s.pend_field != ui.shown.pend_field || s.pend_bars != ui.shown.pend_bars
        || memcmp(&s.roll, &ui.shown.roll, sizeof s.roll) || memcmp(s.req_val, ui.shown.req_val, sizeof s.req_val)
#if UI_RT
        || s.changes != ui.shown.changes || memcmp(s.rows, ui.shown.rows, sizeof s.rows)   /* (a row landed: an
                                            instrument, an override, the kit: shown at once, not at the next bar) */
#endif
        )
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
#if VNL_UI_RUNTIME
    if ((splash_on || splash_fading) && (ui.btn || plat_keys() || ui.redraw_input))   /* a control pressed */
        splash_end();
    ui.redraw_input = 0;
    set_keep();
    if (sv_flush(may_write()) > 0)        /* a save made while the music played: written now all is silent */
        ui.redraw = 1;
#endif
    speak();
    if (ui.msg_until && now >= ui.msg_until) {
        ui.msg_until = 0;
        ui.redraw = 1;
    }
#if UI_RT
    {                                    /* the record turns: its label alone, unless the screen is drawn anyway; a
                                            fade's step: the text alone */
        uint8_t was = ui_hnav.spin, was_fade = ui_hnav.fade, was_on = splash_on;
        spin_tick(now);
        if ((ui_hnav.fade != was_fade || splash_on != was_on) && !ui.redraw && !rm.on && ui_host >= 0) {
            uint32_t m_t0 = M_T(), m_r0 = pf_render_us(), dt;
            ui_host_text();
            dt = M_DT(m_t0);
            if (dt < M_SANE_US && dt > m_fade_max)
                m_fade_max = dt;
            m_ui_us += M_OWN(m_t0, m_r0);
        }
        if (ui_hnav.spin != was && !ui.redraw && !rm.on && ui_host >= 0) {
            uint32_t m_t0 = M_T(), m_r0 = pf_render_us(), dt;
            ui_record_turn(&UI_SCREENS[ui_shown_id()], ui_hnav.spin);
            dt = M_DT(m_t0);
            if (dt < M_SANE_US && dt > m_spin_max)
                m_spin_max = dt;
            m_ui_us += M_OWN(m_t0, m_r0);
        }
    }
#endif
    if (ui.redraw) {
#if VNL_UI_RUNTIME
        uint32_t m_t0 = M_T(), m_r0 = pf_render_us(), dt;
        m_cause_main |= 1u << MC_DRAW;
        M_SEEN(MC_DRAW);
#endif
        ui.redraw = 0;
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
        if (ui_host >= 0)
            ui_host_draw();
        else
#endif
            draw();
#if VNL_UI_RUNTIME
        m_cause_main &= ~(1u << MC_DRAW);
        dt = M_DT(m_t0);                 /* (the FM-1's too: before, only the simulator's was timed) */
        if (dt < M_SANE_US && dt > m_draw_max)
            m_draw_max = dt;
        m_ui_us += M_OWN(m_t0, m_r0);
#endif
    }
}

#ifdef PLAT_HOST
static int32_t rem_bar = -1;             /* "remember": the song shown then (its seed) and its bar */
static uint32_t rem_seed;
static song_roll_t rem_roll;             /* (and its roll, the set its library one: expect same_roll) */
#if UI_RT
static int16_t rem_rows[ROW_N];          /* (and its rows, "as rolled" as they played: expect same_<part>,
                                            parts_moved) */
/* a part's rows (LK_ROWS') as the song shown plays them against the ones remembered: 1 all alike */
static int part_same(const snap_t *s, int part)
{
    int16_t now[ROW_N];
    int i, p = 0, same = 1;
    lk_resolve(s, 0x7F, now);
    for (i = 0; i < (int)sizeof LK_ROWS; i++) {
        if (LK_ROWS[i] < 0) p++;
        else if (p == part && now[LK_ROWS[i]] != rem_rows[LK_ROWS[i]]) same = 0;
    }
    return same;
}
#endif
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
    else if (!strcmp(what, "home"))     /* the home shown (1: the main or now-playing screen in use) or another (0) */
        *got = ui_hnav.stack[ui_hnav.depth - 1] == ui_hnav.home;
    else if (!strcmp(what, "home_now"))  /* the home is the now-playing screen (1) or the main screen (0) */
        *got = ui_hnav.home == US_NOW;
    else if (!strcmp(what, "home_kept"))  /* the home in the flash is the one in use (0: its save waits for silence) */
        *got = (ui_hnav.home == US_NOW ? SET_HOME_NOW : SET_HOME_MAIN) == set_home;
    else if (!strcmp(what, "theme"))     /* the theme: base's palette (Settings' THEME) */
        *got = theme;
    else if (!strcmp(what, "theme_kept"))   /* the theme in the flash is the one in use (0: its save waits) */
        *got = theme == set_theme;
    else if (!strcmp(what, "bass_sat"))  /* Settings' BASS SATURATION: 1 on, 0 off */
        *got = bass_sat;
    else if (!strcmp(what, "bass_sat_kept"))   /* the bass saturation in the flash is the one in use (0: it waits) */
        *got = bass_sat == set_bsat;
    else if (!strcmp(what, "bass_sat_live"))   /* what the bass plays with now (the engine's: a fresh note takes it) */
        *got = vx_bass_sat_live();
#ifdef PLAT_HOST
    else if (!strcmp(what, "bass_shaped"))     /* the bass's harmonics ran since the last asked (1) or not (0) */
        *got = vx_bass_shaped() > 0u;
#endif
    else if (!strcmp(what, "splash"))    /* the boot splash showing */
        *got = splash_on;
    else if (!strcmp(what, "key_lit")) {   /* the keys lit, bit k key k (blinking ones not counted) */
        uint32_t blink;
        *got = (int)ui_key_lights(&ui_hnav, &s, &blink);
    }
#endif
    else if (!strcmp(what, "voices"))
        *got = s.voices;
#if VNL_UI_RUNTIME
    else if (!strcmp(what, "guard"))     /* the CPU guard: its level now (cpuguard.h CG_*), the highest since the live
                                            state's reset, the levels come off, the voices cut; the late blocks */
        *got = cg_level();
    else if (!strcmp(what, "guard_top"))
        *got = cg_stats()->top;
    else if (!strcmp(what, "guard_recov"))
        *got = (int)cg_stats()->recov;
    else if (!strcmp(what, "shed"))
        *got = (int)(vx_shed() - m_shed0);
    else if (!strcmp(what, "late"))
        *got = (int)m_late;
#endif
#if UI_RT
    else if (!strncmp(what, "same_", 5) && strcmp(what, "same_song") && strcmp(what, "same_roll")) {
        /* same_drums .. same_texture, same_songline: the part's rows as remembered (1), not (0) */
        static const char *const PN[] = {"drums", "bass", "comp", "pad", "lead", "texture", "songline"};
        int p;
        for (p = 0; p < 7 && strcmp(what + 5, PN[p]); p++) {}
        if (p == 7) return 0;
        *got = part_same(&s, p);
    } else if (!strcmp(what, "parts_moved")) {   /* the unlocked parts whose rows differ from the ones remembered */
        int p;
        *got = 0;
        for (p = 0; p < 7; p++)
            *got += !(ui_hnav.locks >> p & 1u) && !part_same(&s, p);
    } else if (!strcmp(what, "locks"))   /* the parts locked, bit UI_PART_* */
        *got = ui_hnav.locks;
    else if (!strcmp(what, "saved")) {   /* the saved songs' slots holding a song */
        int k;
        *got = 0;
        for (k = 0; k < UI_SLOTS; k++)
            *got += sv_slots()[k].state != UI_SLOT_EMPTY;
    } else if (!strcmp(what, "save_waits"))   /* a save or delete waits for silence to be written (1) */
        *got = sv_waiting();
#endif
    else if (!strcmp(what, "remote"))
        *got = rm.on;
    else if (!strcmp(what, "same_song"))   /* the song playing or ready is the one remembered (1), another (0); none:
                                              -1 */
        *got = (s.has_song || s.ready) && rem_bar >= 0 ? s.roll.seed == rem_seed : -1;
    else if (!strcmp(what, "same_roll")) {   /* its roll is the one remembered: seed, every rolled value, the set */
        song_roll_t r = s.roll;
        r.v[RF_SET] = s.set_base;
        *got = (s.has_song || s.ready) && rem_bar >= 0 ? !memcmp(&r, &rem_roll, sizeof r) : -1;
    } else if (!strcmp(what, "bars_on"))     /* the bar playing (or held), less the one remembered (the same song) */
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
 *   remember                              the song playing and its bar, for "expect same_song" and "bars_on", and
 *                                         its rows, for "expect same_<part>" and "parts_moved" (tests/scenarios/locks.vnl) */
static int rm_fails;
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d)
{
    (void)c, (void)d;
    if (!strcmp(cmd, "uiscreen")) {     /* uiscreen ID: the UI runtime shows that screen (workbench/ui/screens.js's id) */
        int i;
        uint8_t home = ui_hnav.home;     /* (Settings' HOME SCREEN as it is) */
        for (i = 0; i < UI_N_SCREENS && strcmp(UI_SCREEN_NAME[i], a); i++) {}
        if (i == UI_N_SCREENS) {
            printf("uiscreen: no screen %s\n", a);
            rm_fails++;
            return 1;
        }
        ui_host = i;
        splash_on = splash_fading = 0;
        ui_nav_init(&ui_hnav);
        ui_hnav.home = home;
        ui_hnav.eng = &UI_HENG;
        ui_hnav.slots = sv_slots();
        ui_hnav.stack[0] = (uint8_t)i;
        ui_hkeys = plat_keys();
        lcd_fill(0, 0, 240, 240, C_BLACK);
        ui_draw_all();
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
            for (k = 0; k < 50; k++) {
                ui_draw_all();           /* (a full draw: every band sent) */
                ui_host_draw();
            }
            us = (int)((clock() - t0) * 1000000.0 / CLOCKS_PER_SEC / 50);
            printf("uibench %s %d us\n", UI_SCREEN_NAME[i], us);
            if (us > worst)
                worst = us, at = i;
        }
        printf("uibench worst %s %d us (the host's)\n", UI_SCREEN_NAME[at], worst);
        {                                /* the record's turn: its label's box alone (ui_record_turn) */
            clock_t t0 = clock();
            for (k = 0; k < 256; k++)
                ui_record_turn(&UI_SCREENS[US_NOW], k);
            printf("uibench spin %d us (a turn's frame, the host's)\n", (int)((clock() - t0) * 1000000.0 / CLOCKS_PER_SEC / 256));
            t0 = clock();                /* the main screen's, small in its top band */
            for (k = 0; k < 256 * 16; k++)
                ui_record_turn(&UI_SCREENS[US_MAIN], k & 255);
            printf("uibench spin-main %.2f us (a turn's frame, the host's)\n",
                   (clock() - t0) * 1000000.0 / CLOCKS_PER_SEC / (256 * 16));
            t0 = clock();
            ui_hnav.stack[ui_hnav.depth - 1] = (uint8_t)US_NOW;
            for (k = 0; k < 64; k++) {
                ui_hnav.fade = (uint8_t)(k & 15);
                ui_host_text();
            }
            ui_hnav.fade = 0;
            printf("uibench fade %d us (a fade's step, the host's)\n", (int)((clock() - t0) * 1000000.0 / CLOCKS_PER_SEC / 64));
        }
        ui_hnav.stack[ui_hnav.depth - 1] = (uint8_t)keep;
        return 1;
    }
    if (!strcmp(cmd, "uifade")) {        /* uifade N: the screen's text redrawn alone at {fade} N (a fade's step) */
        ui_hnav.fade = (uint8_t)atoi(a);
        if (ui_host >= 0) ui_host_text();
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
    if (!strcmp(cmd, "heavy")) {         /* heavy N [off]: each block N us of busy work a sounding voice, and the CPU
                                            guard on as on the FM-1, on the host's clock (off: the guard off, as the
                                            simulator boots) */
        heavy_us = (uint32_t)atoi(a);
        cg_on(strcmp(b, "off") != 0, 240u, 204u, 256u);
        cg_need_late(2);
        return 1;
    }
    if (!strcmp(cmd, "live")) {          /* live [reset]: the live state asked for as over USB (F0 7D 56 4E 04 [01] F7);
                                            its frames, hex (tools/fm1_state.py's test reads them) */
        static const uint8_t ASK[5] = {RM_PFX0, RM_PFX1, RM_PFX2, 0x04u, 0x01u};
        live_sent[0] = 0;
        app_sysex(ASK, !strcmp(a, "reset") ? 5u : 4u);
        printf("live %s\n", live_sent);
        return 1;
    }
    if (!strcmp(cmd, "remember")) {
        snap_t s;
        snap_read(&s);
        rem_bar = s.has_song ? s.bar : -1;
        rem_seed = s.roll.seed;
        rem_roll = s.roll;
        rem_roll.v[RF_SET] = s.set_base;
#if UI_RT
        lk_resolve(&s, 0x7F, rem_rows);
#endif
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

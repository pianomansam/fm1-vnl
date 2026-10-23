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
 * Controls (minimal; the real UI comes later): PLAY stops / plays (a stop is true silence; play after a stop is a
 * fresh song), K1 key +-1 semitone, K2 tempo +-1 BPM, K3 motion +-5, K4 mood +-1: each a request that lands on the
 * song's next 4-bar boundary. Part of the unity build (device/unity.c, host/plat_host.c), after the platform:
 * gfx.c and announce.c are in scope. */
#include <string.h>
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
#include <stdlib.h>
#endif
#include "song.h"
#include "harmony.h"
#include "theory.h"
#include "library.h"
#include "voices.h"

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

static const char *const KEY_NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
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

/* ------------------------------------------------------- main -> render --- */
/* the next song, rolled by the main loop: full until the render takes it */
static song_roll_t mb_roll;
static volatile uint8_t mb_full;

/* commands, drained at a block's start (AMB-1's pattern) */
enum { C_PLAY, C_REQ };
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
#define SLOG_TIME(n) (slog_samples += (n))
#else
#define SLOG_SONG(s) ((void)0)
#define SLOG_STEP(s, bar, st, ev, n) ((void)0)
#define SLOG_GAP(secs) ((void)0)
#define SLOG_TIME(n) ((void)0)
#endif

/* ------------------------------------------------------------- render --- */
enum { RS_STOP, RS_WAIT, RS_SONG, RS_GAP };   /* stopped; playing: waiting for a song, in one, between two */
static song_t song;
static uint8_t rs = RS_WAIT;
static volatile uint8_t panic_now;
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

static void song_begin(void)
{
    song_start(&song, &mb_roll);
    BARRIER();
    mb_full = 0;                         /* the main loop rolls the next one */
    if (vx_active())
        vx_set_gain(1.0f);
    else
        vx_init();
    r_now = r_pos = r_tick = 0;
    r_frac = 0.0f;
    memset(req_set, 0, sizeof req_set);
    songs_started++;
    rs = RS_SONG;
    SLOG_SONG(&song);
}

/* one 16th: the fade (if the song's last block fades), the requests' slots, the step's events to the voices */
static void step(void)
{
    const plan_block_t *last = &song.plan.b[song.plan.n ? song.plan.n - 1 : 0];
    int bar = song.bar, st = (int)(r_tick & 15u), n, i;
    float adv;
    uint32_t ip;
    if (song.plan.n && (last->flags & PB_FADE) && bar >= last->start) {
        int len = last->bars * 16, at = (bar - last->start) * 16 + st + 1;
        vx_set_gain(1.0f - (float)at / (float)len);
    }
    if (st == 0 && bar % 4 == 0)
        memset(req_set, 0, sizeof req_set);   /* every waiting request lands on this boundary */
    n = song_tick(&song, evs, 64);
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
    if (op == C_PLAY) {
        if (rs == RS_STOP)
            rs = RS_WAIT;                /* a fresh song from the mailbox */
        else {
            vx_release_all();
            rs = RS_STOP;
        }
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
    snap.pend_field = f;
    snap.pend_bars = bars;
    BARRIER();
    snap_seq++;
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    uint32_t i = 0, k;
    float gain = (float)master_q12 * (8388607.0f / 4096.0f);   /* MASTER, read once a block: float -> 24-bit */
    if (panic_now) {
        panic_now = 0;
        vx_init();
        rs = RS_STOP;
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
                else {                   /* the silence between songs: 2-4 s, from the song's seed */
                    uint32_t h = song.roll.seed * 2654435761u;
                    vx_release_all();
                    gap_left = (2000u + (h >> 16) % 2001u) * 441u / 10u;
                    SLOG_GAP((float)gap_left / 44100.0f);
                    rs = RS_GAP;
                }
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
        if (rs == RS_SONG || vx_active())   /* true silence once the last release is over */
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
    last_roll = mb_roll;
    rolled_any = 1;
    next_seed = seed_next(next_seed);
    BARRIER();
    mb_full = 1;
}

void app_init(void)
{
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
    {                                    /* exploring voicing families in renders: VNL_DROP2=1 */
        const char *d = getenv("VNL_DROP2");
        th_set_drop2(d && *d == '1');
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
static void value_text(int f, int v, char *t)
{
    t[0] = 0;
    if (f == RF_KEY)
        text_cat(t, KEY_NAME[v % 12]);
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
    text_cat(t, KEY_NAME[s->roll.v[RF_KEY] % 12]);
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
        th_chord_name(pcs, np, t);
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
        value_text(s->pend_field, s->req_val[s->pend_field], n);
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
        if (!ui.shown.has_song)
            text_cat(t, ui.shown.stopped ? "STOPPED" : "NEXT SONG");
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

static void input(void)
{
    uint32_t btn = plat_buttons();
    int32_t e;
    int k;
    if ((btn >> B_PLAY & 1u) && !(ui.btn >> B_PLAY & 1u)) {
        post(C_PLAY, 0, 0);
        app_say(ui.shown.stopped ? "PLAY" : "STOP", "");
    }
    for (k = 0; k < 4; k++) {               /* K1..K4: a request, by steps (the render adds them up) */
        int f = KNOB_FIELD[k];
        char t[12];
        if ((e = plat_enc(EN_K1 + k)) == 0)
            continue;
        post(C_REQ, f, (int)e * KNOB_STEP[k]);
        if (ui.shown.has_song) {            /* what it will be, as far as the main loop can tell */
            int v = ui.shown.req_set[f] ? ui.shown.req_val[f] : ui.shown.roll.v[f];
            value_text(f, field_step(f, v, (int)e * KNOB_STEP[k]), t);
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
        if (s->has_song) {
            line_key(s, t);
            cv_text(230 - text_w(&FONT_XS, t), 10, &FONT_XS, t, C_HI);
        }
    }
    cv_blit(0, 0);
    /* the song: where it is, the chord, the change on its way */
    cv_begin(240, 120, C_BLACK);
    if (s->has_song) {
        line_place(s, t);
        cv_text(10, 4, &FONT_XS, t, C_GRAY);
        line_chord(s, t);
        cv_text(10, 34, &FONT_B, t, C_WHITE);
        line_pending(s, t);
        cv_text(10, 74, &FONT_XS, t, C_HI);
    } else {
        cv_text(10, 34, &FONT_B, s->stopped ? "STOPPED" : "NEXT SONG", C_GRAY);
    }
    cv_blit(0, 40);
    /* the four knobs and their values */
    cv_begin(240, 40, C_BLACK);
    for (k = 0; k < 4; k++) {
        int32_t x = 4 + k * 58;
        cv_text(x, 2, &FONT_XS, KNOB_NAME[k], C_GRAY);
        if (s->has_song) {
            value_text(KNOB_FIELD[k], s->roll.v[KNOB_FIELD[k]], t);
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
    if (s.has_song != ui.shown.has_song || s.stopped != ui.shown.stopped || s.bar != ui.shown.bar
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
int app_expect(const char *what, int *got)
{
    snap_t s;
    snap_read(&s);
    if (!strcmp(what, "playing"))
        *got = s.playing;
    else if (!strcmp(what, "songs"))
        *got = s.songs;
    else if (!strcmp(what, "bar"))
        *got = s.has_song ? s.bar : -1;
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
    else
        return 0;
    return 1;
}
#endif


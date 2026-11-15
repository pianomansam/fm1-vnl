/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The device's screens runtime on the host (app/ui): the value bag against the mock's values in the same states
   (ui_values_fixture.h, workbench/tools/ui_fixture.mjs); the navigator (nav.c) against the mock's Device's rules. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "values.h"
#include "screens_gen.h"
#include "choices_gen.h"
#include "song.h"
#include "plan.h"
#include "override.h"
#include "voices.h"
#include "sthold.h"

typedef struct { const char *name; int kind; const char *str; double num; } ui_fx_val_t;
typedef struct { int field, val, at; } ui_fx_req_t;
typedef struct {
    const char *name;
    int screen, playing, cursor, nreq;
    ui_fx_req_t req[2];
    const ui_fx_val_t *vals;
    int n;
} ui_fx_case_t;
#include "ui_values_fixture.h"

static int fails;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        if (!(c)) {                                                                                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                            \
            printf(__VA_ARGS__);                                                                                   \
            printf("\n");                                                                                          \
            fails++;                                                                                               \
        }                                                                                                          \
    } while (0)

/* a value id by name (the first: a name in two families is the first's, as gen.py numbers them) */
static int value_id(const char *name)
{
    int i;
    for (i = 1; i < UI_N_VALUES; i++)
        if (!strcmp(UI_VALUE_STR(i), name)) return i;
    return 0;
}

/* the snapshot the render would publish for seed, at bar (from 0), playing or stopped with the song ready */
static void snap_for(snap_t *s, uint32_t seed, int bar, int playing)
{
    song_roll_t r;
    song_plan_t p;
    const plan_block_t *b;
    int i;
    memset(s, 0, sizeof *s);
    song_override(NULL);
    roll_song(NULL, seed, &r);
    s->roll = r;
    s->playing = (uint8_t)playing;
    s->stopped = (uint8_t)!playing;
    s->has_song = (uint8_t)playing;
    s->ready = (uint8_t)!playing;
    s->bar = playing ? bar : 0;
    plan_build(&r, &p);
    b = plan_at(&p, s->bar);
    s->section = playing && b ? b->section : -1;
    s->flags = b ? b->flags : 0;
    s->pend_field = s->pend_bars = -1;
    for (i = 0; i < p.n && i < SNAP_SECS; i++) {
        s->secs[i].start = p.b[i].start;
        s->secs[i].bars = p.b[i].bars;
        s->secs[i].section = p.b[i].section;
    }
    s->n_secs = (uint8_t)i;
    ui_song_rows(&r, s->rows);
}

/* every value the mock's bag has in the fixture's states: the same text, truthiness, fraction */
static void test_values_fixture(void)
{
    unsigned c;
    for (c = 0; c < sizeof UI_FX_CASES / sizeof UI_FX_CASES[0]; c++) {
        const ui_fx_case_t *x = &UI_FX_CASES[c];
        snap_t s;
        ui_nav_t n;
        ui_bag_t b;
        int i, bad = 0;
        snap_for(&s, UI_FX_SEED, UI_FX_BAR, x->playing);
        ui_nav_init(&n);
        n.stack[0] = (uint8_t)x->screen;
        n.cursor[x->screen] = (int8_t)x->cursor;
        for (i = 0; i < x->nreq; i++) {
            n.req_val[x->req[i].field] = (int16_t)x->req[i].val;
            n.req_at[x->req[i].field] = (int16_t)x->req[i].at;
        }
        ui_values(&s, &n, &b);
        for (i = 0; i < x->n; i++) {
            const ui_fx_val_t *v = &x->vals[i];
            int id = value_id(v->name);
            char t[UI_VALUE_MAX];
            if (!id) {
                CHECK(0, "%s: no value %s on the device", x->name, v->name);
                continue;
            }
            if (v->kind == 0) {
                const char *got = ui_value(&b, id, 0, t);
                if (strcmp(got, v->str)) bad++;
                CHECK(!strcmp(got, v->str), "%s: {%s} is \"%s\", the mock's \"%s\"", x->name, v->name, got, v->str);
            } else if (v->kind == 1) {
                int got = ui_true(&b, id);
                if (got != (v->num != 0)) bad++;
                CHECK(got == (v->num != 0), "%s: %s is %d, the mock's %d", x->name, v->name, got, v->num != 0);
            } else {
                int den, num = ui_value_num(&b, id, &den);
                double got = (double)num / den;
                if (fabs(got - v->num) > 1e-12) bad++;
                CHECK(fabs(got - v->num) <= 1e-12, "%s: %s is %.17g, the mock's %.17g", x->name, v->name, got, v->num);
            }
        }
        (void)bad;
    }
}

/* a change waiting: the value at once, marked; the line saying when it lands */
static void test_values_pending(void)
{
    snap_t s;
    ui_nav_t n;
    ui_bag_t b;
    char t[UI_VALUE_MAX];
    snap_for(&s, UI_FX_SEED, 13, 1);
    ui_nav_init(&n);
    n.req_val[UFLD_TEMPO] = 86;
    n.req_at[UFLD_TEMPO] = 16;
    ui_values(&s, &n, &b);
    CHECK(!strcmp(ui_value(&b, UV_FIELD(UFLD_TEMPO, UVK_FIELD_VALUE), 0, t), "*86"), "tempo: %s", t);
    CHECK(ui_true(&b, UV_FIELD(UFLD_TEMPO, UVK_FIELD_PEND)), "tempoPend");
    CHECK(!strcmp(ui_value(&b, UV_PENDING, 0, t), "TEMPO 86 IN 3 BARS"), "pending: %s", t);
}

/* Review Focus 2: a stale or unknown choice (a kit with no name) shows as its number */
static void test_values_unknown_choice(void)
{
    snap_t s;
    ui_nav_t n;
    ui_bag_t b;
    char t[UI_VALUE_MAX];
    snap_for(&s, UI_FX_SEED, 13, 1);
    s.rows[ROW_KIT] = 99;
    s.rows[ROW_LEAD_STYLE] = -7;
    ui_nav_init(&n);
    ui_values(&s, &n, &b);
    CHECK(!strcmp(ui_value(&b, UV_FIELD(UFLD_DRUM_KIT, UVK_FIELD_VALUE), 0, t), "99"), "{drumKit}: %s", t);
    CHECK(!strcmp(ui_value(&b, value_id("kit"), 0, t), "99"), "{kit}: %s", t);
    CHECK(!strcmp(ui_value(&b, UV_FIELD(UFLD_LEAD_STYLE_SET, UVK_FIELD_VALUE), 0, t), "-7"), "{leadStyleSet}: %s", t);
    CHECK(ui_true(&b, UV_FIELD(UFLD_DRUM_KIT, UVK_FIELD_LAST)), "an unknown value is at its list's end");
}

/* ---- the navigator (nav.c): the mock's Device, its calls on the engine logged ---- */
static char eng_log[2048];
static int eng_muted[UI_PART_TEXTURE + 1], eng_solo = -1, eng_level[UI_PART_TEXTURE + 1];
static void logf_(const char *what, int a, int b)
{
    char t[48];
    snprintf(t, sizeof t, "%s%s %d %d", eng_log[0] ? "; " : "", what, a, b);
    strncat(eng_log, t, sizeof eng_log - strlen(eng_log) - 1);
}
static void e_set(int row, int v) { logf_("set", row, v); }
static void e_play(void) { logf_("play", 0, 0); }
static void e_reroll(void) { logf_("reroll", 0, 0); }
static void e_reroll_part(int p) { logf_("reroll_part", p, 0); }
static void e_jump(int start, int i) { logf_("jump", start, i); }
static void e_mute(int p, int on) { eng_muted[p] = on; logf_("mute", p, on); }
static void e_solo(int p) { eng_solo = p; logf_("solo", p, 0); }
static int e_muted(int p) { return eng_muted[p]; }
static int e_soloed(void) { return eng_solo; }
static void e_level(int p, int db) { eng_level[p] = db; logf_("level", p, db); }
static int e_level_get(int p) { return eng_level[p]; }
static void e_stutter(int on) { logf_("stutter", on, 0); }
static const ui_engine_t ENG = {e_set, e_play, e_reroll, e_reroll_part, e_jump, e_mute, e_solo, e_muted, e_soloed,
                                e_level, e_level_get, 0, 0, 0, 0, 0, 0, 0,   /* (saved songs, settings: none) */
                                e_stutter};

static void nav_fresh(ui_nav_t *n, snap_t *s, int bar)
{
    snap_for(s, UI_FX_SEED, bar, 1);
    ui_nav_init(n);
    n->stack[0] = n->home = (uint8_t)US_MAIN;   /* (Settings' HOME SCREEN DETAILED: the app's, set_load) */
    n->eng = &ENG;
    eng_log[0] = 0;
    memset(eng_muted, 0, sizeof eng_muted);
    memset(eng_level, 0, sizeof eng_level);
    eng_solo = -1;
    ui_update(n, s, 0);
}
static int shown(const ui_nav_t *n) { return n->stack[n->depth - 1]; }

/* ALGO 3, EDIT, K1 +1, OCT+, SELECT, ALGO 1, HOME (the plan's nav_main_to_comp) */
static void test_nav_main_to_comp(void)
{
    ui_nav_t n;
    snap_t s;
    char t[UI_VALUE_MAX];
    ui_bag_t b;
    nav_fresh(&n, &s, 2);
    ui_press(&n, UC_IN_ALGO, 3);
    CHECK(n.cursor[US_MAIN] == 3, "ALGO 3 highlights Comp (%d)", n.cursor[US_MAIN]);
    ui_press(&n, UC_IN_EDIT, 1);
    CHECK(shown(&n) == US_P_COMP && n.depth == 2, "EDIT opens Comp (%d, depth %d)", shown(&n), n.depth);
    CHECK(strstr(n.said, "ACCOMP, PAGE 1/2") == n.said, "the reader hears Comp: %s", n.said);
    ui_press(&n, UC_IN_K1, 1);
    CHECK(n.req_at[UFLD_COMP_INS] == 4, "K1 asks for another comp instrument at bar 4 (%d)", n.req_at[UFLD_COMP_INS]);
    ui_press(&n, UC_IN_OCT_PLUS, 1);
    CHECK(n.req_at[UFLD_COMP_REG] == 4 && n.req_val[UFLD_COMP_REG] == 1, "OCT+ asks for the register up (%d at %d)",
          n.req_val[UFLD_COMP_REG], n.req_at[UFLD_COMP_REG]);
    ui_values(&s, &n, &b);
    CHECK(!strcmp(ui_value(&b, UV_FIELD(UFLD_COMP_REG, UVK_FIELD_VALUE), 0, t), "*UP AN OCTAVE"), "{compReg}: %s", t);
    CHECK(!strcmp(n.msg[0], "REGISTER") && !strcmp(n.msg[1], "UP AN OCTAVE"), "the header: %s %s", n.msg[0], n.msg[1]);
    ui_press(&n, UC_IN_SELECT, 1);
    CHECK(shown(&n) == US_P_COMP_MORE, "SELECT: Comp's page 2 (%d)", shown(&n));
    ui_press(&n, UC_IN_ALGO, 1);
    CHECK(shown(&n) == US_P_LEAD && n.depth == 2, "ALGO: the next part, Lead, at its first page (%d)", shown(&n));
    ui_press(&n, UC_IN_HOME, 1);
    CHECK(shown(&n) == US_MAIN && n.depth == 1, "HOME: the main screen");
    CHECK(!strstr(eng_log, "set"), "nothing sent before the bar before its boundary: %s", eng_log);
}

/* K2 +2 at bar 2: waits, sent in bar 3 once, done at bar 4; turning back undoes (sent: set back) */
static void test_nav_pending(void)
{
    ui_nav_t n;
    snap_t s;
    int tempo;
    nav_fresh(&n, &s, 2);
    tempo = s.roll.v[RF_TEMPO];
    ui_press(&n, UC_IN_K2, 2);
    CHECK(n.req_at[UFLD_TEMPO] == 4 && n.req_val[UFLD_TEMPO] == tempo + 2, "K2 +2: tempo %d at 4 (%d at %d)", tempo + 2,
          n.req_val[UFLD_TEMPO], n.req_at[UFLD_TEMPO]);
    s.bar = 3;
    ui_update(&n, &s, 100);
    ui_update(&n, &s, 200);
    {
        char want[32];
        snprintf(want, sizeof want, "set %d %d", ROW_TEMPO, tempo + 2);
        CHECK(!strcmp(eng_log, want), "sent once in bar 3: %s", eng_log);
    }
    ui_press(&n, UC_IN_K2, -2);
    CHECK(n.req_at[UFLD_TEMPO] < 0, "turned back to what plays: nothing waits");
    CHECK(strstr(eng_log, "; set") != NULL, "and the one sent is set back: %s", eng_log);
    s.bar = 5;
    ui_update(&n, &s, 300);
    ui_press(&n, UC_IN_K2, 1);
    CHECK(n.req_at[UFLD_TEMPO] == 8, "at bar 5 a change waits for bar 8 (%d)", n.req_at[UFLD_TEMPO]);
    {   /* the landing (Sam, 2026-11-07: the old value flashed between the pending one and the new): at the boundary
           the song plays the change a 16th later; until its row shows it the pending value stays, so no frame shows
           the old one */
        ui_bag_t bg;
        char t[UI_VALUE_MAX], want[UI_VALUE_MAX];
        int k, v = n.req_val[UFLD_TEMPO], old = s.rows[ROW_TEMPO];
        ui_values(&s, &n, &bg);
        snprintf(want, sizeof want, "%s", ui_value(&bg, UV_ROW(ROW_TEMPO, UVK_ROW_PEND), 0, t));
        for (k = 0; k < 3; k++) {         /* bar 8, before the engine has it (rows still the old value) */
            s.bar = 8;
            s.rows[ROW_TEMPO] = (int16_t)old;
            ui_update(&n, &s, 400u + (uint32_t)k);
            ui_values(&s, &n, &bg);
            CHECK(n.req_at[UFLD_TEMPO] == 8 && !strcmp(ui_value(&bg, UV_ROW(ROW_TEMPO, UVK_ROW_PEND), 0, t), want),
                  "the landing bar, the row not there yet: still the pending %s (%s)", want, t);
        }
        s.rows[ROW_TEMPO] = (int16_t)v;   /* the engine plays it */
        ui_update(&n, &s, 410);
        CHECK(n.req_at[UFLD_TEMPO] < 0, "the row shows it: nothing waits");
        ui_press(&n, UC_IN_K2, 1);        /* a change the engine never plays (refused): gone a bar past its boundary */
        s.bar = 12;
        ui_update(&n, &s, 420);
        CHECK(n.req_at[UFLD_TEMPO] == 12, "not played at its boundary: it waits (%d)", n.req_at[UFLD_TEMPO]);
        s.bar = 13;
        ui_update(&n, &s, 430);
        CHECK(n.req_at[UFLD_TEMPO] < 0, "a bar past its boundary nothing waits");
    }
}

/* the pending line: the earliest boundary, a tie the first made */
static void test_nav_pending_order(void)
{
    ui_nav_t n;
    snap_t s;
    ui_bag_t b;
    char t[UI_VALUE_MAX];
    nav_fresh(&n, &s, 2);
    ui_press(&n, UC_IN_K2, 1);   /* tempo first */
    ui_press(&n, UC_IN_K4, -1);  /* then mood, the same boundary (a lower field: not what decides) */
    ui_press(&n, UC_IN_K2, 1);   /* tempo again: it keeps its place */
    ui_values(&s, &n, &b);
    CHECK(!strncmp(ui_value(&b, UV_PENDING, 0, t), "TEMPO", 5), "the first made: %s", t);
}

/* Review Focus 3: stopped, between songs, past the last boundary: nothing waits; the header still says it */
static void test_nav_stopped(void)
{
    ui_nav_t n;
    snap_t s;
    nav_fresh(&n, &s, 2);
    s.has_song = 0;
    s.playing = 0;
    s.stopped = 1;
    ui_update(&n, &s, 0);
    ui_press(&n, UC_IN_K2, 1);
    CHECK(n.req_at[UFLD_TEMPO] < 0 && !n.msg_until, "stopped: nothing waits, nothing said");
    snap_for(&s, UI_FX_SEED, 2, 1);
    s.bar = s.roll.v[RF_BARS] - 2;
    ui_update(&n, &s, 0);
    ui_press(&n, UC_IN_K2, 1);
    CHECK(n.req_at[UFLD_TEMPO] < 0 && !strcmp(n.msg[0], "TEMPO"), "past the last boundary: nothing waits, the header "
          "says it (%d, %s)", n.req_at[UFLD_TEMPO], n.msg[0]);
}

/* a new song: home, nothing waiting */
static void test_nav_new_song(void)
{
    ui_nav_t n;
    snap_t s;
    nav_fresh(&n, &s, 2);
    ui_press(&n, UC_IN_ALGO, 3);
    ui_press(&n, UC_IN_EDIT, 1);
    ui_press(&n, UC_IN_K1, 1);
    s.roll.seed++;
    ui_update(&n, &s, 0);
    CHECK(shown(&n) == US_MAIN && n.depth == 1 && n.req_at[UFLD_COMP_INS] < 0, "a new song: the main screen, nothing "
          "waits (%d)", shown(&n));
}

/* HOME on the main screen: now playing, alone on the breadcrumb; the home stays the main screen (Settings' alone
   changes it): HOME again, a new song, HOME from elsewhere all land on the main screen */
static void test_nav_home_root(void)
{
    ui_nav_t n;
    snap_t s;
    nav_fresh(&n, &s, 2);
    ui_press(&n, UC_IN_HOME, 1);
    CHECK(shown(&n) == US_NOW && n.depth == 1 && n.home == US_MAIN, "HOME: now playing, the home kept (%d, %d)",
          shown(&n), n.home);
    ui_press(&n, UC_IN_GLO, 1);
    ui_press(&n, UC_IN_HOME, 1);
    CHECK(shown(&n) == US_MAIN && n.depth == 1, "HOME on Settings: the home, the main screen (%d)", shown(&n));
    ui_press(&n, UC_IN_HOME, 1);
    s.roll.seed++;
    ui_update(&n, &s, 0);
    CHECK(shown(&n) == US_MAIN && n.depth == 1, "a new song: the home, the main screen (%d)", shown(&n));
}

/* PRESET on the main screen: the highlighted part's instrument; SEQ on Bass: its re-roll, sent in the bar before */
static void test_nav_preset_seq(void)
{
    ui_nav_t n;
    snap_t s;
    char want[32];
    nav_fresh(&n, &s, 2);
    ui_press(&n, UC_IN_PRESET, 1);
    CHECK(n.req_at[UFLD_DRUM_KIT] < 0 && n.req_at[UFLD_BASS_INS] < 0, "Song highlighted: PRESET does nothing");
    ui_press(&n, UC_IN_ALGO, 2);
    ui_press(&n, UC_IN_PRESET, 1);
    CHECK(n.req_at[UFLD_BASS_INS] == 4, "Bass highlighted: PRESET steps the bass (%d)", n.req_at[UFLD_BASS_INS]);
    ui_press(&n, UC_IN_EDIT, 1);
    ui_press(&n, UC_IN_SEQ, 1);
    CHECK(n.roll_part == UI_PART_BASS && n.roll_at == 4, "SEQ on Bass: its re-roll waits for bar 4");
    s.bar = 3;
    ui_update(&n, &s, 0);
    snprintf(want, sizeof want, "reroll_part %d 0", UI_PART_BASS);
    CHECK(strstr(eng_log, want) != NULL, "sent in bar 3: %s", eng_log);
    ui_press(&n, UC_IN_HOME, 1);
    ui_press(&n, UC_IN_SEQ, 1);
    CHECK(strstr(eng_log, "reroll 0 0") != NULL, "SEQ on the main screen: a new song: %s", eng_log);
}

/* Review Focus 4: a spun encoder adds up; two keys at once; a long press while a knob turns; the lights */
static void test_nav_fast_keys(void)
{
    ui_nav_t n;
    snap_t s;
    char want[64];
    uint32_t blink, on;
    nav_fresh(&n, &s, 2);
    ui_press(&n, UC_IN_SELECT, 12);
    CHECK(n.cursor[US_MAIN] == 12 % 7, "SELECT +12: the highlight wraps to %d (%d)", 12 % 7, n.cursor[US_MAIN]);
    ui_key(&n, 0, 1, 1000);                    /* F (drums) and C (pad) down together */
    ui_key(&n, 7, 1, 1000);
    ui_key(&n, 0, 0, 1100);                    /* F let go soon: drums muted */
    ui_tick(&n, 1200);
    ui_press(&n, UC_IN_K1, 1);                 /* a knob turns while C is held */
    ui_tick(&n, 1600);                         /* C held 600 ms: the pad soloed, once */
    ui_tick(&n, 1700);
    ui_key(&n, 7, 0, 1800);
    snprintf(want, sizeof want, "mute %d 1; solo %d 0", UI_PART_DRUMS, UI_PART_PAD);
    CHECK(!strcmp(eng_log, want), "each key's press judged on its own: %s", eng_log);
    CHECK(n.req_at[UFLD_KEY] == 4, "the knob's turn still asked (%d)", n.req_at[UFLD_KEY]);
    on = ui_key_lights(&n, &s, &blink);
    CHECK(on == 0 && blink == 1u << 7, "pad soloed: key 7 blinking, the others dark (on %x, blink %x)", on, blink);
    eng_log[0] = 0;
    ui_key(&n, 7, 1, 2000);                    /* a press on the soloed part's key: un-solo, not mute */
    ui_key(&n, 7, 0, 2100);
    CHECK(!strcmp(eng_log, "solo -1 0"), "a press on the soloed pad un-solos it: %s", eng_log);
    eng_muted[UI_PART_DRUMS] = 0;
    eng_muted[UI_PART_BASS] = 1;
    on = ui_key_lights(&n, &s, &blink);
    CHECK(on == (1u << 0 | 1u << 4 | 1u << 6 | 1u << 7) && !blink, "bass muted: keys 0, 4, 6, 7 lit (%x)", on);
}

/* the switches flip either way; the mixer's level and a lane's mute on Parts */
static void test_nav_switch_mixer(void)
{
    ui_nav_t n;
    snap_t s;
    nav_fresh(&n, &s, 2);
    ui_press(&n, UC_IN_FX, 1);
    CHECK(shown(&n) == US_LEVELS_KNOBS, "FX: the mixer (its knobs layout)");
    ui_press(&n, UC_IN_K2, -3);
    CHECK(eng_level[UI_PART_BASS] == -3 && !strcmp(n.msg[0], "BASS") && !strcmp(n.msg[1], "-3 DB"),
          "K2 sets the bass's level at once (%d, %s %s)", eng_level[UI_PART_BASS], n.msg[0], n.msg[1]);
    ui_press(&n, UC_IN_K2, -20);
    CHECK(eng_level[UI_PART_BASS] == -12, "clamped at -12 dB (%d)", eng_level[UI_PART_BASS]);
}

/* Sections: the list scrolls with the highlight (rows: 7); a jump from the last section lands on the song's last bar;
   it clears once landed, even backward */
static void test_nav_sections(void)
{
    ui_nav_t n;
    snap_t s;
    ui_bag_t b;
    char t[UI_VALUE_MAX];
    int i, bars;
    nav_fresh(&n, &s, 2);
    s.n_secs = 9;                              /* nine sections of 4 bars */
    for (i = 0; i < 9; i++) {
        s.secs[i].start = (uint8_t)(4 * i);
        s.secs[i].bars = 4;
        s.secs[i].section = SEC_GROOVE;
    }
    bars = s.roll.v[RF_BARS];
    ui_update(&n, &s, 0);
    ui_press(&n, UC_IN_SEL, 1);
    CHECK(shown(&n) == US_SECTIONS, "SEL: the sections");
    ui_press(&n, UC_IN_SELECT, 8);
    ui_values(&s, &n, &b);
    CHECK(n.off[US_SECTIONS] == 2 && ui_true(&b, UV_I(6, UVK_I_SEL)), "the highlight on the 9th: lines from the 3rd "
          "(off %d)", n.off[US_SECTIONS]);
    CHECK(!strcmp(ui_value(&b, UV_SEC_RANGE, 0, t), "3-9 OF 9"), "the pager: %s", t);
    s.bar = bars - 2;                          /* the last section: its boundary past the end */
    ui_update(&n, &s, 0);
    ui_press(&n, UC_IN_EDIT, 1);
    CHECK(n.jump_i == 8 && n.jump_at == bars - 1, "a jump from the last section lands on the last bar (%d at %d)",
          n.jump_i, n.jump_at);
    CHECK(strstr(eng_log, "jump") != NULL, "sent at once (the bar before): %s", eng_log);
    s.bar = 4;                                 /* the song went back (a jump to an earlier section) */
    ui_update(&n, &s, 0);
    CHECK(n.jump_i < 0, "it clears once the song has moved on, back too");
}

/* ui_kit_pick (desk.js's approvedKit; smoke.mjs holds it to the page's over 300 seeds): only a retired or removed
   rolled kit (the mixed sampled kits: removed 2026-11-10) moves, onto an approved kit of the song's family; with no
   bank never onto a sampled shipped kit; onto Room - brushes only in a sunny song (Sam, 2026-11-12) */
static void test_kit_pick(void)
{
    uint32_t seed;
    int moved = 0, kept = 0, room_br = 0;
    for (seed = 1; seed <= 300; seed++) {
        song_roll_t r;
        int was, k, nb;
        roll_song(NULL, seed, &r);
        was = lib_song_kit(&r);
        k = ui_kit_pick(&r, 1);
        nb = ui_kit_pick(&r, 0);
        if (CH_STATUS(ch_flags(ROW_KIT, was)) >= 2) {
            CHECK(k >= 0 && CH_STATUS(ch_flags(ROW_KIT, k)) == 0 &&
                      CH_FAM(ch_flags(ROW_KIT, k)) == CH_FAM(ch_flags(ROW_KIT, nb)),
                  "seed %u: retired or removed kit %d picks %d (no bank: %d)", (unsigned)seed, was, k, nb);
            CHECK(nb >= 0 && !SONG_KIT_IS_SAMPLED(nb - SONG_KIT_PRESET), "seed %u: no bank, yet kit %d", (unsigned)seed,
                  nb);
            CHECK(k != SONG_KIT_PRESET + SONG_KIT_SAMPLED + 2 || r.v[RF_MOOD] == 3,
                  "seed %u: Room - brushes in a song of mood %d (sunny only: Sam, 2026-11-12)", (unsigned)seed,
                  r.v[RF_MOOD]);
            room_br += k == SONG_KIT_PRESET + SONG_KIT_SAMPLED + 2;
            moved++;
        } else {
            CHECK(k == -1 && nb == -1, "seed %u: approved kit %d moved to %d", (unsigned)seed, was, k);
            kept++;
        }
    }
    CHECK(moved > 0, "no seed in 300 rolled a retired kit (%d kept)", kept);
    CHECK(room_br > 0, "no sunny song in 300 picked Room - brushes");
}

/* G5 (key 26) held: the stutter on while down (the engine told at once, said), off when let go (on whatever screen by
   then); a short press the same, no move queued; never lit (its LED doesn't work). F#3 (key 1), on any screen: each
   press the next move at the section's end (set ROW_MOVE: 3 simple stutter, 4 complex, 1 drum fill, 2 comp fill, 0
   off), said; it blinks while one waits (the shared blink: *blink), on Sections too; stopped, a press queues too */
static void test_nav_stutter(void)
{
    ui_nav_t n;
    snap_t s;
    static const int NEXT[6] = {3, 4, 1, 2, 0, 3};
    static const char *const SAID[6] = {"END STUTTER SIMPLE", "END STUTTER COMPLEX", "END DRUM FILL", "END COMP FILL",
                                        "END OFF", "END STUTTER SIMPLE"};
    uint32_t on, blink, t;
    char want[96];
    nav_fresh(&n, &s, 2);
    n.said[0] = 0;
    ui_key(&n, 26, 1, 1000);
    CHECK(!strcmp(eng_log, "stutter 1 0") && !strcmp(n.said, "STUTTER"), "G5 down: %s, said %s", eng_log, n.said);
    ui_tick(&n, 2000);
    on = ui_key_lights(&n, &s, &blink);
    CHECK(!(on >> 26 & 1u) && !(blink >> 26 & 1u), "G5 never lit (%x, %x)", on, blink);
    ui_press(&n, UC_IN_SEL, 1);                /* to Sections (its keys jump; G5 still the stutter's) */
    eng_log[0] = 0;
    n.said[0] = 0;
    ui_key(&n, 26, 0, 2500);
    CHECK(!strcmp(eng_log, "stutter 0 0") && !n.said[0], "G5 up on another screen: %s, said %s", eng_log, n.said);
    eng_log[0] = 0;
    ui_key(&n, 26, 1, 2600);
    ui_key(&n, 26, 0, 2680);
    CHECK(!strcmp(eng_log, "stutter 1 0; stutter 0 0"), "G5 tapped: the hold only (%s)", eng_log);
    for (t = 0; t < 6; t++) {                  /* F#3: the moves in turn (3 on the main screen, 3 on Sections) */
        if (t == 3) ui_press(&n, UC_IN_SEL, 1);
        eng_log[0] = 0;
        ui_key(&n, 1, 1, 3000 + t * 100);
        ui_key(&n, 1, 0, 3050 + t * 100);
        snprintf(want, sizeof want, "set %d %d", ROW_MOVE, NEXT[t]);
        CHECK(!strcmp(eng_log, want) && !strcmp(n.said, SAID[t]), "F#3 press %u: %s, said %s", t + 1, eng_log, n.said);
    }
    s.rows[ROW_MOVE] = 0;
    on = ui_key_lights(&n, &s, &blink);
    CHECK(!(blink & 2u) && !(on & 2u), "nothing waiting: F#3 dark (%x, %x)", on, blink);
    s.rows[ROW_MOVE] = MOVE_DRUM_FILL;         /* one waits: F#3 blinks, here and on Sections */
    on = ui_key_lights(&n, &s, &blink);
    CHECK((blink & 2u) && !(on & 2u), "a move waiting: F#3 blinks (%x, %x)", on, blink);
    ui_press(&n, UC_IN_SEL, 1);
    on = ui_key_lights(&n, &s, &blink);
    CHECK(shown(&n) == US_SECTIONS && (blink & 2u), "Sections: F#3 blinks too (%x)", blink);
    snap_for(&s, UI_FX_SEED, 2, 0);            /* stopped: a press queues (for when the song plays) */
    ui_update(&n, &s, 6000);
    eng_log[0] = 0;
    ui_key(&n, 1, 1, 6000);
    ui_key(&n, 1, 0, 6050);
    snprintf(want, sizeof want, "set %d 3", ROW_MOVE);
    CHECK(!strcmp(eng_log, want) && !strcmp(n.said, "END STUTTER SIMPLE"), "stopped, F#3: %s, said %s", eng_log,
          n.said);
}

/* the held stutter's schedule (sthold.c): from each place in a bar, the catch on the press's 16th, the repeat 2
   16ths on, the first step faster on the first beat line 4 or more on, then one a beat to the 64th; let go, live on
   that 16th, the song's own stutters free at the next bar line */
static void test_sthold_schedule(void)
{
    int st0, k, r;
    for (st0 = 0; st0 < 16; st0++) {
        sthold_t h;
        int first = -1, steps = 0, lvl_ok = 1, live = -1, freed = -1;
        memset(&h, 0, sizeof h);
        for (k = 0; k < 48; k++) {
            int st = (st0 + k) % 16, down = k < 30;
            r = sthold_tick(&h, down, st);
            if (k == 0) CHECK(r == STH_CATCH && h.level == 0, "st %d: the catch first (%x)", st0, r);
            if (k == 2) CHECK(r == STH_REPLAY, "st %d: the repeat 2 16ths on (%x)", st0, r);
            if (r & STH_STEP) {
                if (first < 0) first = k;
                CHECK(st % 4 == 0, "st %d: a step off the beat (16th %d)", st0, st);
                if (h.level != ++steps) lvl_ok = 0;
            }
            if (r & STH_LIVE) live = k;
            if (r & STH_FREE) freed = k;
        }
        CHECK(first >= 4 && first < 8, "st %d: the first step %d 16ths on", st0, first);
        CHECK(steps == STH_LEVELS - 1 && lvl_ok && h.level == 3, "st %d: %d steps to level %d", st0, steps, h.level);
        CHECK(live == 30 && !h.on, "st %d: live on the release's 16th (%d)", st0, live);
        CHECK(freed >= 30 && (st0 + freed) % 16 == 0 && !h.lock, "st %d: freed at the bar line (%d)", st0, freed);
    }
    {                                          /* a press shorter than the first step: no step, live, and the lock to
                                                  the bar line */
        sthold_t h;
        memset(&h, 0, sizeof h);
        CHECK(sthold_tick(&h, 1, 0) == STH_CATCH, "the catch");
        CHECK(sthold_tick(&h, 0, 1) == STH_LIVE && h.lock, "a 16th later: live, the lock kept");
        for (k = 2; k < 16; k++) CHECK(sthold_tick(&h, 0, k) == 0, "nothing in the bar (16th %d)", k);
        CHECK(sthold_tick(&h, 0, 0) == STH_FREE && !h.lock, "freed at the bar line");
    }
    CHECK(sthold_lp(0) == 0.0f && sthold_lp(1) > sthold_lp(2) && sthold_lp(2) > sthold_lp(3) && sthold_lp(3) >= 500.0f,
          "the low-pass open on the 8th, closing a step each level, never below 500 Hz");
}

/* the lock: while a held stutter holds, the song's own vx_stutter calls do nothing; freed, they act again */
static void test_sthold_lock(void)
{
    static float l[256], r[256];
    int i;
    vx_init();
    vx_stutter_hold(1, 2000, 0.0f);            /* an 8th caught (2000 samples) */
    for (i = 0; i < 8; i++) {
        memset(l, 0, sizeof l);
        memset(r, 0, sizeof r);
        vx_render(l, r, 256);
    }
    vx_stutter_hold(2, 0, 0.0f);
    CHECK(vx_stutter_held() && vx_stuttering(), "held: repeating");
    vx_stutter(0, 0);                          /* the song's: ignored */
    vx_stutter(1, 500);
    CHECK(vx_stuttering(), "the song's own calls ignored while held");
    vx_stutter_hold(0, 0, 1600.0f);
    vx_stutter_free();
    CHECK(!vx_stutter_held(), "freed");
    vx_stutter(1, 500);                        /* the song's: a catch, live while it catches */
    for (i = 0; i < 4; i++) vx_render(l, r, 256);
    CHECK(!vx_stuttering(), "freed: the song's call acts (a catch)");
    vx_stutter_hold(1, 2000, 0.0f);
    vx_release_all();
    CHECK(!vx_stutter_held(), "a stop lets go of the lock");
}

int main(int argc, char **argv)
{
    const char *only = argc > 1 ? argv[1] : NULL;
#define RUN(t) do { if (!only || strstr(#t, only)) t(); } while (0)
    RUN(test_values_fixture);
    RUN(test_values_pending);
    RUN(test_values_unknown_choice);
    RUN(test_nav_main_to_comp);
    RUN(test_nav_pending);
    RUN(test_nav_pending_order);
    RUN(test_nav_stopped);
    RUN(test_nav_new_song);
    RUN(test_nav_home_root);
    RUN(test_nav_preset_seq);
    RUN(test_nav_fast_keys);
    RUN(test_nav_switch_mixer);
    RUN(test_nav_sections);
    RUN(test_nav_stutter);
    RUN(test_sthold_schedule);
    RUN(test_sthold_lock);
    RUN(test_kit_pick);
    if (fails) {
        printf("%d check(s) failed\n", fails);
        return 1;
    }
    printf("ui: all checks passed\n");
    return 0;
}

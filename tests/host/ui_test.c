/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The device's screens runtime on the host (app/ui): the value bag against the mock's values in the same states
   (ui_values_fixture.h, workbench/tools/ui_fixture.mjs). */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "values.h"
#include "screens_gen.h"
#include "choices_gen.h"
#include "song.h"
#include "plan.h"
#include "override.h"

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
                double got = ui_value_num(&b, id);
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

int main(int argc, char **argv)
{
    const char *only = argc > 1 ? argv[1] : NULL;
#define RUN(t) do { if (!only || strstr(#t, only)) t(); } while (0)
    RUN(test_values_fixture);
    RUN(test_values_pending);
    RUN(test_values_unknown_choice);
    if (fails) {
        printf("%d check(s) failed\n", fails);
        return 1;
    }
    printf("ui: all checks passed\n");
    return 0;
}

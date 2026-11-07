/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The screens' values: the mock's valuesOf (workbench/ui/device.js) in C. ui_values takes the main loop's snapshot and
   the navigator's state; ui_value then writes any value (screens_gen.h's UV_*) on demand, so the bag itself holds
   only what several values share. No heap; the text of a value fits UI_VALUE_MAX. */
#ifndef UI_VALUES_H
#define UI_VALUES_H
#include <stdint.h>
#include "snap.h"
#include "nav.h"
#include "rolled.h"
#include "song.h"

enum { UI_VALUE_MAX = 64, UI_OPTS_MAX = 128 };              /* a value's text, NUL included (the longest name plus "*AS ROLLED (...)") */

/* What the Parts roll and the Mixer show besides the values (the mock's bag.activity and bag.levels): the song's notes
   around the bar playing (the render's: a window of bars, from the song itself), what is muted and soloed, the levels */
typedef struct { uint8_t bar, step, len, note, vel, part; } ui_note_t;   /* step 0..15, len in 16ths, part UI_PART_* */
typedef struct {
    const ui_note_t *notes;
    int n;
    uint32_t mute;                       /* bit UI_PART_*: muted */
    int solo;                            /* the part soloed, -1 none */
    int8_t level[8];                     /* the mixer's level by part, dB */
} ui_activity_t;

typedef struct ui_bag {
    const snap_t *s;
    const ui_nav_t *n;
    int screen;                          /* the screen shown */
    song_rolled_t rolled;                /* what the "as rolled" rows play */
    int pend_field, pend_bars;           /* the earliest field waiting (-1 none), and its bars to go */
    const ui_activity_t *act;            /* (the caller's, after ui_values; NULL: none) */
} ui_bag_t;

/* a value's kind: what the mock's bag holds for it (uistate writes it so: a string, true/false, a number) */
enum { UI_VK_STR, UI_VK_BOOL, UI_VK_NUM };

void ui_values(const snap_t *s, const ui_nav_t *nav, ui_bag_t *b);
/* the value's text (tmp: UI_VALUE_MAX bytes, maybe used); "" when unset; a bool "1" or "", a number in decimal. index:
   reserved (0) */
const char *ui_value(const ui_bag_t *b, int value_id, int index, char *tmp);
int ui_value_kind(int value_id);
/* a number's value as the fraction (the return) / *den: an integer over 1, a Frac 0..1 (the device has no double) */
int ui_value_num(const ui_bag_t *b, int value_id, int *den);
/* a condition (screens_gen.h's ui_when_t entry: a value id, negative for "not"): the mock's !!bag[name] */
int ui_true(const ui_bag_t *b, int cond_id);
/* each row's value as a song with roll r plays it (the render side, for the snapshot): the roll's fields, its set's
   instruments and kit, the overrides in force (song_ov) */
void ui_song_rows(const song_roll_t *r, int16_t rows[ROW_N]);
/* desk.js's approvedKit: a song rolled on a retired kit plays an approved kit of its family instead (rows.js's
   familyOf: the keys', else the lead's, else acoustic), the seed's pick (pickFor's choice with desk.js's seeded
   generator), a sampled shipped kit only with sampled_ok (the bank). The kit row's value, or -1: the rolled kit is
   approved (or none fits) */
int ui_kit_pick(const song_roll_t *r, int sampled_ok);
/* for the navigator: a field's text for value v (the mock's fieldText), its value as the song plays it, its choices
   in order (the device's options: values into out, UI_OPTS_MAX at most; the count) */
const char *ui_field_text(const ui_bag_t *b, int f, int v, char *t);
int ui_field_now(const ui_bag_t *b, int f);
int ui_field_options(const ui_bag_t *b, int f, int *out);
/* the sections of a song stopped and ready (its plan not built by the render yet): from its roll */
void ui_snap_sections(snap_t *s);

#endif

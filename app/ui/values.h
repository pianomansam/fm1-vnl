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

enum { UI_VALUE_MAX = 64 };              /* a value's text, NUL included (the longest name plus "*AS ROLLED (...)") */

typedef struct ui_bag {
    const snap_t *s;
    const ui_nav_t *n;
    int screen;                          /* the screen shown */
    song_rolled_t rolled;                /* what the "as rolled" rows play */
    int pend_field, pend_bars;           /* the earliest field waiting (-1 none), and its bars to go */
} ui_bag_t;

/* a value's kind: what the mock's bag holds for it (uistate writes it so: a string, true/false, a number) */
enum { UI_VK_STR, UI_VK_BOOL, UI_VK_NUM };

void ui_values(const snap_t *s, const ui_nav_t *nav, ui_bag_t *b);
/* the value's text (tmp: UI_VALUE_MAX bytes, maybe used); "" when unset; a bool "1" or "", a number in decimal. index:
   reserved (0) */
const char *ui_value(const ui_bag_t *b, int value_id, int index, char *tmp);
int ui_value_kind(int value_id);
double ui_value_num(const ui_bag_t *b, int value_id);    /* a number's value (a Frac: 0..1) */
/* a condition (screens_gen.h's ui_when_t entry: a value id, negative for "not"): the mock's !!bag[name] */
int ui_true(const ui_bag_t *b, int cond_id);
/* each row's value as a song with roll r plays it (the render side, for the snapshot): the roll's fields, its set's
   instruments and kit, the overrides in force (song_ov) */
void ui_song_rows(const song_roll_t *r, int16_t rows[ROW_N]);

#endif

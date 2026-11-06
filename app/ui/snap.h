/* SPDX-License-Identifier: GPL-3.0-or-later */
/* What the render publishes for the screen (app.c's snapshot, read by the main loop): the transport, the song's place
   and roll, the knobs' requests, and what the screens' values need besides (ui_values): each row's value as the song
   plays it and the plan's sections. */
#ifndef UI_SNAP_H
#define UI_SNAP_H
#include <stdint.h>
#include "roll.h"
#include "choices_gen.h"

enum { SNAP_SECS = 24 };                  /* the plan's blocks a song has at most (song_plan_t's) */

typedef struct {
    uint8_t playing, has_song, stopped;
    uint8_t ready, held;                 /* stopped, a song ready (roll); held: one to continue (bar, section) */
    uint8_t req_set[RF_N];               /* a knob's request waiting for the boundary, and its value */
    int16_t req_val[RF_N];
    int32_t songs, bar, section, flags, pend_field, pend_bars, voices;
    song_roll_t roll;
    int16_t rows[ROW_N];                 /* each row's value as the song plays it (ui_song_rows) */
    uint8_t n_secs;                      /* the plan's blocks: their first bar (from 0), length and SEC_* */
    struct { uint8_t start, bars, section; } secs[SNAP_SECS];
    uint16_t changes;                    /* song_t's: a row request landed */
} snap_t;

#endif

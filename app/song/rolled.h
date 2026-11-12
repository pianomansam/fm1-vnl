/* SPDX-License-Identifier: GPL-3.0-or-later */
/* What a song's "as rolled" choices play: the comping style and rhythm, the lead style, the pad, the strum, the bass
   line's style, the ride, the keys' and the lead's registers. The device's screens ("AS ROLLED (HELD)") and the
   workbench (wb_rolled) both read it. Read only: it asks the engine under the overrides and library hooks in force
   (song_override, lib_set_wb), so the caller installs the ones it wants first. */
#ifndef SONG_ROLLED_H
#define SONG_ROLLED_H
#include "roll.h"

typedef struct {
    int comp;          /* the comp style (CS_*) */
    int comp_pat;      /* its added comp rhythm (a COMP_PAT index), else -1 */
    int lead_style;    /* LS_* */
    int pad_on;        /* 0/1 */
    int pad_place;     /* PL_* */
    int strum_pat;     /* the strum pattern's table index */
    int strum_fill;    /* 0 step back, 1 drive */
    int lead_loop;     /* the firmware's lead loop default */
    int bass_style;    /* BS_*: roots, walking, pedal */
    int ride;          /* RIDE_* */
    int keys_high;     /* the keys sit high (harmony_keys_high) */
    int lead_lo, lead_hi;   /* the lead's band, MIDI (leads_band_for) */
    int keys_with;     /* the keys over a low lead: 1 with the lead, 0 lifted (harmony_keys_draw) */
} song_rolled_t;

void song_rolled(const song_roll_t *r, song_rolled_t *out);

#endif

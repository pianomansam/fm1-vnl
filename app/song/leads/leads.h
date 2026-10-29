/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The lead styles (lead-styles design, 2026-10-29): eight styles by genre, one file each beside this one, sharing a
   motif format, a register center per style and lead instrument, and the band the lead claims around it (the keys
   voice under it: harmony_keys_window). lead.c dispatches here for a style; the older engines (the phrase planner,
   the hybrid) still plan where no style does, inside the same band. */
#ifndef SONG_LEADS_H
#define SONG_LEADS_H
#include <stdint.h>
#include "lead.h"

enum { LS_IMPRESSIONISM, LS_ROMANTICISM, LS_JAZZ60S, LS_MODAL, LS_JAZZHOP, LS_FILMSCORE, LS_BOSSA, LS_AMBIENT, LS_N };

/* A style's motif, in one form for every style (a later style may take another's): up to 8 notes, each an on-grid
   onset and length in 16ths from the motif's start (it may span bars), its scale steps from the first note (step[0]
   is 0), and its contour (bit i set: note i + 1 rises over note i). */
typedef struct { uint8_t n; uint8_t at[8], len[8]; int8_t step[8]; uint8_t contour; } lead_motif_t;

/* A motif from n notes (onsets ascending, lengths 1 or more, si: scale indexes); 0 (and m untouched) if they don't
   fit the form. */
int lead_motif_from(lead_motif_t *m, int n, const int *at, const int *len, const int *si);
/* The motif's notes again, its first on scale index si0; returns n. */
int lead_motif_to(const lead_motif_t *m, int si0, int *at, int *len, int *si);

/* The song's lead style (LS_*), or -1: an older engine plans (no style forced, or lead_style 0 or 1). */
int leads_style(const song_roll_t *r);
/* The register center (MIDI) of a style (-1: an older engine) on a lead instrument, kept 6 semitones inside the
   instrument's range. */
int leads_center(int style, int ins);
/* The band the song's lead claims over percussive keys: its center (the workbench's lead_oct an octave off it) +-6,
   in the instrument's range. The song sets the keys' window under it (song.c). */
void leads_band(const song_roll_t *r, int *lo, int *hi);
/* The lead's range this bar over percussive keys: from the band's floor, moved up to a 4th over the keys' top where
   the keys reach into the band (their window could not hold them: today's floor); to E6 as before (the workbench's
   lead_oct an octave off it, its keys_oct raising it with the keys, in the instrument's range), the band's top at
   least. lo_x: the floor for a note that sounds on into the next bar (over its keys, nkeys, likewise). */
void leads_range_bar(const song_roll_t *r, const int keys[4], const int nkeys[4], int *lo, int *lo_x, int *hi);
/* Plans the bar with the song's style (arguments as lead_plan_bar's, which calls it once it has cleared ls and
   found the lead playing). 0 where no style plans (an older engine does), else 1. */
int leads_plan_bar(lead_state_t *ls, const song_roll_t *r, const plan_block_t *b, int bar, const int keys[4],
                   const int nkeys[4], const int *pcs, int np, int can_push);

#endif

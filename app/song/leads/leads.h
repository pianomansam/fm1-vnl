/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The lead styles (lead-styles design, 2026-10-29): eight styles by genre, one file each beside this one, sharing a
   motif format, a register center per style and lead instrument, and the band the lead claims around it (the keys
   voice under it: harmony_keys_window). lead.c dispatches here for a style; the older engines (the phrase planner,
   the hybrid) still plan where no style does, over their own floor (a 4th over the keys' top) until they're
   removed: the band reaches them only through the keys' window. */
#ifndef SONG_LEADS_H
#define SONG_LEADS_H
#include <stdint.h>
#include "lead.h"

enum { LS_IMPRESSIONISM, LS_ROMANTICISM, LS_JAZZ60S, LS_MODAL, LS_JAZZHOP, LS_FILMSCORE, LS_BOSSA, LS_AMBIENT, LS_N };

/* A style's motif, in one form for every style (a later style may take another's): up to 8 notes, each an on-grid
   onset and length in 16ths from the motif's start (it may span bars; a triplet's onset rounded down to its 16th, its
   lateness not carried), its steps from the first note (step[0] is 0) in the collection `scale` names (LM_*), and its
   contour (bit i set: note i + 1 rises over note i).
   What each style carries as its motif:
   - Impressionism: the arabesque of its phrase heads (its quick notes and the long note they land on), in the
     mode's pentatonic (with the mode's color note in its modal songs).
   - Romanticism: its rhythm cell, one bar's onsets with the walk's steps through them (in the mode's scale), which
     every bar of the phrase restates a step or two higher or lower. The spine the cell is sequenced along (a bar
     head a bar, rising, falling or an arch) is the style's own way of developing it, not part of the motif. */
enum { LM_MODE, LM_PENT, LM_PENT_COLOR };   /* the mode's 7 notes; its pentatonic; the pentatonic and the mode's
                                               color note */
typedef struct { uint8_t n; uint8_t at[8], len[8]; int8_t step[8]; uint8_t contour, scale; } lead_motif_t;

/* A motif from n notes (onsets ascending, lengths 1 or more, si: scale indexes); 0 (and m untouched) if they don't
   fit the form. */
int lead_motif_from(lead_motif_t *m, int n, const int *at, const int *len, const int *si);
/* The song's motif in that form (its style's: leads_style), 0 where no style plans (an older engine). */
int leads_motif(const song_roll_t *r, lead_motif_t *m);
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

/* ---- for the style files (and the older engines in lead.c) ---- */

typedef struct {                  /* the bar a style plans */
    const song_roll_t *r;
    const int *keys, *pcs;        /* the keys' voicing this bar (4 notes, 0 none); the chord, root first */
    int np;
    unsigned bass;                /* pitch classes the bass strikes in the bar */
    int lo, hi;                   /* the lead's range this bar (leads_range_bar over percussive keys; a pad's place) */
    int lo_x;                     /* the floor for a note that sounds on into the next bar */
    int can_push;                 /* the next bar's chord is known (no request lands on it) */
    int pad;                      /* the keys sustain (a pad, the organ) */
    int vibes;                    /* the vibes lead: struck, so the swell velocities are lifted */
} leads_ctx_t;
typedef struct { const int *pcs; int np; unsigned bass; } leads_next_t;   /* the next bar's chord (np 0: none) */

int leads_is_ct(int note, const int *pcs, int np);
/* a chord tone or a scale note, never a semitone from a chord tone or a note the bass strikes, and not an avoid
   note */
int leads_legal(const song_roll_t *r, int note, const int *pcs, int np, unsigned bass);
/* the pitch classes leads_legal allows over a chord (bit pc) */
unsigned leads_legal_pcs(const song_roll_t *r, const int *pcs, int np, unsigned bass);
/* pc's octave in [lo, hi] nearest `near`; 0 if none */
int leads_place(int pc, int lo, int hi, int near);
/* Where bar sits in its phrase: a run of contiguous lead blocks (from run), 8-bar phrases from its start (pass, and
   bip the bar in it), a 4-bar remainder half a phrase (pbars 4); last: no lead block follows in the song. b must be
   the plan's own block (its neighbors are read from the plan). */
void leads_phrase_at(const plan_block_t *b, int bar, int *run, int *pass, int *bip, int *pbars, int *last);
/* the roll's scale, 7 pitch classes over the key; scale index <-> MIDI note (index 0: the key's lowest octave) */
void leads_scale(const song_roll_t *r, int *sc);
int leads_si_note(int key, const int *sc, int si);
int leads_note_si(int key, const int *sc, int note);   /* the scale note at or under note */
/* k members of mask (pitch-class bits) from note, up (k > 0) or down */
int leads_coll_step(unsigned mask, int note, int k);
/* note in the bar's range, legal, and a chord tone where strong */
int leads_ok(const leads_ctx_t *c, int note, int strong);
/* One onset at p (thirds of a 16th: a triplet's lateness), held to end (16ths, past 16 when it rings on), with
   velocity v shaped for the instrument (a percussive lead low among the keys lifted, the vibes' struck range) */
void leads_put(lead_state_t *ls, const leads_ctx_t *c, int p, int note, int end, int v);

/* the styles (one file each) */
void impressionism_plan(lead_state_t *ls, const leads_ctx_t *c, const plan_block_t *b, int bar,
                        const leads_next_t *nx);
void impressionism_motif(const song_roll_t *r, lead_motif_t *m);
void romanticism_plan(lead_state_t *ls, const leads_ctx_t *c, const plan_block_t *b, int bar,
                      const leads_next_t *nx);
void romanticism_motif(const song_roll_t *r, lead_motif_t *m);

#endif

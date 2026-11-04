/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The lead styles (lead-styles design, 2026-10-29): eight styles by genre, one file each beside this one, sharing a
   motif format, a register center per style and lead instrument, and the band the lead claims around it (the keys
   voice under it: harmony_keys_window). lead.c dispatches here: every song plays a style. */
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
     head a bar, rising, falling or an arch) is the style's own way of developing it, not part of the motif.
   - '60s Jazz: its statement, 2-4 8ths off the beat into a target over the bar line, undisplaced; its steps count in
     the chord's legal notes (the mode's, less those that rub: LM_MODE). Restatements displace it and sequence it.
   - Modal Jazz: its phrase heads' 1-3 notes stepping into the held tone (in the mode's scale), every head exact.
   - Jazz-Hop: its hook, 2-4 quick notes into a held tone in one bar (in the mode's pentatonic: LM_PENT), restated
     every 2 bars, the turnaround's one change aside.
   - Film Score: its phrase heads' 3-5 notes on the 8th grid into the long note (in the mode's pentatonic), each head
     restating its rhythm and up/down shape; the question sequences it a step up, the answer falls home.
   - Bossa Nova: its statement, 2-4 repeated notes on a syncopated cell, then a step (in the mode's scale) up or down
     into the long note on the "and" of 4, held over the bar line; bar 3 restates it on the same note or a step off.
   - Ambient: its 3-5 notes, legato, the last let ring (in the mode's pentatonic), restated every 2 bars at the same
     pitches while the chords move under it. */
enum { LM_MODE, LM_PENT, LM_PENT_COLOR };   /* the mode's 7 notes; its pentatonic; the pentatonic and the mode's
                                               color note */
typedef struct { uint8_t n; uint8_t at[8], len[8]; int8_t step[8]; uint8_t contour, scale; } lead_motif_t;

/* A motif from n notes (onsets ascending, lengths 1 or more, si: scale indexes); 0 (and m untouched) if they don't
   fit the form. */
int lead_motif_from(lead_motif_t *m, int n, const int *at, const int *len, const int *si);
/* The song's motif in that form, for its style (LS_*: the song's held lead.style - 1, or leads_style(r) for a roll
   with no song); returns 1. */
int leads_motif(const song_roll_t *r, int style, lead_motif_t *m);
/* The motif's notes again, its first on scale index si0; returns n. */
int lead_motif_to(const lead_motif_t *m, int si0, int *at, int *len, int *si);

/* The song's lead style (LS_*): the lead_style override's (2 + LS_*), else rolled (leads_style_roll; 0 and 1, the
   retired phrase planner and hybrid, roll too). A pure function of the roll and the overrides. */
int leads_style(const song_roll_t *r);
/* The song's rolled style, the override aside: weighted by the lead instrument's fit and the set's flavor
   (leads_weight), from the song's seed. */
int leads_style_roll(const song_roll_t *r);
/* A style's roll weight (0: the instrument isn't rolled for it) on a lead instrument in a set of flavor FL_*. */
int leads_weight(int ins, int flavor, int style);
/* 1 if the lead instrument plays the style (its fits; any other lead instrument takes the flute's: all eight) */
int leads_fit(int ins, int style);
/* The song's rolled style after a change (a set or mood request): prev (LS_*) where the song's lead instrument plays
   it, else rolled again (leads_style_roll). A song rolls its style once (song_start) and keeps it through this; the
   workbench's override plays over it and, cleared, gives it back (song.c). */
int leads_style_keep(const song_roll_t *r, int prev);
/* The register center (MIDI) of a style on a lead instrument, kept 6 semitones inside the instrument's range. */
int leads_center(int style, int ins);
/* The band a style's lead claims over percussive keys: its center (the workbench's lead_oct an octave off it) +-6, in
   the instrument's range. The song sets the keys' window under it (song.c). The style (LS_*) is the caller's: a song's
   held lead.style - 1 (song.c), or leads_style(r) for a roll with no song; leads_range_bar and leads_motif likewise. */
void leads_band_for(const song_roll_t *r, int style, int *lo, int *hi);
/* The lead's range this bar over percussive keys: from the band's floor, moved up to a 4th over the keys' top where
   the keys reach into the band (their window could not hold them: today's floor); to E6 as before (the workbench's
   lead_oct an octave off it, its keys_oct raising it with the keys, in the instrument's range), the band's top at
   least. lo_x: the floor for a note that sounds on into the next bar (over its keys, nkeys, likewise). */
void leads_range_bar(const song_roll_t *r, int style, const int keys[4], const int nkeys[4], int *lo, int *lo_x,
                     int *hi);
/* Plans the bar with the song's style, ls->style (leads_style where it is 0: unresolved; arguments as
   lead_plan_bar's, which calls it once it has cleared ls and found the lead playing). */
void leads_plan_bar(lead_state_t *ls, const song_roll_t *r, const plan_block_t *b, int bar, const int keys[4],
                    const int nkeys[4], const int *pcs, int np, int can_push);

/* The lead loop (leads/loop.c): the switch's firmware default (song_ov()->lead_loop -1 follows it); 1 where the loop
   plays the style (on: every style); the lead bus's low-pass amount at bar and step (ls: the song's lead
   state; b: its plan block; 0 where it doesn't play) */
#define LEAD_LOOP_DEFAULT 0
int leads_loop_plays(int style);
float leads_loop_lp(const song_roll_t *r, const lead_state_t *ls, const plan_block_t *b, int bar, int step,
                    int style);
#ifdef LEADS_LOOP_STATS
/* the song's counters (0 bars replayed, 1 notes nudged, 2 notes dropped, 3 stutters); the log per bar of the song
   last recorded (1 a stutter, 2 the bar muted, 4 its last 2 beats muted, 8 filtered, 16 a held-back entry) */
int leads_loop_stat(const lead_state_t *ls, int which);
int leads_loop_log(int bar);
#endif

/* ---- for the style files ---- */

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
/* the roll's scale as pitch-class bits; its mode's color note's pitch class (Dorian 6, Phrygian b2, Lydian #4,
   Mixolydian b7, Aeolian b6), -1 for Ionian */
unsigned leads_scale_mask(const song_roll_t *r);
int leads_color_pc(const song_roll_t *r);
/* the mode's pentatonic as pitch-class bits (Ionian, Lydian, Mixolydian: 1 2 3 5 6; Dorian, Phrygian, Aeolian:
   1 b3 4 5 b7), with its color note where color is set (and the mode has one) */
unsigned leads_pent_mask(const song_roll_t *r, int color);
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
void jazz60s_plan(lead_state_t *ls, const leads_ctx_t *c, const plan_block_t *b, int bar, const leads_next_t *nx);
void jazz60s_motif(const song_roll_t *r, lead_motif_t *m);
void modal_plan(lead_state_t *ls, const leads_ctx_t *c, const plan_block_t *b, int bar, const leads_next_t *nx);
void modal_motif(const song_roll_t *r, lead_motif_t *m);
void jazzhop_plan(lead_state_t *ls, const leads_ctx_t *c, const plan_block_t *b, int bar, const leads_next_t *nx);
void jazzhop_motif(const song_roll_t *r, lead_motif_t *m);
void filmscore_plan(lead_state_t *ls, const leads_ctx_t *c, const plan_block_t *b, int bar, const leads_next_t *nx);
void filmscore_motif(const song_roll_t *r, lead_motif_t *m);
void bossa_plan(lead_state_t *ls, const leads_ctx_t *c, const plan_block_t *b, int bar, const leads_next_t *nx);
void bossa_motif(const song_roll_t *r, lead_motif_t *m);
void ambient_plan(lead_state_t *ls, const leads_ctx_t *c, const plan_block_t *b, int bar, const leads_next_t *nx);
void ambient_motif(const song_roll_t *r, lead_motif_t *m);
/* the loop's bar: plan (the style's planner) records the slot the first time, then the slot replays (loop.c) */
void leads_loop_bar(lead_state_t *ls, const leads_ctx_t *c, const plan_block_t *b, int bar, const leads_next_t *nx,
                    int style, void (*plan)(lead_state_t *, const leads_ctx_t *, const plan_block_t *, int,
                                            const leads_next_t *));

#endif

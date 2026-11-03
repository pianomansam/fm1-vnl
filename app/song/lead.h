/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The lead: the song's lead style (leads/leads.h: eight styles by genre, one rolled per song) plays in the plan's
   lead blocks (L_LEAD, with keys, not the intro or outro). Planned a bar at a time; all state in lead_state_t. */
#ifndef SONG_LEAD_H
#define SONG_LEAD_H
#include <stdint.h>
#include "roll.h"
#include "plan.h"
#include "groove.h"

/* the lead loop's counters and per-bar log (leads_loop_stat, leads_loop_log): the tests, the host build and the
   workbench, not the firmware image */
#if defined PLAT_HOST || defined __EMSCRIPTEN__ || defined __APPLE__ || defined __linux__
#define LEADS_LOOP_STATS 1
#endif

typedef struct {
    int on[16], len[16];          /* the bar's onsets (MIDI, 0 none) and lengths in 16ths */
    uint8_t vel[16];              /* the onsets' velocities (25-110) */
    unsigned acc;                 /* accented onsets (a style's landings) */
    unsigned ring;                /* onsets that ring across the bar line (pushes) */
    int last;                     /* the last note sung (0: none yet) */
    int style;                    /* the style the lead plays, LS_* + 1 (0: not resolved: leads_style): the
                                     workbench's forced one, else the song's rolled one */
    int rolled;                   /* the song's rolled style, LS_* + 1: song.c keeps it all song (leads_style_keep) */
    int anchor, akey, ctrun;      /* Bossa Nova's own across bars (bossa.c) */
    int tied;                     /* a note was pushed from the last bar into this one */
    int active;                   /* the lead sounds in the planned bar (an onset, or a note pushed in) */
    int onsets;                   /* lead onsets that start in the planned bar */
    uint8_t dl[16];               /* a style's onset late by thirds of a 16th (a triplet: 0-2) */
    int tie_end;                  /* a style's note tied in from the last bar ends at this 16th */
    union {                       /* one style's own state across bars (a song plays one style; zeroed with the
                                     song) */
        struct {
            int step_dir;         /* the step its last bar planned over the bar line (+1, -1) */
            int head;             /* the answer's first note, chosen by the pickup before it (0: none) */
        } ro;                     /* Romanticism */
        struct {
            int cn, bar;          /* notes a statement carries over the bar line into bar */
            int tgt;              /* the last statement's target (its guide-tone line) */
            int p[8], note[8], end[8], vel[8];   /* the carried notes: onset (thirds of a 16th), note, end (16ths),
                                                    velocity */
        } jz;                     /* '60s Jazz */
        struct {
            int note, vel;        /* the held tone tied into this bar, for its fall-off */
        } mo;                     /* Modal Jazz */
        struct {
            int lvl;              /* the motif's first note, chosen once per song (0: not yet) */
        } am;                     /* Ambient */
    } st;
    struct {                      /* the lead loop (leads/loop.c): the recorded phrase, L bars by slot (bar % L) */
        uint8_t note[4][16], len[4][16], vel[4][16], dl[4][16];
        uint16_t ring[4], acc[4];
        uint8_t have, bars;       /* the slots recorded (bits); L */
        uint32_t sig;             /* what it was recorded for (a new song, key, mode, chords, set or style: anew) */
#ifdef LEADS_LOOP_STATS
        int stat[4];              /* bars replayed, notes nudged, notes dropped, stutters (leads_loop_stat) */
#endif
    } loop;
} lead_state_t;

/* 1 where the lead plays: blocks with L_LEAD and L_KEYS, not intro or outro */
int lead_plays(const plan_block_t *b);
/* Plans one bar (call at step 0) with the song's style (leads_plan_bar). keys, nkeys: this bar's and the next bar's
   sounding keys voicings (4 notes, 0 none); pcs: this bar's chord, root first. can_push 0: a request lands on the
   next bar, so nothing is pushed into it. b must be the plan's own block (a style reads its neighbors for the
   phrase's position). Returns the number of lead onsets that start in the bar (0 where the lead doesn't play). */
int lead_plan_bar(lead_state_t *ls, const song_roll_t *r, const plan_block_t *b, int bar, const int keys[4],
                  const int nkeys[4], const int *pcs, int np, int can_push);
/* The lead's events of one step of the planned bar. */
int lead_step(const lead_state_t *ls, int step, float step_samples, song_ev_t *out, int max);

#endif

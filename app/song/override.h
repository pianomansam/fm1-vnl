/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Per-song overrides (the workbench's): force a choice the roll makes, or re-roll one part. Default off: with none
   set, every choice is the roll's, exactly as before. Read where each choice is made (harmony, lead, groove). */
#ifndef SONG_OVERRIDE_H
#define SONG_OVERRIDE_H
#include <stdint.h>

typedef struct {
    int8_t comp;        /* -1 rolled, else CS_* */
    int8_t lead_style;  /* -1 rolled, else 2 + LS_* a lead style (leads/leads.h); 0 and 1 (the phrase planner and
                           the hybrid, retired: an old snapshot's) play as rolled */
    int8_t pad_place;   /* -1 rolled, else PL_* (sustained keys only: percussive keys are always below) */
    int8_t ride;        /* -1 rolled, else RIDE_NONE / RIDE_ALL / RIDE_SECTION (sampled kits only) */
    int8_t keys_oct;    /* -1, 0, +1: the keys' register, clamped to the instrument's range */
    int8_t lead_oct;    /* -1, 0, +1: the lead's register, clamped likewise */
    uint32_t voicing_salt;  /* 0: as rolled; else a different, still rule-abiding voicing path */
    uint32_t phrase_salt;   /* 0: as rolled; else the lead drawn anew (its motif, rhythm and contour; its rolled
                               style kept) */
    uint32_t bass_salt;     /* 0: as rolled; else the bass's style, walking window and velocities drawn anew */
    uint32_t pad_salt;      /* 0: as rolled; else the pad's solo voice, its sections' parts and which sections have
                               it (song_pad_in) drawn anew */
    uint32_t drums_salt;    /* 0: as rolled; else new per-hit velocity and lateness jitter for the drums */
    int8_t pad_on;          /* -1 rolled (song_pad_on), 0 no pad layer in the song, 1 a pad layer */
    /* forced pattern table entries (patterns_gen.h), approved or candidate: the workbench's way to hear a candidate,
       which the roll never draws. -1 rolled (the roll draws over the approved entries only) */
    int8_t strum_pat;       /* a STRUM_PAT index (the strummed keys) */
    int8_t no_busy_thin;    /* 0: a busy lead thins the comping (half notes, stabs, offbeats, the arpeggio: harmony.c),
                               as always; 1: they keep their full rhythm (held on 2 (and 4) and the comp rhythms never
                               thin) */
    int8_t lead_loop;       /* -1 default (follows LEAD_LOOP_DEFAULT: on), 0 the lead loop off, 1 on */
    int8_t strum_fill;      /* a strummed song's fill figure (strum_fill_drive): 0 the song's draw (left out of an
                               initializer, it stays the draw), 1 step back, 2 drive */
    int8_t comp_pat;        /* a comp rhythm forced (the keys play CS_PAT where they can): 0 none (left out of an
                               initializer, it stays none), else 1 + its COMP_PAT index, approved or candidate */
    int8_t kick;            /* the song's kick phrase (roll.h's lib_song_kick): 0 rolled (left out of an initializer,
                               it stays rolled), 1 the pattern's own, else 2 + its KICK_PHRASE index (patterns_gen.h),
                               approved or candidate */
    int8_t keys_with;       /* the keys over a low lead (lib_lead_lift: the jazz guitar, the cello): 0 rolled (left out
                               of an initializer, it stays rolled: harmony_keys_with's draw), 1 lift (an octave up in
                               its blocks: harmony_keys_lift), 2 with the lead (their own register all song, thinned
                               where it plays: harmony.c's with_lead) */
} song_ov_t;

/* The overrides by key (the workbench's rows: rows.json's ov and reroll keys), as wb_ov_set / wb_ov_get see them:
   strum_fill -1 the song's draw, 0 step back, 1 drive; comp_pat -1 none, else a COMP_PAT index; no_busy_thin 0/1;
   kick -1 rolled, 0 the pattern's own, else 1 + a KICK_PHRASE index (the device's kick row: options.json's
   kickPhrases in order); keys_with -1 rolled, 0 lift, 1 with the lead */
enum { OV_COMP, OV_LEAD_STYLE, OV_PAD_PLACE, OV_RIDE, OV_KEYS_OCT, OV_LEAD_OCT, OV_VOICING_SALT, OV_PHRASE_SALT,
       OV_BASS_SALT, OV_PAD_SALT, OV_DRUMS_SALT, OV_PAD_ON, OV_STRUM_PAT, OV_NO_BUSY_THIN, OV_LEAD_LOOP, OV_STRUM_FILL,
       OV_COMP_PAT, OV_KICK, OV_KEYS_WITH, OV_N };
void song_ov_set(song_ov_t *o, int key, int value);
int song_ov_get(const song_ov_t *o, int key);

void song_override(const song_ov_t *ov);   /* NULL: none (default); the struct is copied */
const song_ov_t *song_ov(void);            /* never NULL: a static all-rolled struct when none */
const song_ov_t *song_ov_none(void);       /* the overrides as rolled (none set): what song_override(NULL) installs */

/* seed itself when salt is 0, else a hash of the two: a part's re-rolled seed */
uint32_t song_ov_salted(uint32_t seed, uint32_t salt);
/* The lead's seed: seed itself unless phrase_salt is set */
uint32_t song_ov_phrase_seed(uint32_t seed);
/* An instrument's playable range (MIDI), for the register overrides */
void song_ov_range(int ins, int *lo, int *hi);

#endif

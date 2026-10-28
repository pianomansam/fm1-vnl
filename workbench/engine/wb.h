/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The workbench's engine glue: the song engine (app/song, app/engine) in WebAssembly, driven from the page. One song
   plays; the page stages changes (roll fields, the custom set's roles, overrides) and wb_apply lands them at the next
   bar line by catch-up: the song restarts silently with the staged state and ticks up to that bar, then sound resumes,
   so every state is as if the song had always had those decisions. A seed restarts from bar 1. */
#ifndef WB_H
#define WB_H
#include <stdint.h>

/* wb_ov_set's keys: song_ov_t's fields (override.h), in its order */
enum { WB_OV_COMP, WB_OV_LEAD_STYLE, WB_OV_PAD_PLACE, WB_OV_RIDE, WB_OV_KEYS_OCT, WB_OV_LEAD_OCT, WB_OV_VOICING_SALT,
       WB_OV_PHRASE_SALT, WB_OV_N };

#define WB_MAX_FRAMES 1024u

void wb_init(void);                     /* stopped, seed 1 rolled */
float *wb_out_l(void);
float *wb_out_r(void);
void wb_render(uint32_t n);             /* n (<= 1024) frames into the outputs; silence when stopped (after a 40 ms
                                           fade) */
void wb_seed(uint32_t seed);            /* roll a song; it restarts at bar 1 at the next render, staged changes made
                                           before then included */
void wb_play(int on);                   /* on: from the start of the bar it stopped in; off: out in 40 ms */

/* Staged (the page's view): the values the next landing applies */
int wb_roll_get(int field);
int wb_roll_set(int field, int value);  /* roll_set's rules; the value applied */
int wb_roll_dice(int field);            /* roll_dice; the new value, or -1 if no other legal one */
/* A pitched role's instrument (V_CHORD, V_BASS, V_LEAD, V_CANDY, V_PAD) in the custom set: the first edit copies the
   song's set into it and switches RF_SET to LIB_SET_WB */
void wb_role_set(int role, int ins);
int wb_role_get(int role);              /* -1 for another role */
void wb_ov_set(int key, int value);
int wb_ov_get(int key);
int wb_ov_applied(int key);             /* the register as the engine applies it (KEYS_OCT staged; LEAD_OCT as of the
                                           last landing); other keys as wb_ov_get */
int wb_kit_get(void);                   /* KIT_*: the staged song's kit */
void wb_kit_set(int kit);               /* in the custom set (as wb_role_set), played always */
void wb_kick_phrase(int a, int b);      /* a two-bar kick (step masks, bars A and B) over the staged pattern; 0, 0: off */
int wb_roll_legal(int field, int value);    /* 1 if wb_roll_set would take it */
void wb_stage_mark(void);               /* save the staged state ... */
void wb_stage_undo(void);               /* ... and put it back */
void wb_apply(void);                    /* land the staged state at the next bar line (calls before it coalesce) */

int wb_bar(void);                       /* the bar sounding, from 1 (0 before the first step) */
int wb_bars(void);
float wb_seconds(void);                 /* the song's time at the sample last rendered */
int wb_catchups(void);                  /* wb_apply's landings since wb_init (tests) */
int wb_landings(void);                  /* every landing (play's and wb_apply's): the page reads back after one */
const char *wb_state(void);             /* JSON: the staged seed, roll, roles, overrides, kit, pattern, kick phrase */
const char *wb_library(void);           /* JSON: the library's maps and progressions */
const char *wb_consts(void);            /* JSON: the RF_*, WB_OV_* and V_* indices and LIB_SET_WB, for the page */

#endif

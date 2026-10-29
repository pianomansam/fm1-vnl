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
       WB_OV_PHRASE_SALT, WB_OV_BASS_SALT, WB_OV_PAD_SALT, WB_OV_DRUMS_SALT, WB_OV_PAD_ON, WB_OV_N };

#define WB_MAX_FRAMES 1024u

void wb_init(void);                     /* stopped, seed 1 rolled */
float *wb_out_l(void);
float *wb_out_r(void);
void wb_render(uint32_t n);             /* n (<= 1024) frames into the outputs; silence when stopped (after a 40 ms
                                           fade) */
void wb_seed(uint32_t seed);            /* roll a song (staged): wb_apply lands it from bar 1 */
void wb_play(int on);                   /* on: from the start of the bar it stopped in; off: out in 40 ms */

/* Staged (the page's view): the values the next landing applies */
int wb_roll_get(int field);
int wb_roll_set(int field, int value);  /* roll_set's rules; the value applied */
int wb_roll_dice(int field);            /* roll_dice; the new value, or -1 if no other legal one */
/* A pitched role's instrument (V_CHORD, V_BASS, V_LEAD, V_PAD; V_CANDY's is kept but unplayed) in the custom set:
   the first edit copies the song's set into it and switches RF_SET to LIB_SET_WB */
void wb_role_set(int role, int ins);
int wb_role_get(int role);              /* -1 for another role */
void wb_ov_set(int key, int value);
int wb_ov_get(int key);
int wb_keys_oct_applied(void);          /* the keys' register as the engine applies it to the staged state */
int wb_comp_applied(void);              /* the keys' comping style (CS_*) as the engine applies it to the staged state */
int wb_kit_get(void);                   /* KIT_*: the staged song's kit (WB_KIT_PRESET + id: a preset) */
void wb_kit_set(int kit);               /* in the custom set (as wb_role_set), played always; the lanes cleared (a
                                           preset's value: wb_kit_preset) */
/* The drum lanes (wb_drums.h), staged with the custom set like the kit */
void wb_kit_preset(int id);             /* WB_KIT_*: a kit as shipped on every lane (a sampled one once loaded) */
int wb_lane_src(int lane, int kind, int a, int b);   /* lane (V_KICK's order) plays a source (WB_SRC_*); 0: refused */
void wb_lane_db(int lane, float db);    /* lane's level */
void wb_audition(int lane, int vel);    /* lane's staged sound now, outside the song, through the drums' chain */
void wb_kick_phrase(int a, int b);      /* a two-bar kick (step masks, bars A and B) on the staged pattern's kick lane;
                                           0, 0: the pattern's own */
/* The drum screen's grid: the custom two-bar pattern (LIB_PATTERN_WB), the song's library pattern copied into it on
   the first edit. lane: D_KICK..D_RIM; bar01: 0 (A) or 1 (B); state: 0 empty, 1 hit, 2 ghost */
void wb_pat_step(int lane, int bar01, int step, int state);
int wb_pat_get(int lane, int bar01, int step);  /* the staged pattern's step (-1: out of range) */
int wb_pat_load(int pattern_id);        /* a library pattern into the custom one, for the grid; 0: not legal here */
int wb_roll_legal(int field, int value);    /* 1 if wb_roll_set would take it */
void wb_stage_mark(void);               /* save the staged state ... */
void wb_stage_undo(void);               /* ... and put it back */
void *wb_stage_ptr(void);               /* the staged state's bytes (a probe instance takes a copy) */
int wb_stage_size(void);
int wb_lead_oct_probe(void);            /* the lead's register as the engine applies it to the staged state (runs
                                           scratch songs: a probe instance only, never the one playing) */
void wb_apply(void);                    /* commit the staged state: it lands at the next bar line (a new seed or map from
                                           bar 1; stopped, at play); staging after it isn't heard until the next */
void wb_jump(int bar);                  /* the next landing goes to bar (from 0: wb_sections' start) of the committed
                                           song, with any commit waiting (one landing, at the next bar line); stopped:
                                           where play starts */
const char *wb_sections(int heard);     /* JSON: the committed song's plan (heard: the song playing) [{"start": bar from 0, "bars", "name"}] */

/* Listening only (heard at once, kept across landings; the song unchanged): vx_mute's buses */
void wb_mute(unsigned mask);            /* a bit per bus: V_CHORD, V_BASS, V_LEAD, V_PAD, V_KICK (the drums) */
void wb_solo(int role);                 /* that bus alone (a drum voice: the drums), the mutes aside; -1 none */
uint32_t wb_heard_hash(void);           /* the events played since the last landing, (bar, step, role, note) hashed
                                           (FNV-1a over each as 32-bit values; a drum's note its lane): tests */
int wb_heard_count(void);

/* The committed song's events (a probe instance only: it runs a scratch song). The page copies the playing instance's
   committed state (wb_commit_ptr, wb_stage_size bytes) into the probe's first. Records of 8 bytes, little-endian:
   {u16 bar (from 0); u8 step; u8 role (V_*; a drum's V_KICK..V_RIDE); u8 note (a drum's: its lane, role - V_KICK);
   u8 vel; u16 len_steps (16ths, rounded)} */
void *wb_commit_ptr(void);
int wb_events(uint32_t max);            /* the count, or -1 if more than max (0: the buffer's 32768) */
uint32_t wb_events_ptr(void);
int wb_events_count(void);

int wb_bar(void);                       /* the bar sounding, from 1 (0 before the first step) */
int wb_bars(void);
float wb_seconds(void);                 /* the song's time at the sample last rendered */
int wb_catchups(void);                  /* wb_apply's landings since wb_init (tests) */
int wb_live_get(int field);             /* the song playing: a roll field (RF_N: the seed) */
const char *wb_live_state(void);         /* JSON: wb_state's fields for the song being heard (not the staged one) */
int wb_pending(void);                    /* 1: a commit is waiting for its bar line */
const char *wb_state(void);             /* JSON: the staged seed, roll, roles, overrides, kit, pattern, kick lane,
                                           drums ({preset, lanes: [kind, a, b, dB] each, kit_lanes, grid}) */
const char *wb_library(void);           /* JSON: the library's maps and progressions */
const char *wb_consts(void);            /* JSON: the RF_*, WB_OV_*, V_*, WB_SRC_* and WB_KIT_* indices and LIB_SET_WB,
                                           for the page */

#endif

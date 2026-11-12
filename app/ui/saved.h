/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The saved songs (Sam, 2026-11-12: "just plain SAVE on the song screen"): sixteen slots in the flash, one store
   object (OBJ_SONGS: app.h), each slot a fixed record of SV_BYTES. A record holds everything that makes the song play
   again as it was: its roll (the seed and every rolled value: key, scale, tempo, motion, mood, swing, map,
   progression, color, set, drum pattern, texture, length; a song's roll is not a function of its seed alone, the
   chain's key flow and novelty passes read the song before it) and the rows the screens set (the instruments, the
   kit, every override and re-roll), raw as the song had them ("as rolled" stays as rolled). A recall replays only the
   rows that differ from what the roll alone plays, so a song saved untouched starts exactly as it first did.
   Record, version 1, little-endian: [0] SV_VERSION (0: an empty slot) [1] SV_NROWS [2..5] the seed [6..35] the roll's
   RF_N values (int16; RF_SET the library set, never LIB_SET_WB) [36..37] its motion_moved [38..83] SV_ROWS' values
   (int16, in SV_ROWS' order). The object: "VNS1" (SV_MAGIC) then UI_SLOTS records.
   The flash is written only while all is silent (an erase stalls the audio: a crackle, Sam 2026-11-11): a save made
   while a song sounds waits in RAM (one record) and goes to the flash when the music stops or at the next silence
   (sv_flush); the list shows it at once. RAM: the slots' summaries (ui_slot_t, 8 B each) and that record. */
#ifndef UI_SAVED_H
#define UI_SAVED_H
#include <stdint.h>
#include "nav.h"
#include "snap.h"
#include "roll.h"

enum { SV_VERSION = 1, SV_NROWS = 23, SV_BYTES = 84, SV_HEAD = 4, SV_OBJ_BYTES = SV_HEAD + UI_SLOTS * SV_BYTES };
#define SV_MAGIC 0x31534E56u             /* "VNS1" */

typedef struct { song_roll_t roll; int16_t rows[SV_NROWS]; } sv_song_t;
extern const int8_t SV_ROWS[SV_NROWS];  /* the rows a record holds (ROW_*: every one with a request that isn't a roll
                                           field's), in rows.json's order */

/* the song shown (playing, held or ready) as a record: its roll and rows, the changes waiting for their boundary
   landed in it (the navigator's: what the screen shows marked). 0, or -1 no song */
int sv_capture(const snap_t *s, const ui_nav_t *n, sv_song_t *out);
/* SV_ROWS[i]'s value when the song plays as rolled (what a recall leaves alone); kit_rolled: the kit row a plain song
   with this roll starts on (app.c's kit_pick, else the roll's own) */
int sv_rolled(const sv_song_t *sg, int i, int kit_rolled);
void sv_pack(const sv_song_t *sg, uint8_t *b);
int sv_unpack(const uint8_t *b, sv_song_t *sg);   /* 0, or -1 an empty slot or another version */

/* the slots: read from the flash at boot (sv_boot); their summaries (the list's) */
void sv_boot(void);
const ui_slot_t *sv_slots(void);
int sv_load(int slot, sv_song_t *out);            /* 0, or UI_SAVE_EMPTY */
/* a song into a slot (-1: the one already holding the same song, else the first free one): the slot, or UI_SAVE_FULL,
   UI_SAVE_BUSY (another save waits for silence); silent: the flash may be written now */
int sv_put(int slot, const sv_song_t *sg, int silent);
int sv_erase(int slot, int silent);               /* 0, UI_SAVE_EMPTY, UI_SAVE_BUSY */
int sv_flush(int silent);                         /* what waits written, while silent: 1 written, 0 none, < 0 failed */
int sv_waiting(void);                             /* a save or delete waits for silence */
/* a record as bytes, for the USB tool (tools/fm1_songs.py): a slot's, or into a slot (an empty record: deleted) */
int sv_get_bytes(int slot, uint8_t *b);           /* 0, or UI_SAVE_EMPTY (b zeroed) */
int sv_put_bytes(int slot, const uint8_t *b, int silent);   /* the slot, or UI_SAVE_* (-5: not a record) */

#endif

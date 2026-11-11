/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The screens' navigator: the mock's Device (workbench/ui/device.js) in C. The screen stack, the cursors and pages,
   the controls' actions, the changes waiting for their 4-bar boundary (marked at once), a part's re-roll and a
   section jump waiting, the keys (a press mutes, a long press solos) and their lights, the header's message and what
   the screen reader says. What the mock asks of the workbench's engine it asks of ui_engine_t (app.c: the song's
   requests, the voices' mute, solo and levels). The value bag (values.h) reads its state. */
#ifndef UI_NAV_H
#define UI_NAV_H
#include <stdint.h>
#include "screens_gen.h"
#include "snap.h"

enum { UI_STACK = 8, UI_MSG = 24, UI_SAID = 160, UI_LONG_MS = 500, UI_KEYS = 27 };

/* the mock's engine: a row's change (sent in the bar before its boundary, as the mock sends it; the song lands it
   there), play / stop, a new song, a part's re-roll, a jump to a section's first bar, the listening (mute, solo:
   -1 none) and the mixer (levels, dB). Parts are UI_PART_*. Any may be NULL. */
typedef struct {
    void (*set)(int row, int value);
    void (*play)(void);
    void (*reroll)(void);
    void (*reroll_part)(int part);
    void (*jump)(int start, int i);
    void (*mute)(int part, int on);
    void (*solo)(int part);
    int (*muted)(int part);
    int (*soloed)(void);                 /* the part soloed, -1 none */
    void (*level)(int part, int db);
    int (*level_get)(int part);
} ui_engine_t;

typedef struct {
    uint8_t stack[UI_STACK], depth;      /* the breadcrumb: stack[depth - 1] the screen shown (depth >= 1) */
    uint8_t home;                        /* the home (HOME's, a new song's): UI_HOME, or a go's with home (the app keeps it) */
    uint8_t spin;                        /* a record's turn, 1/256 turns (the app's: {spin}) */
    int8_t cursor[UI_MAX_SCREENS];       /* each screen's highlighted line (a menu, the lanes, the sections) */
    int8_t off[UI_MAX_SCREENS];          /* a scrolling list's first line shown (listOff) */
    int16_t req_val[UI_N_FIELDS];        /* a field's change waiting (the mock's req): its value, the bar it lands on */
    int16_t req_at[UI_N_FIELDS];         /* (from 0; -1 none), whether it was sent, and when it was made (the */
    uint8_t req_sent[UI_N_FIELDS];       /* earliest made first among those landing together, as the mock's order) */
    uint16_t req_seq[UI_N_FIELDS], seq;
    int8_t roll_part;                    /* a part's re-roll waiting (SEQ on its screen): UI_PART_*, -1 none */
    int16_t roll_at;
    uint8_t roll_screen, roll_sent;      /* the screen it was asked on (its title names it) */
    int8_t jump_i;                       /* a section jump waiting: the section, -1 none */
    int16_t jump_at, jump_start, jump_sent_bar;
    uint8_t jump_sent;
    uint8_t held_action[UI_KEYS];        /* a key held whose action has a long press: its action (0: none), when */
    uint8_t held_fired[UI_KEYS];         /* it went down, whether the long press acted */
    uint32_t held_t[UI_KEYS];
    char msg[2][UI_MSG];                 /* the header's message (app_say), until msg_until (ms; 0 none) */
    uint32_t msg_until;
    uint32_t now;                        /* ms, as last told */
    char said[UI_SAID];                  /* what the screen reader was last given */
    uint32_t seed;                       /* the song last seen (a new one: home, nothing waits) */
    uint8_t seen;
    const snap_t *live;                  /* the snapshot last seen (ui_update) */
    const ui_engine_t *eng;
} ui_nav_t;

void ui_nav_init(ui_nav_t *n);           /* the home screen (UI_HOME), nothing waiting */
/* the song as the screen sees it now (the mock's Device.update): requests sent in the bar before their boundary,
   done past it; a new song back to the main screen, nothing waiting; the time (ms) */
void ui_update(ui_nav_t *n, const snap_t *s, uint32_t ms);
void ui_press(ui_nav_t *n, int control, int detents);     /* a button (detents 1) or an encoder turned */
void ui_key(ui_nav_t *n, int key, int down, uint32_t ms); /* a key (0..26) down or up */
void ui_tick(ui_nav_t *n, uint32_t ms);                   /* a key held past UI_LONG_MS: its long action, once */
void ui_new_song(ui_nav_t *n);                            /* back to the main screen */
/* the keys lit (the return) and blinking (*blink), bit k key k, on the screen shown */
uint32_t ui_key_lights(const ui_nav_t *n, const snap_t *s, uint32_t *blink);
void ui_speak(ui_nav_t *n);                               /* the screen's say, into said */
int ui_list_off(ui_nav_t *n);                             /* the list's first line shown (it follows the highlight) */

#endif

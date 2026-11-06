/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The screens' navigator: the mock's Device (workbench/ui/device.js) in C. Its state here; its functions (presses,
   keys, the long press, the lights) come with nav.c. The value bag (values.h) reads the stack, the cursors and what
   waits. */
#ifndef UI_NAV_H
#define UI_NAV_H
#include <stdint.h>
#include "screens_gen.h"

enum { UI_STACK = 8, UI_MSG = 24 };

typedef struct {
    uint8_t stack[UI_STACK], depth;      /* the breadcrumb: stack[depth - 1] the screen shown (depth >= 1) */
    int8_t cursor[UI_MAX_SCREENS];       /* each screen's highlighted line (a menu, the lanes, the sections) */
    int16_t req_val[UI_N_FIELDS];        /* a field's change waiting (the mock's req): its value and the bar it lands */
    int16_t req_at[UI_N_FIELDS];         /* on (from 0); -1 none */
    int8_t roll_part;                    /* a part's re-roll waiting (SEQ on its screen): UI_PART_*, -1 none */
    int16_t roll_at;
    uint8_t roll_screen;                 /* the screen it was asked on (its title names it) */
    int8_t jump_i;                       /* a section jump waiting: the section, -1 none */
    int16_t jump_at;
    char msg[2][UI_MSG];                 /* the header's message (app_say), until msg_until (ms; 0 none) */
    uint32_t msg_until;
    uint32_t now;                        /* ms, as last told */
} ui_nav_t;

void ui_nav_init(ui_nav_t *n);           /* the home screen, nothing waiting */

#endif

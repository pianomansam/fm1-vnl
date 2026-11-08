/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The screens' drawing: a screen's bands from its values (the mock's drawScreen). Part of the unity build: render.c
   draws with gfx.c's canvas (cv_begin, cv_rect, cv_text, cv_blit, text_w). */
#ifndef UI_RENDER_H
#define UI_RENDER_H
#include <stdint.h>
#include "screens_gen.h"
#include "values.h"

/* the screen's bands whose bit is set in band_mask (bit i: band i; all on a screen change, else those whose values
   changed), each drawn whole and blitted */
void ui_draw(const ui_screen_t *sc, const ui_bag_t *b, uint32_t band_mask);
void ui_draw_all(void);                  /* the next draw sends every band (the panel was cleared) */

#endif

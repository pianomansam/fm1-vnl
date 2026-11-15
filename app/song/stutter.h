/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The whole-mix stutter's placement (stutter.c): which bars of a song stutter, and in which figure. The engine plays
   the figure (voices.c's vx_stutter_fig); the bar and figure ride in the lead's state (lead.h's loop.sbar, sfig),
   which the song plans a bar ahead. */
#ifndef SONG_STUTTER_H
#define SONG_STUTTER_H
#include "lead.h"

/* the figures: 1-2 simple (an 8th repeat, a 16th roll), 3-4 complex (an accelerating roll, the 2-beat catch) */
enum { STUT_SIMPLE = 1, STUT_COMPLEX = 2 };   /* the kinds (ls->loop.sq: a stutter queued on demand) */

/* 1 where bar (b: its plan block) is the last bar of a group as the screen shows it (8-bar groups from the block's
   start, a 4-bar tail its own) */
int stutter_group_last(const plan_block_t *b, int bar);
/* 1 where a move at the section's end (a fill or a stutter on demand) may land on bar (b: its plan block, in the
   plan's array): its section's last bar, never the outro's nor the song's last */
int move_lands(const plan_block_t *b, int bar);
/* a figure's catch: the 16th (0-15) of its bar it starts on (vx_stutter_fig's first step) */
int stutter_catch(int fig);
/* the figure the song's own draws give bar (b: its plan block, of the song's plan), 0 none */
int stutter_song(const song_roll_t *r, const lead_state_t *ls, const plan_block_t *b, int bar);
/* the figure bar will play as the song stands: the one queued (ls->loop.sq) where it lands there, else the song's */
int stutter_at(const song_roll_t *r, const lead_state_t *ls, const plan_block_t *b, int bar);
/* a figure of kind (STUT_*) drawn for bar, evenly within the kind */
int stutter_draw(const song_roll_t *r, int kind, int bar);
/* bar's stutter into ls (loop.sbar, sfig, sod): the queued one where it lands there (the queue emptied), else the
   song's; lead_plan_bar calls it for every bar */
void stutter_plan_bar(lead_state_t *ls, const song_roll_t *r, const plan_block_t *b, int bar);

#ifdef LEADS_LOOP_STATS
/* the tests' and the host's: per bar of the song last planned, its figure (bits 0-2) and 8 where it came on demand */
int stutter_log(int bar);
/* a test hook (host renders: VNL_STUT=1): 1 stutters every group's last bar a simple stutter may take, figures 1-4
   in turn; 0 the draws */
void stutter_force(int on);
#endif

#endif

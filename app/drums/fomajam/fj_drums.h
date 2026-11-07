/* SPDX-License-Identifier: GPL-3.0-only */
/* FOMAJAM's modeled drum machines (fj_drums.c, adapted from FOMAJAM's src/drums.c: integer tables of poketrack's
   trommelsynthesizer), rendered a voice at a time for the drum lanes (wb_drums.c), the knobs at the middle. The
   machines and voices are FOMAJAM's (its drums.h). 909 and SDS-V are there too, unused yet. Integers only, so the
   FM-1 and the workbench render the same samples. */
#ifndef FJ_DRUMS_H
#define FJ_DRUMS_H
#include <stdint.h>

enum { V_BD, V_BD2, V_SD, V_SD2, V_RS, V_CP, V_LT, V_MT, V_HT, V_CH, V_OH, V_CY, V_RD, V_CB, V_CB2, V_TB, V_LC, V_MC,
       V_HC, V_TI, V_MA, V_CL, V_COUNT };
enum { M_808, M_909, M_SDS, M_CR78, M_MODELLED };
int fj_has(int machine, int voice);              /* 1 if the machine has the voice */
/* a voice's sound, rendered once (the lanes play it back: wb_drums.c): fj_voice_begin, then fj_voice_next for its
   samples (Q15, at full velocity: a voice's tone doesn't move with velocity, only its level), up to n at a time;
   fewer once it has ended. One voice at a time; the same samples every time. */
void fj_voice_begin(int machine, int voice);
uint32_t fj_voice_next(int16_t *out, uint32_t n);

#endif

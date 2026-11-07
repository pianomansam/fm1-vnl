/* SPDX-License-Identifier: GPL-3.0-only */
/* FOMAJAM's modeled drum machines (fj_drums.c, adapted from FOMAJAM's src/drums.c: integer tables of poketrack's
   trommelsynthesizer), as the drum lanes play them (wb_drums.c): a voice a slot, the knobs at the middle. The
   machines and voices are FOMAJAM's (its drums.h). 909 and SDS-V are there too, unused yet. Integers only, so the
   FM-1 and the workbench render the same samples. */
#ifndef FJ_DRUMS_H
#define FJ_DRUMS_H
#include <stdint.h>

enum { V_BD, V_BD2, V_SD, V_SD2, V_RS, V_CP, V_LT, V_MT, V_HT, V_CH, V_OH, V_CY, V_RD, V_CB, V_CB2, V_TB, V_LC, V_MC,
       V_HC, V_TI, V_MA, V_CL, V_COUNT };
enum { M_808, M_909, M_SDS, M_CR78, M_MODELLED };
#define FJ_SLOTS 8

void fj_seed(uint32_t seed);                     /* the voices' shared noise from seed (a song's start) */
int fj_has(int machine, int voice);              /* 1 if the machine has the voice */
void fj_start(int slot, int machine, int voice, int vel);   /* (re)starts the slot's voice, vel 1-127 */
void fj_stop(int slot);
/* the slot's next n samples into x (-1..1; 0 once it has ended); 0 once ended */
int fj_render(int slot, float *x, uint32_t n);

#endif

/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The workbench's engine (workbench/engine/wb.c) built natively, for workbench/tools/kits_native.mjs: the same calls
   as the page's on wb.wasm, so the two renders can be held to each other sample for sample (the drum lanes' float
   code: FOMAJAM's machines, Optimist's, the bank's kits). Usage: wb_native SECONDS SEED... For each seed: the song
   rolled, its kit as the device picks it (wb_kit_pick: a retired one swapped, as desk.js does), played from bar 1;
   prints "seed kit hash" (FNV-1a over the left and right outputs' float bits, 128-frame quanta). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wb.h"

int wb_kit_pick(void);
void wb_kit_set(int kit);
void wb_apply(void);

int main(int argc, char **argv)
{
    int i, secs = argc > 1 ? atoi(argv[1]) : 20;
    for (i = 2; i < argc; i++) {
        uint32_t seed = (uint32_t)strtoul(argv[i], 0, 10), h = 2166136261u, n, k, u;
        int kit;
        wb_init();
        wb_seed(seed);
        kit = wb_kit_pick();
        if (kit >= 0)
            wb_kit_set(kit);
        wb_apply();
        wb_play(1);
        for (n = 0; n < (uint32_t)secs * 44100u; n += 128u) {
            wb_render(128u);
            for (k = 0; k < 128u; k++) {
                memcpy(&u, &wb_out_l()[k], 4);
                h = (h ^ u) * 16777619u;
                memcpy(&u, &wb_out_r()[k], 4);
                h = (h ^ u) * 16777619u;
            }
        }
        printf("%u %d %u\n", (unsigned)seed, kit, (unsigned)h);
    }
    return 0;
}

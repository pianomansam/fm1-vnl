/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The seeded kit pick (values.h ui_kit_pick): its own file, so the workbench's wasm builds it too (wb_kit_pick: its
   smoke test holds it to desk.js's approvedKit) */
#include "values.h"
#include "choices_gen.h"
#include "library.h"

int ui_kit_pick(const song_roll_t *r, int sampled_ok)
{
    const lib_set_t *set = lib_set_by_id((uint8_t)r->v[RF_SET]);
    int kit = lib_song_kit(r), fam = CH_ACOUSTIC, n = 0, i, xs[16];
    uint32_t x = r->seed ? r->seed : 1u, q;
    if (ch_flags(ROW_KIT, kit) < 0 || CH_STATUS(ch_flags(ROW_KIT, kit)) != 2)
        return -1;
    if (set) {
        int fk = CH_FAM(ch_flags(ROW_KEYS, set->chords)), fl = CH_FAM(ch_flags(ROW_LEAD, set->lead));
        if (ch_flags(ROW_KEYS, set->chords) >= 0 && (fk == CH_ACOUSTIC || fk == CH_SYNTH))
            fam = fk;
        else if (ch_flags(ROW_LEAD, set->lead) >= 0 && (fl == CH_ACOUSTIC || fl == CH_SYNTH))
            fam = fl;
    }
    for (i = 0; i < ch_count(ROW_KIT) && n < 16; i++) {   /* approved, of the family (a kit has its own: no electric) */
        int v = ch_value(ROW_KIT, i), f = ch_flags(ROW_KIT, v);
        if (CH_STATUS(f) == 0 && !(f & CH_HIDDEN) && v != kit && CH_FAM(f) == fam &&
            (sampled_ok || v < SONG_KIT_PRESET + SONG_KIT_SAMPLED))
            xs[n++] = v;
    }
    if (!n)
        return -1;
    x = x * 1103515245u + 12345u;        /* floor(x / 2^32 * n), exactly, in 32 bits */
    q = ((x >> 16) * (uint32_t)n + (((x & 0xFFFFu) * (uint32_t)n) >> 16)) >> 16;
    return xs[q % (uint32_t)n];
}

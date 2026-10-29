/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "override.h"
#include "library.h"

static const song_ov_t NONE = {-1, -1, -1, -1, 0, 0, 0u, 0u, 0u, 0u, 0u, -1};
static song_ov_t cur = {-1, -1, -1, -1, 0, 0, 0u, 0u, 0u, 0u, 0u, -1};

void song_override(const song_ov_t *ov) { cur = ov ? *ov : NONE; }

const song_ov_t *song_ov(void) { return &cur; }

uint32_t song_ov_salted(uint32_t seed, uint32_t salt)
{
    uint32_t h;
    if (!salt) return seed;
    h = seed ^ salt * 0x9E3779B1u;
    h ^= h >> 16; h *= 0x85EBCA6Bu; h ^= h >> 13; h *= 0xC2B2AE35u; h ^= h >> 16;
    return h;
}

uint32_t song_ov_phrase_seed(uint32_t seed) { return song_ov_salted(seed, cur.phrase_salt); }

/* roughly each instrument's real compass (the samples' and synths' useful range); synths otherwise C1-C8 */
void song_ov_range(int ins, int *lo, int *hi)
{
    switch (ins) {
    case INS_RHODES: case INS_DX_EP: *lo = 28; *hi = 103; break;          /* E1-G7 */
    case INS_WURLI: *lo = 33; *hi = 96; break;                            /* A1-C7 */
    case INS_FELT_PIANO: case INS_KNIGHT: *lo = 21; *hi = 108; break;     /* A0-C8 */
    case INS_NYLON: *lo = 40; *hi = 83; break;                            /* E2-B5 */
    case INS_EMILY: case INS_GUITAR_LICK: *lo = 40; *hi = 86; break;      /* E2-D6 */
    case INS_FLUTE: *lo = 60; *hi = 96; break;                            /* C4-C7 */
    case INS_MUTED_TPT: *lo = 52; *hi = 82; break;                        /* E3-Bb5 */
    case INS_SAX: *lo = 49; *hi = 81; break;                              /* alto: Db3-A5 */
    case INS_VIBES: *lo = 53; *hi = 89; break;                            /* F3-F6 */
    case INS_KALIMBA: *lo = 60; *hi = 88; break;
    case INS_MUSIC_BOX: *lo = 60; *hi = 96; break;
    case INS_BELLS: *lo = 60; *hi = 108; break;
    case INS_CELLO: *lo = 36; *hi = 76; break;                            /* C2-E5 */
    case INS_STR_LEAD: *lo = 55; *hi = 100; break;                        /* violin: G3-E7 */
    default: *lo = 24; *hi = 108; break;
    }
}

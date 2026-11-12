/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "override.h"
#include "library.h"

static const song_ov_t NONE = {-1, -1, -1, -1, 0, 0, 0u, 0u, 0u, 0u, 0u, -1, -1, 0, -1, 0, 0, 0, 0};
static song_ov_t cur = {-1, -1, -1, -1, 0, 0, 0u, 0u, 0u, 0u, 0u, -1, -1, 0, -1, 0, 0, 0, 0};

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
    case INS_RHODES: *lo = 28; *hi = 103; break;                          /* E1-G7 */
    case INS_FELT_PIANO: case INS_KNIGHT: *lo = 21; *hi = 108; break;     /* A0-C8 */
    case INS_NYLON: *lo = 40; *hi = 83; break;                            /* E2-B5 */
    case INS_EMILY: *lo = 40; *hi = 86; break;                            /* E2-D6 */
    case INS_JAZZ_GTR: *lo = 54; *hi = 87; break;                         /* F#3-Eb6: its zones A3-C6, 3
                                                                             semitones either way (it leads an octave
                                                                             low, leads.c) */
    case INS_SAMP_CELLO: *lo = 42; *hi = 72; break;                       /* F#2-C5: its zones A2-A4, 3
                                                                             semitones either way (a low lead) */
    case INS_HARP: *lo = 24; *hi = 103; break;                            /* C1-G7: a concert harp's */
    case INS_WHISTLE: *lo = 69; *hi = 96; break;                          /* A4-C7: a whistler's (its floor
                                                                             about A4-C5) */
    case INS_SOFT_SYNTH: *lo = 48; *hi = 96; break;                       /* C3-C7 (a low lead: its melody
                                                                             C4-C6) */
    case INS_FLUTE: *lo = 60; *hi = 96; break;                            /* C4-C7 */
    case INS_VIBES: *lo = 53; *hi = 89; break;                            /* F3-F6 */
    default: *lo = 24; *hi = 108; break;
    }
}

void song_ov_set(song_ov_t *o, int key, int value)
{
    switch (key) {
    case OV_COMP: o->comp = (int8_t)value; break;
    case OV_LEAD_STYLE: o->lead_style = (int8_t)value; break;
    case OV_PAD_PLACE: o->pad_place = (int8_t)value; break;
    case OV_RIDE: o->ride = (int8_t)value; break;
    case OV_KEYS_OCT: o->keys_oct = (int8_t)value; break;
    case OV_LEAD_OCT: o->lead_oct = (int8_t)value; break;
    case OV_VOICING_SALT: o->voicing_salt = (uint32_t)value; break;
    case OV_PHRASE_SALT: o->phrase_salt = (uint32_t)value; break;
    case OV_BASS_SALT: o->bass_salt = (uint32_t)value; break;
    case OV_PAD_SALT: o->pad_salt = (uint32_t)value; break;
    case OV_DRUMS_SALT: o->drums_salt = (uint32_t)value; break;
    case OV_PAD_ON: o->pad_on = (int8_t)value; break;
    case OV_STRUM_PAT: o->strum_pat = (int8_t)value; break;
    case OV_NO_BUSY_THIN: o->no_busy_thin = (int8_t)(value != 0); break;
    case OV_LEAD_LOOP: o->lead_loop = (int8_t)value; break;
    case OV_STRUM_FILL: o->strum_fill = (int8_t)(value == 0 || value == 1 ? value + 1 : 0); break;   /* -1: drawn */
    case OV_COMP_PAT: o->comp_pat = (int8_t)(value >= 0 && value < 127 ? value + 1 : 0); break;   /* -1: none */
    case OV_KICK: o->kick = (int8_t)(value >= 0 && value < 126 ? value + 1 : 0); break;   /* -1: rolled */
    case OV_KEYS_WITH: o->keys_with = (int8_t)(value == 1); break;
    default: break;
    }
}

int song_ov_get(const song_ov_t *o, int key)
{
    switch (key) {
    case OV_COMP: return o->comp;
    case OV_LEAD_STYLE: return o->lead_style;
    case OV_PAD_PLACE: return o->pad_place;
    case OV_RIDE: return o->ride;
    case OV_KEYS_OCT: return o->keys_oct;
    case OV_LEAD_OCT: return o->lead_oct;
    case OV_VOICING_SALT: return (int)o->voicing_salt;
    case OV_PHRASE_SALT: return (int)o->phrase_salt;
    case OV_BASS_SALT: return (int)o->bass_salt;
    case OV_PAD_SALT: return (int)o->pad_salt;
    case OV_DRUMS_SALT: return (int)o->drums_salt;
    case OV_PAD_ON: return o->pad_on;
    case OV_STRUM_PAT: return o->strum_pat;
    case OV_NO_BUSY_THIN: return o->no_busy_thin;
    case OV_LEAD_LOOP: return o->lead_loop;
    case OV_STRUM_FILL: return o->strum_fill - 1;   /* -1 the song's draw, 0 step back, 1 drive */
    case OV_COMP_PAT: return o->comp_pat - 1;       /* -1 none, else a COMP_PAT index */
    case OV_KICK: return o->kick - 1;               /* -1 rolled, 0 the pattern's own, else 1 + a KICK_PHRASE index */
    case OV_KEYS_WITH: return o->keys_with;
    default: return 0;
    }
}

/* SPDX-License-Identifier: MPL-2.0 */
/* The seed library: curated "good guesses" (progressions, instrument sets, drum patterns, song maps, moods).
   Every entry has a stable id (never reused) and a source note, so it can be curated by ear later. */
#ifndef SONG_LIBRARY_H
#define SONG_LIBRARY_H
#include <stdint.h>
#include "theory.h"

enum { L_KEYS = 1, L_KICK = 2, L_SNARE = 4, L_HATS = 8, L_BASS = 16, L_LEAD = 32, L_CANDY = 64, L_BED = 128 }; /* L_BED: Plan 3 */
enum { MOOD_SAD = 1, MOOD_WISTFUL = 2, MOOD_WARM = 4, MOOD_SUNNY = 8 };      /* bits; LIB_MOODS is indexed 0..3 in this order */
enum { PK_VAMP, PK_LOOP, PK_JAZZY };

enum { INS_RHODES, INS_WURLI, INS_FELT_PIANO, INS_NYLON, INS_PAD, INS_UPRIGHT, INS_ELEC_BASS, INS_SUB, INS_FLUTE,
       INS_MUTED_TPT, INS_SAX, INS_VIBES, INS_KALIMBA, INS_GUITAR_LICK, INS_MUSIC_BOX, INS_BELLS, INS_VOX_PAD, INS_NONE };
enum { KIT_DUSTY, KIT_SOFT, KIT_808, KIT_BRUSHES };
enum { FL_LOFI, FL_BOSSA, FL_CITYPOP, FL_NEOSOUL, FL_CHILLWAVE };
enum { D_KICK, D_SNARE, D_HAT, D_OHAT, D_SHAKER, D_RIM };
enum { PF_BOOMBAP, PF_HALFTIME, PF_SPARSE, PF_BRUSHES, PF_BOSSA };
enum { EN_TOGETHER, EN_SNARE_FIRST, EN_FROM_TOP };
enum { MID_BREAK, MID_KICK_OUT, MID_HATS_ONLY, MID_NONE };
enum { END_KEYS_OUTRO, END_FADE, END_RING };

typedef struct { uint8_t id; uint8_t kind; uint8_t n; uint8_t bars_per_chord; th_chord_spec_t c[4]; uint8_t moods; uint8_t modes; const char *src; } lib_prog_t;
/* modes: bit (1 << MODE_x) for each mode whose degrees give the chords the note claims; moods: MOOD_* bits. n * bars_per_chord is 2, 4 or 8. */
typedef struct { uint8_t id; const char *name; uint8_t chords, bass, lead, candy, kit; uint8_t flavor; uint8_t moods; const char *src; } lib_set_t;
/* hit/ghost: 16 steps per lane, bit i = step i. poly3: the shaker lane is a period-3 polymeter (hit[D_SHAKER] holds it). */
typedef struct { uint8_t id; uint8_t family; uint16_t hit[6]; uint16_t ghost[6]; uint8_t half_time; uint8_t poly3; const char *src; } lib_pattern_t;
typedef struct { uint8_t id; const char *name; uint8_t intro_bars, entry, middle, ending; uint8_t sparse; const char *src; } lib_map_t;
typedef struct { uint8_t mode_w[MODE_N]; uint8_t color_w[3]; int8_t lead_shift; uint8_t candy_pct; } lib_mood_t;

extern const lib_prog_t LIB_PROGS[];       extern const int LIB_NPROGS;
extern const lib_set_t LIB_SETS[];         extern const int LIB_NSETS;
extern const lib_pattern_t LIB_PATTERNS[]; extern const int LIB_NPATTERNS;
extern const lib_map_t LIB_MAPS[];         extern const int LIB_NMAPS;
extern const lib_mood_t LIB_MOODS[4];      /* sad, wistful, warm, sunny */

const lib_prog_t *lib_prog_by_id(uint8_t id);        /* all four: NULL if unknown */
const lib_set_t *lib_set_by_id(uint8_t id);
const lib_pattern_t *lib_pattern_by_id(uint8_t id);
const lib_map_t *lib_map_by_id(uint8_t id);

#endif

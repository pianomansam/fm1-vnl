/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The seed library: curated "good guesses" (progressions, instrument sets, drum patterns, song maps, moods).
   Every entry has a stable id (never reused) and a source note, so it can be curated by ear later. */
#ifndef SONG_LIBRARY_H
#define SONG_LIBRARY_H
#include <stdint.h>
#include "theory.h"

enum { L_KEYS = 1, L_KICK = 2, L_SNARE = 4, L_HATS = 8, L_BASS = 16, L_LEAD = 32, L_CANDY = 64, L_BED = 128 }; /* L_BED: Plan 3 */
enum { MOOD_SAD = 1, MOOD_WISTFUL = 2, MOOD_WARM = 4, MOOD_SUNNY = 8 };      /* bits; LIB_MOODS is indexed 0..3 in this order */
enum { PK_VAMP, PK_LOOP, PK_JAZZY };

/* INS_WURLI and INS_DX_EP: removed (Sam, 2026-11-04): no patch, no set; ids never reused (lib_keys_approved) */
enum { INS_RHODES, INS_WURLI, INS_FELT_PIANO, INS_NYLON, INS_PAD, INS_UPRIGHT, INS_ELEC_BASS, INS_SUB, INS_FLUTE,
       INS_MUTED_TPT, INS_SAX, INS_VIBES, INS_KALIMBA, INS_GUITAR_LICK, INS_MUSIC_BOX, INS_BELLS, INS_VOX_PAD,
       INS_CHIP_BASS, INS_ORGAN, INS_SAW_LEAD, INS_SYNTH_PLUCK, INS_DX_EP, INS_FM_BASS,
       INS_STRINGS, INS_WARM_PAD, /* the pad layer's (V_PAD) */
       INS_KNIGHT, INS_EMILY,     /* sampled (M_SAMP; INS_NYLON too): the Knight upright, the Emilyguitar */
       INS_STR_LEAD, INS_CELLO,   /* removed (the string synth's leads), as are the muted trumpet, sax and guitar lick
                                     (Sam, 2026-10-29): no patch, no set; ids never reused (lib_lead_approved) */
       INS_CHOIR,                 /* the formant choir: a pad layer the song draws over its set's (lib_song_pad), and
                                     one of the pad's solo voices (pads_solo_voice). The vox pad (INS_VOX_PAD) it
                                     replaced is removed (Sam, 2026-10-31): no patch, no set; its id never reused */
       INS_JAZZ_GTR,              /* sampled (M_SAMP): the jazz guitar, an archtop's pickup (a lead, a candidate
                                     2026-11-10, approved 2026-11-11: sets 38-40) */
       INS_SAMP_CELLO,            /* leads (2026-11-11, approved: sets 41-45): the sampled cello (M_SAMP, the Karoryfer x Bigcat
                                     cello's bowing; the synth cello parked 2026-11-11: no id), */
       INS_HARP,                  /* the harp (M_SAMP: VSCO-2 CE's concert harp; the modal retone gone) */
       INS_NONE }; /* append new ones before INS_NONE */
/* the synth kits (drums.c), then the mixed sampled kits (retired; their hits left the bank 2026-11-10: the ids stay, a
   sampled kit's song's mark under a shipped sampled kit, song.c; their own hits are silent, drums.c) */
enum { KIT_DUSTY, KIT_SOFT, KIT_808, KIT_BRUSHES, KIT_S_WARM, KIT_S_DARK, KIT_S_RUSTY, KIT_S_GENTLE, KIT_S_CLEAN, KIT_N };
enum { FL_LOFI, FL_BOSSA, FL_CITYPOP, FL_NEOSOUL, FL_CHILLWAVE };
enum { D_KICK, D_SNARE, D_HAT, D_OHAT, D_SHAKER, D_RIM };
enum { PF_BOOMBAP, PF_HALFTIME, PF_SPARSE, PF_BRUSHES, PF_BOSSA };
enum { EN_TOGETHER, EN_SNARE_FIRST, EN_FROM_TOP };
enum { MID_BREAK, MID_KICK_OUT, MID_HATS_ONLY, MID_NONE };
enum { END_KEYS_OUTRO, END_FADE, END_RING };

typedef struct { uint8_t id; uint8_t kind; uint8_t n; uint8_t bars_per_chord; th_chord_spec_t c[4]; uint8_t moods; uint8_t modes; const char *src; } lib_prog_t;
/* modes: bit (1 << MODE_x) for each mode whose degrees give the chords the note claims; moods: MOOD_* bits. n * bars_per_chord is 2, 4 or 8. */
/* pad: the pad layer's instrument (INS_STRINGS or INS_WARM_PAD; a song may draw the choir over it: lib_song_pad).
   candy (and lib_mood_t's candy_pct, and L_CANDY): unused since the ear candy went (Sam, 2026-10-29), kept as the curation's record */
/* kit: the synth kit (KIT_N: none, the skit only); skit: the sampled kit (KIT_N: none, the synth kit always) */
typedef struct { uint8_t id; const char *name; uint8_t chords, bass, lead, candy, pad, kit, skit; uint8_t flavor; uint8_t moods; const char *src; } lib_set_t;
/* hit/ghost: 16 steps per lane, bit i = step i. poly3: the shaker lane is a period-3 polymeter (hit[D_SHAKER] holds it). */
typedef struct { uint8_t id; uint8_t family; uint16_t hit[6]; uint16_t ghost[6]; uint8_t half_time; uint8_t poly3; const char *src;
                     uint8_t two_bar; uint16_t hit_b[6]; uint16_t ghost_b[6]; } lib_pattern_t;
typedef struct { uint8_t id; const char *name; uint8_t intro_bars, entry, middle, ending; uint8_t sparse; const char *src; } lib_map_t;
typedef struct { uint8_t mode_w[MODE_N]; uint8_t color_w[3]; int8_t lead_shift; uint8_t candy_pct; } lib_mood_t;

/* 1 for a sampled kit (KIT_S_*), else 0 */
int lib_kit_sampled(int kit);

extern const lib_prog_t LIB_PROGS[];       extern const int LIB_NPROGS;
extern const lib_set_t LIB_SETS[];         extern const int LIB_NSETS;
extern const lib_pattern_t LIB_PATTERNS[]; extern const int LIB_NPATTERNS;
extern const lib_map_t LIB_MAPS[];         extern const int LIB_NMAPS;
extern const lib_mood_t LIB_MOODS[4];      /* sad, wistful, warm, sunny */

const lib_prog_t *lib_prog_by_id(uint8_t id);        /* all four: NULL if unknown */
/* the workbench's custom set and pattern (default off: NULL); lib_*_by_id(LIB_*_WB) return them */
enum { LIB_SET_WB = 255, LIB_PATTERN_WB = 255 };
void lib_set_wb(const lib_set_t *s, uint8_t base_id);   /* base_id: the set whose id-keyed rules (walking bass, keys shift) it keeps */
void lib_pattern_wb(const lib_pattern_t *p);
/* a lane's hit and ghost masks in `bar`: bar B's on odd bars of a two-bar pattern, else bar A's */
unsigned lib_lane_hits(const lib_pattern_t *p, int lane, int bar);
unsigned lib_lane_ghosts(const lib_pattern_t *p, int lane, int bar);
const lib_set_t *lib_set_by_id(uint8_t id);
/* 1: slow attack, held breath or a held tone (pads, the flute, the organ, the strings, the choir, the sampled cello); 0:
   struck, plucked, the triangle bass, the FM bass */
int lib_ins_sustained(int ins);
/* a removed or retired lead (the muted trumpet, sax, guitar lick, string lead, cello; the kalimba, saw lead, synth
   pluck and Emilyguitar as leads): the flute or the vibes in its place; else ins */
int lib_lead_approved(int ins);
/* a removed bass (the upright): the electric bass in its place; else ins */
int lib_bass_approved(int ins);
/* a removed keys (the Wurli, the DX EP: 2026-11-04): the Rhodes in its place; else ins */
int lib_keys_approved(int ins);
/* 1: a sampled struck-string instrument (the Knight, the nylon, the Emilyguitar, the jazz guitar): stabs and strums
   suit it (on the keys); none plays without the sample bank */
int lib_ins_sampled_struck(int ins);
/* 1: a sampled instrument (lib_ins_sampled_struck's, the sampled cello, the harp): none plays without the bank */
int lib_ins_sampled(int ins);
/* A lead under the others' register: the semitones its style centers sit under the flute's (leads.c), 0 for none:
   the jazz guitar 12 (Sam 2026-11-10: "this octave does work"), the sampled cello 24 (Sam 2026-11-11: an
   octave under round 1's, a cello's melody G2-E4, its top A4). lib_lead_low: 1 for any of them (its ceiling down,
   E5 or A4; its patch's chorus its send into the keys', voices.c) */
int lib_lead_drop(int ins);
int lib_lead_low(int ins);
/* 1: a low lead the keys lift over (the jazz guitar, the sampled cello): in its blocks the keys and the pad's chord part an
   octave up where the workbench leaves the keys' register as rolled (harmony_keys_lift; Sam: "the other instruments
   should have gone up an octave"), the lead's top under the keys' lowest note (leads.c's range_bar) */
int lib_lead_lift(int ins);
/* the sample bank (engine/samples.h): 1 mounted (the default), 0 none (the app's boot, or a bank being written). With
   none the songs keep to synth instruments: no set with sampled keys, bass or lead is drawn or taken, a set's kit is
   its synth one, and a sampled instrument or kit asked for by hand is refused */
void lib_set_samples(int on);
int lib_samples(void);
int lib_set_playable(const lib_set_t *s);   /* its keys, bass and lead, and a kit, play without the bank, or it is
                                               mounted */
int lib_set_walks(const lib_set_t *s);
int lib_set_keys_shift(const lib_set_t *s);          /* semitones the keys sit from their usual octave */               /* the bass may walk in this set */
const lib_pattern_t *lib_pattern_by_id(uint8_t id);
const lib_map_t *lib_map_by_id(uint8_t id);

#endif

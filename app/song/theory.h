/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Music theory for the song engine: major and minor keys, the scale over a chord, chords with colors, voicing and voice
   leading, guide tones, avoid notes. */
#ifndef SONG_THEORY_H
#define SONG_THEORY_H
#include <stdint.h>

/* A song is major or minor (Sam, 2026-11-05: "easier writing and approving of progressions. plus reasoning about
   what i'm hearing"); the six modes before it (Ionian 0 and Dorian 1 kept their values as major and minor). */
enum { MODE_MAJOR, MODE_MINOR, MODE_N };
enum { COLOR_7, COLOR_9, COLOR_RICH };                     /* 7ths; 9ths; 9/11/13 where the key or quality allows */
/* Q_DIATONIC: the key's own chord on the degree; the rest force a quality and its colors: Q_MIN7 9 and 11, Q_DOM7 9
   and 13, Q_MAJ7 9, Q_MIN7B9 a b9 (left out: it rubs the root) and 11 (Dorian's ii, Mixolydian's vi), Q_MAJ7S11 9 and
   #11 (Lydian's I, Dorian's III) */
enum { Q_DIATONIC, Q_MIN7, Q_DOM7, Q_MAJ7, Q_MIN7B9, Q_MAJ7S11 };

/* degree 0-6 in the major or natural minor scale; alter shifts the root in semitones (b2 b3 #4 b6 b7 in major, b2 3
   #4 6 7 in minor: the simplest spelling) */
typedef struct { int8_t degree; int8_t alter; uint8_t quality; } th_chord_spec_t;

/* The scale over a chord, one rule for the lead and the bass: the song's major or natural minor scale, each degree
   that a chord tone alters (a tone off the scale, on the degree it spells: b2 b3/3 #4 b6/6 b7/7) taking the chord's
   tone, unless the degree's own tone is in the chord too. Over F7 in C minor, A replaces Ab: the modes' colors come
   from the chords. pcs NULL (n 0): the plain scale. Bit i: key + i semitones. */
uint16_t th_scale_pool(int key, int mode, const int *pcs, int n);
/* The same as 7 semitones over the key, ascending (degree order) */
void th_scale(int key, int mode, const int *pcs, int n, int *sc);
int th_in_scale(int key, int mode, const int *pcs, int n, int note);   /* note: any MIDI note or pitch class */

/* Absolute pitch classes, root first, then 3rd, 5th (kept when it matters), 7th, color tones. Returns 4 or 5. */
int th_chord_pcs(int key, int mode, th_chord_spec_t c, int color, int *pcs);

/* Four notes (ascending), bottom 48-55, top <= 72. May leave out the root or 5th. prev NULL: near E3-D4. */
void th_voice(const int *pcs, int n, const int *prev, int *out4);
/* th_voice with the top note at most `hi` (the dark intro and outro: 64); rootless 0 keeps the root (no bass).
   Returns 0 when no voicing fits under hi (out4 then holds a placeholder). */
int th_voice_range(const int *pcs, int n, const int *prev, int *out4, int hi, int rootless);
/* th_voice_range for a first chord (no prev) near another reference spread than F4 over G3 (top 65, bottom 55) */
void th_voice_spread(const int *pcs, int n, int *out4, int hi, int rootless, int top, int bottom);
/* exploring voicing families (off by default; the host simulator turns it on with VNL_DROP2=1): drop-2 voicings */
void th_set_drop2(int on);

/* Why a voicing (host song log only; NULL, the default, records nothing): the chord's tones, the rules in force and
   the three best candidates with their scores, lowest first. Roles: 0 root, 1 3rd, 2 5th, 3 7th, 4 extension. */
typedef struct {
    int o[4], drop_role, drop_pc, double_pc, moved_mask;       /* moved_mask: pitch classes of common tones moved */
    int total, drop, move, top, common, range, low2, cluster;
} th_cand_t;
typedef struct {
    int n, pcs[5], role[5], rootless, dominant, have_prev, prev[4], hi, nbest;
    th_cand_t best[3];
} th_why_t;
void th_set_why(th_why_t *w);

/* hearing an alternative (host renders only): wherever the voicer would choose `from`, play `to` (4 notes, MIDI) */
void th_add_swap(const int *from, const int *to);

void th_guide_tones(const int *pcs, int n, int *third, int *seventh);   /* pitch classes, -1 if absent */
int th_is_avoid(int key, int mode, const int *pcs, int n, int note);    /* scale note (th_scale_pool over the chord)
                                                                           a semitone above a chord tone */
/* a dominant's (major 3rd and minor 7th over pcs[0]) minor 7th as a pitch class, else -1 */
int th_dom_b7(const int *pcs, int n);
const char *th_chord_name(const int *pcs, int n, char *buf);            /* buf >= 9 bytes */

#endif

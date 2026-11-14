/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Music theory for the song engine: modes, chords with colors, voicing and voice leading, guide tones, avoid notes. */
#ifndef SONG_THEORY_H
#define SONG_THEORY_H
#include <stdint.h>

enum { MODE_IONIAN, MODE_DORIAN, MODE_PHRYGIAN, MODE_LYDIAN, MODE_MIXOLYDIAN, MODE_AEOLIAN, MODE_N };
enum { COLOR_7, COLOR_9, COLOR_RICH };                     /* 7ths; 9ths; 9/11/13 where the mode allows */
enum { Q_DIATONIC, Q_MIN7, Q_DOM7, Q_MAJ7 };               /* Q_DIATONIC: from the mode; the rest force a quality */

/* degree 0-6 in the mode; alter shifts the root in semitones (borrowed or chromatic chords). */
typedef struct { int8_t degree; int8_t alter; uint8_t quality; } th_chord_spec_t;

uint16_t th_mode_pool(int mode);                           /* bit i: root + i semitones is in the mode */
int th_in_scale(int key, int mode, int note);              /* note: any MIDI note or pitch class */

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
th_why_t *th_get_why(void);                    /* the one th_set_why installed (NULL: none) */
/* the voicer's penalty for two notes a semitone apart, or a minor 9th: so high only a chord with no other fit has one */
enum { TH_CLUSTER_COST = 10000 };

/* hearing an alternative (host renders only): wherever the voicer would choose `from`, play `to` (4 notes, MIDI) */
void th_add_swap(const int *from, const int *to);

void th_guide_tones(const int *pcs, int n, int *third, int *seventh);   /* pitch classes, -1 if absent */
int th_is_avoid(int key, int mode, const int *pcs, int n, int note);    /* scale note a semitone above a chord tone */
/* a dominant's (major 3rd and minor 7th over pcs[0]) minor 7th as a pitch class, else -1 */
int th_dom_b7(const int *pcs, int n);
/* the chord's name, its root spelled as the key (tonic pitch class, MODE_*) is: sharps in a sharp key, flats in a flat
   one (by the mode's parent major: Db Lydian is Ab major's notes, flats), C major's modes the mixed table (Db Eb F# Ab
   Bb); buf >= 9 bytes */
const char *th_chord_name(const int *pcs, int n, int key, int mode, char *buf);

#endif

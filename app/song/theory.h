/* SPDX-License-Identifier: MPL-2.0 */
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
/* th_voice with the top note at most `hi` (the dark intro and outro: 64); rootless 0 keeps the root (no bass) */
void th_voice_range(const int *pcs, int n, const int *prev, int *out4, int hi, int rootless);
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

void th_guide_tones(const int *pcs, int n, int *third, int *seventh);   /* pitch classes, -1 if absent */
int th_is_avoid(int key, int mode, const int *pcs, int n, int note);    /* scale note a semitone above a chord tone */
const char *th_chord_name(const int *pcs, int n, char *buf);            /* buf >= 9 bytes */

#endif

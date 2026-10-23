/* SPDX-License-Identifier: MPL-2.0 */
/* The sequencer: steps a rolled song through its plan one 16th at a time and emits every event of that step
   (drums, bass, chords, lead, candy). Listener requests wait in one slot per field and land on the next 4-bar
   boundary. All state lives in song_t (no statics), so two songs can run side by side. */
#ifndef SONG_SONG_H
#define SONG_SONG_H
#include <stdint.h>
#include "roll.h"
#include "plan.h"
#include "groove.h"
#include "harmony.h"
#include "melody.h"

enum { SS_PLAYING, SS_ENDED };

typedef struct {
    /* read-only for callers */
    song_roll_t roll;             /* the roll as it plays now (requests applied) */
    song_plan_t plan;             /* the plan as it plays now (Motion and Map requests merged in) */
    int bar;                      /* the bar of the next step to tick (RF_BARS once ended) */
    int state;                    /* SS_* */
    /* private */
    int step;                     /* the next step to tick, 0-15 */
    motif_t motif;
    melody_phrases_t ph;
    harmony_voicing_t voicing;    /* the chord part's voicings: the loop's, and the last sounded */
    float ss;                     /* samples of the step last ticked (before the first tick: the first step) */
    float glide_from, glide_to;   /* step samples before and after a tempo glide */
    int glide_t0;                 /* the glide's first step (song step index) */
    uint32_t pos;                 /* sample position of the next step, integer part */
    float frac;                   /* and its fraction */
    uint32_t edge;                /* sample position of the next 4-bar boundary (or the song's end) */
    uint8_t pend[RF_N];           /* a request is waiting for this field */
    int16_t pend_val[RF_N];
    int16_t pend_bar[RF_N];       /* the bar it lands on */
} song_t;

void song_start(song_t *s, const song_roll_t *r);           /* plan, motif, phrases; bar 0, step 0 */
/* One 16th: its events sorted by delay; 0 once ended. Events beyond max are dropped: pass max >= 64. */
int song_tick(song_t *s, song_ev_t *out, int max);
float song_step_samples(const song_t *s);                   /* 44100 * 60 / (tempo * 4), following a tempo glide */
void song_request(song_t *s, int field, int value);         /* lands on the next 4-bar boundary */
int song_pending(const song_t *s, int *field, int *bars_left);  /* the earliest pending change; 0 if none */

#endif

/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The sequencer: steps a rolled song through its plan one 16th at a time and emits every event of that step
   (drums, bass, chords, lead, pads). Listener requests wait in one slot per field and land on the next 4-bar
   boundary. All state lives in song_t (no statics), so two songs can run side by side. */
#ifndef SONG_SONG_H
#define SONG_SONG_H
#include <stdint.h>
#include "roll.h"
#include "plan.h"
#include "groove.h"
#include "harmony.h"
#include "melody.h"
#include "lead.h"
#include "pads.h"

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
    lead_state_t lead;            /* the lead's bar plan and its song-long choices */
    harmony_voicing_t voicing;    /* the chord part's voicings: the loop's, and the last sounded */
    int keys_win;                 /* the keys' window's top (harmony_keys_window): under the lead's band, all set */
    int8_t win_oct, win_style;    /* the workbench's lead_oct and lead_style it was worked out under */
    pads_state_t pads;            /* the pad layer: the 4-bar group's notes, and the last chord part and solo note */
    float ss;                     /* samples of the step last ticked (before the first tick: the first step) */
    float glide_from, glide_to;   /* step samples before and after a tempo glide */
    int glide_t0;                 /* the glide's first step (song step index) */
    uint32_t pos;                 /* sample position of the next step, integer part */
    float frac;                   /* and its fraction */
    uint32_t edge;                /* sample position of the next 4-bar boundary (or the song's end) */
    uint8_t pend[RF_N];           /* a request is waiting for this field */
    int16_t pend_val[RF_N];
    int16_t pend_bar[RF_N];       /* the bar it lands on */
    int16_t ride_lo, ride_hi;     /* the bars [lo, hi) whose hats go to the ride (groove_ride) */
    float pad_level;              /* the pad layer's level the step last ticked played (song_pad_level) */
} song_t;

void song_start(song_t *s, const song_roll_t *r);           /* plan, motif, phrases; bar 0, step 0 */
/* One 16th: its events sorted by delay; 0 once ended. Events beyond max are dropped: pass max >= 64. */
int song_tick(song_t *s, song_ev_t *out, int max);
float song_step_samples(const song_t *s);                   /* 44100 * 60 / (tempo * 4), following a tempo glide */
/* The pad layer's level (0-1) for the step song_tick last played, for vx_set_pad_level: 0 where the layer is silent
   (a song or block without it: song_pad_on, song_pad_in), 1 where it plays; over the last bar before a block without
   it falling linearly to 0 by its last step, over the first bar back rising to 1 (1/16 on its first step). With the
   layer's swell and release, the pad fades over a section line instead of cutting (Task 1b). */
float song_pad_level(const song_t *s);
void song_request(song_t *s, int field, int value);         /* lands on the next 4-bar boundary */
int song_pending(const song_t *s, int *field, int *bars_left);  /* the earliest pending change; 0 if none */

/* SPIKE: bass rhythm: 1 a cell per song, 2 the kick's, 3 half notes on 1 and 3, 4 beats 1 and 2,
   5 beats 1, 2 and 4; 0 off */
void song_set_bass_spike(int mode);

/* Host renders (VNL_PADS=double, an A/B against the design's chord part): 1 makes the pad layer's chord part double the
   keys' voicing (the same pitches, on their strikes) over held or strummed comping (a strum: its guitar shape's top
   notes), instead of its own voice-led notes; 0 off */
void song_set_pads_double(int on);

#endif

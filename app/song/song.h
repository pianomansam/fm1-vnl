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
#include "lead.h"
#include "pads.h"
#include "override.h"
#include "library.h"

enum { SS_PLAYING, SS_ENDED };
enum { SONG_ROW_REQS = 8 };       /* song_request_row's requests waiting at once (one a row) */

typedef struct {
    /* read-only for callers */
    song_roll_t roll;             /* the roll as it plays now (requests applied) */
    song_plan_t plan;             /* the plan as it plays now (Motion and Map requests merged in) */
    int bar;                      /* the bar of the next step to tick (RF_BARS once ended) */
    int state;                    /* SS_* */
    /* private */
    int step;                     /* the next step to tick, 0-15 */
    lead_state_t lead;            /* the lead's bar plan and its song-long choices */
    harmony_voicing_t voicing;    /* the chord part's voicings: the loop's, and the last sounded */
    int keys_win;                 /* the keys' window's top (harmony_keys_window): under the lead's band, all set */
    int8_t win_oct, win_style;    /* the workbench's lead_oct and the lead style (lead.style) it was worked out
                                     under */
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
    /* song_request_row's: the requests waiting (row, value, the bar each lands on), and what landed ones made the
       song's own: its overrides (installed with song_override every tick) and its set (a role's or the kit's
       request: LIB_SET_WB, installed with lib_set_wb, copied from set_base's) */
    uint8_t rq_n;
    struct { int16_t row, val, bar; } rq[SONG_ROW_REQS];
    uint8_t own;                  /* SONG_OWN_OV, SONG_OWN_SET */
    uint8_t set_base;
    song_ov_t ov;
    lib_set_t set;
    uint16_t changes;             /* bumps as a row request lands: the voices read the instruments again */
    int8_t kit_preset;            /* the shipped kit the drum lanes play (SONG_KIT_PRESET + it is the kit row's value:
                                     app/drums/wb_drums.h's WB_KIT_*), -1 none */
    /* the next bar's planning, done ahead (song_tick: its voicings at the bar's 13th 16th, the lead's plan at its 14th,
       the pad group's at its 15th; a song's first bar at its start) so the downbeat's tick carries only its own
       events: the states it started from (v0, l0), what it made, the bar it is for (-1 none), how far it got (0
       voiced, 1 planned, 2 the pad group too) and the requests and window it was made under (a change between: made
       again on the downbeat, as before) */
    int16_t stg_bar;
    uint8_t stg_stage;
    int8_t stg_np;
    int stg_keys[4], stg_nkeys[4], stg_pcs[5];
    uint32_t stg_key;
    harmony_voicing_t stg_v0, stg_v;
    lead_state_t stg_l0, stg_l;
    pads_state_t stg_p;
} song_t;
/* the kit row's shipped kits (the drum lanes': wb_drums.h WB_KIT_PRESET..), past the engine's KIT_*: the synth
   machines', then from SONG_KIT_SAMPLED the sampled ones (the sample bank's) */
#define SONG_KIT_PRESET 16
#define SONG_KIT_PRESETS 6
#define SONG_KIT_SAMPLED 3
enum { SONG_OWN_OV = 1, SONG_OWN_SET = 2 };

void song_start(song_t *s, const song_roll_t *r);           /* the plan; bar 0, step 0 */
/* song_start with the kit row at kit from the start (-1: as rolled): the song's set its own, as a kit request makes
   it, before the plan, as the workbench stages a kit before its song starts */
void song_start_kit(song_t *s, const song_roll_t *r, int kit);
/* One 16th: its events sorted by delay; 0 once ended. Events beyond max are dropped: pass max >= 64. */
int song_tick(song_t *s, song_ev_t *out, int max);
float song_step_samples(const song_t *s);                   /* 44100 * 60 / (tempo * 4), following a tempo glide */
/* The pad layer's level (0-1) for the step song_tick last played, for vx_set_pad_level: 0 where the layer is silent
   (a song or block without it: song_pad_on, song_pad_in), 1 where it plays; over the last bar before a block without
   it falling linearly to 0 by its last step, over the first bar back rising to 1 (1/16 on its first step). With the
   layer's swell and release, the pad fades over a section line instead of cutting (Task 1b). */
float song_pad_level(const song_t *s);
/* The lead bus's low-pass amount the lead loop's filter moves ask for at bar's 16th st (leads/loop.c; 0: none, or
   the loop not playing). The step calls it once, after song_tick, for the bar and 16th that tick played: a live
   change re-records inside the tick, so its entry is closed from its landing bar's first 16th */
float song_lead_lp_at(const song_t *s, int bar, int st);
void song_request(song_t *s, int field, int value);         /* lands on the next 4-bar boundary */
int song_pending(const song_t *s, int *field, int *bars_left);  /* the earliest pending change; 0 if none */
/* The requests waiting in `later` (the same song further on: a stop's) carried into s (the song back at the start of
   that bar), each landing on the bar it would have; one waiting in s but not in `later` landed on that bar's
   downbeat and lands there again */
void song_carry_requests(song_t *s, const song_t *later);

/* A request on any row the screens set, landing on the next 4-bar boundary exactly as song_request's and carried
   across a stop like them. row: SR_ROLL + RF_* a roll field (song_request), SR_ROLE + V_* a role's instrument (V_CHORD,
   V_BASS, V_LEAD, V_PAD; library.c's stand-ins for a removed one), SR_OV + OV_* an override or a re-roll's salt
   (override.h's keys and values), SR_KIT a kit (KIT_*, or a shipped kit: SONG_KIT_PRESET..; the set's kit under it
   Brushes, or for a sampled one the mixed Rusty, as wb.c's wb_kit_preset). A role or the kit makes the song's set its own (LIB_SET_WB,
   copied from the song's), as the workbench's custom set; overrides become the song's own (song_override, from the
   ones in force). Both are undone when the same song_t starts a new song. 1: it waits; 0: refused (an unknown row, a
   kit the device has not, the song's end first, nothing playing). A second request on a row replaces the first. */
enum { SR_ROLL = 0, SR_ROLE = 32, SR_OV = 64, SR_KIT = 96 };
int song_request_row(song_t *s, int row, int value);

/* SPIKE: bass rhythm: 1 a cell per song, 2 the kick's, 3 half notes on 1 and 3, 4 beats 1 and 2,
   5 beats 1, 2 and 4; 0 off */
void song_set_bass_spike(int mode);

/* Host renders (VNL_PADS=double, an A/B against the design's chord part): 1 makes the pad layer's chord part double the
   keys' voicing (the same pitches, on their strikes) over held or strummed comping (a strum: its guitar shape's top
   notes), instead of its own voice-led notes; 0 off */
void song_set_pads_double(int on);

#endif

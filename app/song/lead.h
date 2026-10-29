/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The lead (lead design, 2026-10-22): it plays like its instrument. A percussive lead (kalimba, the felt piano, the
   sampled keys and guitars, the synth pluck) plays the hybrid: one shaped arpeggio per song over the keys' voicing an
   octave up, landing on a voice-leading skeleton note at each chord change (on 1, late, pushed an 8th early, or after a beat's rest). A sustained lead
   (flute, saw lead) and the vibes sing phrases (sustained-lead design, phrase.h): the song's motif in an 8-bar
   question and answer, each planned degree re-anchored to the bar's chord, a breath after the question and the
   arrival. It plays in the plan's lead blocks (L_LEAD, with keys, not the intro or outro). Notes keep the melody's
   rules: in the chord-scale, no avoid notes, chord tones on steps 0 and 8; a note pushed across a chord change is
   legal over both.
   Planned a bar at a time; all state in lead_state_t. */
#ifndef SONG_LEAD_H
#define SONG_LEAD_H
#include <stdint.h>
#include "roll.h"
#include "plan.h"
#include "groove.h"

typedef struct {
    int on[16], len[16];          /* the bar's onsets (MIDI, 0 none) and lengths in 16ths */
    uint8_t vel[16];              /* the onsets' velocities (0: 84 accented, else 74) */
    unsigned acc;                 /* accented onsets */
    unsigned ring;                /* onsets that ring across the bar line (pushes) */
    int last;                     /* the last skeleton note, or the sustained lead's last note sung (0: none yet) */
    int anchor;                   /* the sustained lead's phrase anchor (a scale index over akey: degree 0) */
    int akey;                     /* the key the anchor counts from, plus 1 (0: no anchor yet) */
    int leap;                     /* the last interval sung leapt (+1 up, -1 down): the next note steps back */
    int ctrun;                    /* quick chord tones by skips ending at the last note sung (no arpeggio) */
    int tied;                     /* the next bar's landing was pushed into this one */
    int active;                   /* the lead sounds in the planned bar (an onset, or a note pushed in) */
    int onsets;                   /* lead onsets that start in the planned bar */
    int chosen;                   /* the song's arpeggio pattern is chosen */
    int pat, rhy, rev, swap;
} lead_state_t;

/* 1 where the lead plays: blocks with L_LEAD and L_KEYS, not intro or outro */
int lead_plays(const plan_block_t *b);
/* Plans one bar (call at step 0). keys, nkeys: this bar's and the next bar's sounding keys voicings (4 notes, 0
   none); pcs: this bar's chord, root first. A lead style (leads_plan_bar) plans the bar where the song has one, from
   its band up (leads_range_bar: the keys voiced under it, else a 4th over their top); else the older engines below:
   over percussive keys every note a 4th or more above the keys' top (of both chords for a note that sounds on into
   the next), at most E6, or drops. With a pad the lead keeps to C4-B5, C4-C5 when the pad sits above. can_push 0: a request lands on the next bar, so nothing is pushed into it.
   b must be the plan's own block (the sustained lead reads its neighbors for the phrase's position). Returns the
   number of lead onsets that start in the bar (0 where the lead doesn't play). */
int lead_plan_bar(lead_state_t *ls, const song_roll_t *r, const plan_block_t *b, int bar, const int keys[4],
                  const int nkeys[4], const int *pcs, int np, int can_push);
/* How the hybrid lands on bar's skeleton note: 0 on 1 for an 8th, 1 on 1 for a quarter, 2 an 8th late, 3 a 16th
   late, 4 pushed an 8th early from the bar before, 5 a beat's rest then on 2 (a pure function of seed and bar). */
int lead_landing(uint32_t seed, int bar);
/* 1 if the song's lead plays the hybrid (the arpeggio and landings), 0 if it sings phrases: the lead_style override
   (0 or 1), else by its instrument (a sustained lead or the vibes sing). */
int lead_hybrid(const song_roll_t *r);
/* The hybrid's arpeggio for the song: a LEAD_PAT and a LEAD_RHY index (patterns_gen.h), drawn over the approved
   entries (the arp_pat and arp_rhy overrides force one, a candidate too). A pure function of the roll and the
   overrides. */
void lead_arp_pick(const song_roll_t *r, int *pat, int *rhy);
/* The lead's events of one step of the planned bar. */
int lead_step(const lead_state_t *ls, int step, float step_samples, song_ev_t *out, int max);

#endif

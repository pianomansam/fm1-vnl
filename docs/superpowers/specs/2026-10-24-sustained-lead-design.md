# The sustained lead: design

Status: draft for Sam's review. Replaces the skeleton line (`plan_line` in `app/song/lead.c`) for sustained leads.

## Why

Sam (2026-10-24): the wind and sustained synth leads "are down right terrible right now and need the most
improvement" — "get rid of the ugly" first, then optionally improve the decent (the struck/plucked hybrid). Today the
sustained lead holds a voice-leading skeleton note per chord with rhythm cells, an arc and breaths: it never forms a
phrase, never repeats an idea, and never arrives anywhere. The research (docs/research/arpeggiators.md §3–4,
docs/research/fm1-accompaniment-code.md, Tuna's `melody_tick` and LOF-1's Daydream) says the step from "notes" to
"a part" is **phrase form plus motif memory**.

## Goals

1. **Phrases that go somewhere:** question and answer over 8 bars — a 4-bar question that ends open (on the 3rd or
   5th of its chord, held), a 4-bar answer that comes home (a closing bar that moves by step, an arrival on a stable
   tone, long), and a breath after each.
2. **A motif the ear can follow:** one 1–2-bar idea per song (rhythm and contour), stated at the start, varied with
   small nudges, and **restated literally** at the start of the answer.
3. **Phrased like a wind player:** few, long notes (about 2–5 onsets a bar), leaps answered by a step back. Beat 1 is
   the norm, varied (Sam, 2026-10-24: the first build's "rest on beat 1" rules were "too literal and strict"): phrases
   start on beat 1 about 60% of the time, and beat 1 is struck in about two bars of three.
4. Applies to the sustained leads: flute, sax, muted trumpet, saw lead (with its glide).

## Non-goals

- The struck/plucked hybrid (it gets the rank-thinning fix later, optionally the figure library).
- An answering second voice or counter-melody (later, with the accompaniment work).
- New roll fields or song-engine changes outside the lead.

## Anti-goals

- A wandering line with no repetition (today's problem), or a loop so rigid it sounds like a sequencer.
- Breaking the music rules: chord tones on strong beats, no semitone rubs with keys or bass, no avoid notes, the lead's
  register floor over percussive keys, pushes legal over both chords, the lead 4–6 dB over the keys.

## Design

### The motif (per song, seeded)

- **Rhythm:** 3–6 onsets over 1–2 bars from a long-note vocabulary (quarter, dotted quarter, half, a note tied over the
  bar), on 8th positions; the first onset on the "and" of 1 or on beat 2 most of the time (≈ 65%).
- **Contour:** scale-degree steps between notes (mostly steps, at most one leap of a 4th–6th, followed by a step back
  the other way), stored relative to an anchor chord tone so it re-targets to any chord.
- **Placement:** each note re-anchors to the current chord: strong-beat notes snap to the nearest chord tone (the
  existing `legal()` rules), weak-beat notes may be scale steps (passing/neighbor) when legal.

### The 8-bar phrase (question + answer)

| Bar | Question (1–4) | Answer (5–8) |
| --- | --- | --- |
| 1–2 | the motif | **the motif, literally** (re-anchored) |
| 3 | the motif varied (one nudge) | the motif varied (a different nudge) |
| 4 | a **question**: ends on the chord's 3rd or 5th, held, then a breath | a **closing bar** moving by step to an **arrival** on a stable tone (root or 3rd of the home chord where the progression comes home, else the chord's root), long, then a breath |

- **Nudges** (seeded, one per varied bar, from Tuna/LOF-1): move one note a scale step (±1), alter the last interval,
  invert the contour of the second half, or rhythm-displace one note by an 8th.
- **A step-neighbor before each arrival:** the note before the question's end and the arrival approaches it by step.
- **Breaths:** at least half a bar of rest after bars 4 and 8.
- Lead blocks shorter than 8 bars use the question only (or the answer for the block's last 4 bars when it ends the
  song's lead); longer blocks repeat the 8-bar phrase with the motif re-varied each time.

### Register and contour

The phrase arcs: it rises through the question toward its highest note around bar 3–4, and the answer settles lower to
the arrival. Within the lead's legal range (the floor over percussive keys; the pad placements' ranges), centered so the
motif's span fits.

### Motion

The motion lever scales density by rank, never by random dropping: a low-motion song plays the motif's anchor notes
and the arrivals only (fewer, longer notes); a high-motion song adds the passing and neighbor notes. The same notes
drop every time.

### Expression

A velocity arc that swells to the phrase's high point and softens into the arrival; the existing delayed vibrato and
breath noise on the winds; the saw lead glides by the existing rule (under 25 ms, within a 4th).

## Tests (measurable)

- **Repetition:** the answer's bars 1–2 repeat the question's bars 1–2 (rhythm identical; contour identical in scale
  steps) in every lead block of ≥ 8 bars.
- **Question and arrival:** bar 4 ends on the chord's 3rd or 5th; bar 8 ends on a stable tone; both held ≥ a half note.
- **Breath:** ≥ half a bar of rest after bars 4 and 8.
- **Wind phrasing:** onsets per bar ≤ 5 on average ≤ 3.5; phrases start off beat 1 in 25–55%; beat 1 struck in 55–85%
  of lead bars; ≥ 70% of leaps (≥ a 4th) are followed by a step back the other way.
- **Motion by rank:** a lower motion's onsets are a subset of a higher motion's, phrase by phrase, and its notes are
  held longer to cover the dropped ones (sounding time within 10%).
- **Rules unchanged:** the clash test (rubs, avoid notes, chord tones on 0 and 8, registers), the lead-over-keys level
  test, pushes legal over both chords, determinism.

## Calls made for the builder (my calls; from the builder check)

1. **Phrase position** counts from the lead block's start (`bar − b->start`). "The block that ends the song's lead"
   is the last lead block in the plan. A request landing mid-phrase (a key change, at a 4-bar line) does not restart
   the phrase: it resumes, and the notes re-anchor to the new chords.
2. **Home and the arrival:** "home" is the progression's first chord (loop position 0). The arrival is over bar 8's
   own chord: its root or 3rd when that chord is home, else its root. No push into bar 9 at an arrival (the breath
   follows it).
3. **Ties and pushes:** a motif note may tie over a bar line (that is the sustained lead's push); the floor over
   percussive keys and legality are checked against both chords, as pushes are today. The breath sits inside bars 4
   and 8 (the question or arrival held about two beats, then about two beats of rest), so bar 5's motif may start on
   beat 1 or later, as the motif says.
4. **Rules beat repetition:** literal restatement is defined on the planned rhythm and scale-degree contour; snapping
   to a legal tone may move a note, and a note with no legal tone drops (counted, expected rare). Tests: rhythm
   identical in every restatement; contour identical in ≥ 90% of restatements after snapping.
5. **Motion:** the motif generator tags each note anchor (first, strong-beat, question, arrival) or ornament (passing,
   neighbor). Motion under 40 drops the ornaments; 40 and over keeps them. The rule applies the same way to the
   restatement. The subset test is for the same seed at two motion values. **Fewer notes, held longer (Sam): when an
   ornament drops, the anchor before it holds through the ornament's time,** so a low-motion phrase covers the same
   span with fewer, longer notes and no new gaps. Test: per phrase, the sounding time at low motion is within 10% of
   the high-motion phrase's, while its onsets are fewer.
6. **Comping:** the busy threshold stays at 4 onsets a bar: long sustained notes are not busy, so the comping keeps
   playing under them; the keys' 3 dB duck still makes room in level.
7. **State:** the motif and each pass's variations are a pure function of (song seed, lead block, pass number),
   regenerated when needed; nothing new is stored beyond what `lead_state_t` already holds.

## Left to the builder

The motif generator's exact weights, the nudge probabilities, the arc's shape, and how a lead block that isn't a
multiple of 4 bars is trimmed.

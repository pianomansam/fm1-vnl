# Background pads and strings: design

Status: draft for Sam's review. Builds on the sound engine (Plan 3a, the synths) and the song engine (lead, comping).

## Why

Sam (2026-10-24): "One thing we don't have is background pads/strings. That would make a nice layer." Comping is now
mostly held chords (held 80%, arpeggio 20%; stabs and offbeats retired), so songs have more air between strikes:
"Since we've moved away from more active comping, the pads/strings layer can fill in the silence."

## Goals

A new, quiet background layer with two parts:

1. **The mid-range chord (Sam):** a sustained chord in the mid range, **throughout** the song.
2. **The high solo note (Sam):** "a solo note an octave above everything". It **starts on beat 1 and stays the
   entire bar**, has **good voice leading**, and is **often the tonic or near it**. It comes and goes **with
   variety**: "sometimes cutting out during a break, sometimes staying."

## Non-goals

- The A-B-A two-lead arrangement (Sam's next item, its own design).
- The piano broken-chord climb (parked in the ledger).
- New effects; samples (synthesis only).

## Anti-goals

- A layer that competes with the lead or thickens the keys into mud: it stays under both in level, out of the
  lead's way, and never doubles what the keys already sustain.
- Breaking the music rules: no semitone rubs with the keys, bass or lead; no avoid notes; the registers.
- Over budget: the CPU ratio stays ≤ 1.0 on the standard heaviest list (tools/cost-bench.sh); memory within budget.

## Constraints

- The engine's rules: C99, float only, no libm, no malloc, ISR-safe, deterministic per seed, denormal-safe.
- The song engine is a pure function of roll and plan; new per-song choices come from new seed streams.
- Cost is tracked per component in docs/cost.md (Sam's standing rule). Today's maximum is 0.902 on the standard list
  (`--forced 20 7 155 138 100 22`), whose heaviest rolls are percussive keys (a Rhodes synth set), so they get the
  whole layer: it has about 0.09 of headroom. A pad voice costs about 0.06 today (M_PAD, 4 voices 0.237), so four more
  M_PAD voices would not fit.

## Design

### The mid-range chord

- **When:** every bar where the keys play, every section (intro and outro included, under their section filters),
  **only over percussive keys** (Rhodes, Wurli, felt piano, DX EP, nylon). Where the keys already sustain (the pad
  sets, the vox pad, the organ) the keys are the pad, so the chord part sits out (my call: no doubled sustain).
- **Notes:** 3 notes, the chord's guide tones first (3rd and 7th), then the 5th or 9th; mid range (about G3 to C5);
  voice-led from chord to chord by the keys' own rules (common tones held, the rest moving to the nearest legal tone,
  the top voice smoothest). No note a semitone from a keys, bass or lead note; chords with no legal 3-note set drop to
  2 notes.
- **Rhythm:** it changes only at chord changes; each chord swells in (attack about 0.6 s) and releases into the next.

### The high solo note

- **Rhythm (Sam):** one note a bar, struck on beat 1, held the whole bar. A note shared by the next bar's chord ties
  over (one long note, no restrike).
- **Pitch (Sam: "often tonic or near it", "good voice leading"):** the scale's 1st, 2nd, 3rd or 7th degree, about an
  octave above the keys' top (about C5 to C6). Each bar keeps the last note if it is legal over the new chord;
  otherwise it moves the smallest step that is legal, preferring the tonic, then the 3rd, the 2nd, the 7th. Over a V7
  the tonic rubs the chord's 3rd, so it steps down to the 7th degree, a typical move.
- **Variety by section (Sam):** each section rolls in or out per song (seeded): about 60% in for a groove, 50% for an
  intro, a break or the outro, and 30% under the lead. In a lead section it is also legal against the lead's notes
  (no rub) and drops out of any bar where the lead climbs within a 3rd of it. A section that rolls "in" holds it for
  the whole section, so it enters and leaves at section lines.

### Sound

- **Strings** (a soft string ensemble: saws, slow attack, ensemble chorus) under the acoustic and electric-acoustic
  sets; a **warm synth pad** under the synth sets (DX keys and FM bass, DX and pluck, Organ and saw lead, Rhodes and
  saw lead). My call; the set table names each set's pad instrument.
- **A frugal voice (my call, for the budget):** an ensemble model in the spirit of a string machine: up to 4 notes of
  one patch summed at the oscillator (a polyBLEP saw, or a saw plus a detuned partner for the pad), through **one**
  shared low-pass and one amp envelope per chord, at half rate. The solo note is a fifth note of the same voice or its
  own one-oscillator voice. Target: the whole layer ≤ 0.08 on the heaviest roll.
- **Bus:** its own role bus (a new pitched role), with the section filter moves the keys get, a send to the reverb,
  the chorus's ensemble depth, and the mud high-pass.

### Mix

- The chord part about 6 dB under the keys while both play; the solo note about 6 dB under the lead's level. The
  lead-over-keys rule (4–6 dB) is measured as today, with the pad layer present.
- Exact levels by ear, from renders.

## Tests (measurable)

- **Chord part:** present in every bar with keys over percussive keys, absent over sustained keys; 3 notes (2 where
  rules force it); changes only at chord changes; top-voice motion ≤ 2 semitones in ≥ 80% of changes; no rubs with
  keys, bass or lead; register G3–C5.
- **Solo note:** at most one onset a bar, on step 0, sounding to the bar's end (or tied); scale degrees 1, 2, 3, 7 in
  100% of notes and the tonic in ≥ 35%; a common tone held across a chord change whenever it is legal; otherwise moves
  ≤ 2 semitones in ≥ 90%; never within a 3rd under a lead note in the same bar; no rubs.
- **Variety:** over 500 songs, each section kind is "in" within ±10% of its share; a section is all in or all out.
- **Level:** the layer's RMS under the keys by 4–8 dB (chord part) where both play; the lead-over-keys test passes.
- **Budget:** cost ≤ 1.0 on the standard list, recorded in docs/cost.md; memory within budget; determinism and the
  carryover test hold.

## Calls made for the builder (my calls; from the builder check)

1. **Budget:** ≤ 1.0 is absolute, measured on the standard list plus a re-pick of the heaviest roll once the layer
   lands (tools/cost-bench.sh's picker), keeping ≥ 0.03 margin for bench noise (so ≤ 0.97). If the layer does not fit,
   cut in this order until it does: (a) the solo note becomes a fifth note of the ensemble voice (no voice of its own);
   (b) the ensemble renders at quarter rate where its low-pass allows; (c) the chord part drops to 2 notes (3rd and
   7th). Never drop the layer from a set silently; if (a)-(c) are not enough, stop and report.
2. **Order:** the layer is planned per bar after the lead (song.c: the lead's plan for the bar, `lead_state_t.on/len`,
   is known at the bar's start), a pure function of roll, plan, voicing and the lead's bar plan. Both parts avoid rubs
   with the lead's notes of that bar.
3. **Chord part under the rules:** 3 notes in ≥ 80% of chords (test); 2 where the rules force it, with the count
   reported.
4. **Solo note against the lead:** decided at the bar's start from the lead's plan: the bar sits out if any lead note
   in it lands within a 3rd of, or a semitone from, the solo note's choice (after trying the other legal choices). So
   a struck solo note always sounds the whole bar. The variety test measures the rolled in/out per section; the bars
   the lead removes are counted and reported separately (expected mostly in lead sections).
5. **Ranges:** the chord part is clamped to G3–C5; the solo note to C5–C6, at least a 3rd above the chord part's top
   note in the same bar (taking the next legal choice up, else sitting out the bar).
6. **Tails:** the layer's notes release like the keys' (the song's end releases everything, vx_release_all); the
   song-after-song carryover test (a song renders the same alone or after another) includes a pad set.
7. **Level tests:** measured on rendered audio by soloing the new bus, as test_lead_over_keys does today; the
   existing mix tests (levels steady, lead over keys) run with the layer present.

## Left to the builder

The ensemble voice's exact design and patch values, the share numbers' fine tuning, the set-to-pad-instrument table,
and the role's voice count, within the constraints above.

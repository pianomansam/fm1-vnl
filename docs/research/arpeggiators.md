# Sophisticated arpeggiators: how they work, what VNL-1 can borrow

> Source material, kept as researched. Context: Sam finds the hybrid lead (`app/song/lead.c`, spec
> `docs/superpowers/specs/2026-10-22-lead-accompaniment-design.md`) "still doesn't feel musical" and wants it replaced
> by a sophisticated arpeggiator. This file covers how good arpeggiators work, which code VNL-1 may reuse under
> MPL-2.0, how lo-fi uses arpeggios, and a proposal sketch.

## Summary

- **The musical arpeggiators are not note-order engines; they are phrase engines.** Yamaha's arpeggios are recorded
  MIDI phrases whose notes are remapped onto the held chord; KARMA's "Generated Effects" are phrase generators with
  separate rhythm, index, cluster, velocity and duration patterns. Up/down/random is the cheap end of the field.
- **Every serious design splits the figure into independent lanes**: rhythm (which steps sound), index (which chord
  tone), octave, velocity, gate, and sometimes transpose. Lanes of different lengths (Cthulhu, Metropolix, Elektron
  per-track lengths) give variation without new material.
- **Variation is controlled, not random.** The recurring tools are per-step conditions that fire on chosen passes
  (Elektron `A:B`, `1ST`, `FILL`), randomness that loops (Marbles "deja vu", Super Arp's modifier loop, Torso's
  looped random), and density thresholds where a sparse pattern is always a subset of the busy one (Grids).
- **Diagnosis of VNL-1's hybrid** (from reading `lead.c`): it arpeggiates the keys' close, rootless voicing folded
  into a narrow band above the keys, so contour scrambles; motion thins it with a per-note hash, so every bar loses
  different notes and no figure is ever heard twice; the 3–6-note pattern runs against 6–8 rhythm slots with
  `(bar*8+i) % plen`, so the figure's phase drifts every bar; velocity is two values; gates are fixed by the rhythm
  table. The result has no motif that repeats, which is the most likely reason it doesn't sound composed.
- **Best reusable code (MIT):** Yarns' arpeggiator and its 23 rhythm masks, Loom's sequencer-driven arpeggiator
  (Yarns fork), Marbles' deja-vu sequence and weighted quantizer, O_C's Carpeggio. All small, fixed-size, no
  allocation in the hot path. Grids, Tuna's Marbles port, frodhes, Teletype and most arp plugins are GPL: ideas only.
- **Top ideas, ranked:** (1) a library of authored one-bar figures indexed by chord tone over a lead-specific open
  voicing, (2) phrase-level repetition with looped variation (A A A' B), (3) density by rank so motion picks nested
  subsets, (4) a contour lane that moves the figure's register across the phrase toward the next skeleton note,
  (5) velocity and gate lanes, slightly out of phase with the rhythm.

## 1. How sophisticated arpeggiators work

### 1.1 Phrase libraries (pattern as phrase)

| System | Mechanism | What makes it musical |
| --- | --- | --- |
| **Yamaha Motif/Montage arpeggios** | Thousands of factory types, made by recording a MIDI phrase and converting it. Up to 16 unique note numbers per phrase. Convert types: *Normal* (plays only the held notes and their octaves, mapped by the phrase's note order), *Fixed* (plays the recorded notes regardless of input), *Org Notes* (the recorded line, transposed to follow the held chord) ([Yamaha, Arpeggio Making 101](https://yamahasynth.com/learn/montage-series-synthesizers/mastering-montage-arpeggio-making-101-part-i/)). | A human-played phrase (rhythm, accents, gate, octave jumps, rests) survives; only the pitches are re-targeted. This is the single biggest difference from a step arp. |
| **Korg KARMA** (Stephen Kay) | Over 1,000 "Generated Effects" (GEs), each with ~400 parameters; a GE is built from separate Rhythm, Index, Cluster, Duration, Velocity, Drum and CC patterns and envelopes, randomized in real time ([SOS review](https://www.soundonsound.com/reviews/korg-karma), [KARMA MW notes](https://sonicstate.com/news/2002/10/17/karma-engine-avialable-in-software/)). | Described as "a cross between a supercharged arpeggiator and a dynamically morphing auto-accompaniment". Phrases vary every pass but keep a recognizable identity because each lane is a pattern, not a coin. |
| **Super Arp** (Ableton Move, Schwung) | 40 "progression" presets in a text notation (`1-5-4-2-3`, `(2-3)` for a dyad) indexing held notes sorted by pitch, 40 rhythm presets (`0` play, `x` rest), a "missing note" policy (fold, wrap, clamp, skip) for indices beyond the chord ([README](https://github.com/handcraftedcc/schwung-superarp)). | Index notation is chord-size independent; the missing-note policy solves "the pattern wants note 5, the chord has 4". |
| **Cthulhu (Xfer)** | Chord memory feeding a grid arp; each parameter tab (note, octave, velocity, length…) can have its own step count ([review summary](https://www.kvraudio.com/forum/viewtopic.php?p=6701837)); patterns include "fingered bottom" (every second note is the bass). | Different lane lengths make a 16-step phrase evolve over many bars. |

### 1.2 Lane-based step arpeggiators

| System | Lanes and tricks |
| --- | --- |
| **Ableton Arpeggiator** | Styles incl. Converge, Diverge, Play Order, Chord Trigger, *Random Other* (no repeat until all used), *Random Once* (one random order, repeated until the chord changes); Gate % (over 100% = legato); Retrigger on note or beat; Repeats (1–2 imitate a strum); Transposition by scale steps over N repeats (a rising sequence); velocity decay toward a target ([Live manual](https://www.ableton.com/en/manual/live-midi-effect-reference/)). |
| **Bitwig Arpeggiator** | Per-step on/off, pitch offset, velocity and gate; every parameter modulatable; pairs with the Humanize note effect (velocity, timing, chance) (user reports: [Ableton forum](https://forum.ableton.com/viewtopic.php?p=1782197), [KVR](https://www.kvraudio.com/forum/viewtopic.php?p=8157130)). |
| **Intellijel Metropolix** | Per-stage lanes for gate type, pulse count (a stage lasts 1–8 clocks), ratchets, probability, an *accumulator* (transposes each pass), and eight mod lanes each with its own length and order; two tracks read one sequence differently ([Intellijel](https://intellijel.com/?p=12774)). |
| **Elektron** (Digitakt, Digitone, Syntakt) | Per-track length and scale (polymeter), parameter locks per step, micro-timing, retrigs. **Trig conditions**: `X%`, `A:B` (play on pass A of every B), `1ST`/`not 1ST`, `FILL`, `PRE` (if this track's last condition fired), `NEI` (if the neighbor track's fired) ([Syntakt manual OS 1.42](https://elektron.se/wp-content/uploads/2026/10/Syntakt-User-Manual_ENG_OS1.42_260930.pdf), [Elektronauts on PRE/NEI](https://www.elektronauts.com/t/nei-and-pre-trig-conditions/51959)). Conditions are how Elektron users make a 1-bar loop sound like a 4- or 8-bar phrase. |
| **Arturia MicroFreak** | Arp modes Up, Order, Random, Pattern; *Dice* rolls a temporary variation of gates, triggers, velocity, decay and release, *Spice* sets how far it goes; pitch is untouched ([Arturia FAQ](https://support.arturia.com/hc/en-us/articles/4405748052626-MicroFreak-General-Questions), [product page](https://www.arturia.com/products/hardware-synths/microfreak/details)). Lesson: randomize articulation, not notes. |
| **Yarns (Mutable)** | Up, Down, Up&Down, Random, Played, Chord; range in octaves; 16-step rhythm mask from a table, or a Euclidean generator (length, fill, rotate) ([manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/yarns/manual/)). |
| **Yarns Loom** (Chris Rogers' firmware) | *Sequencer-driven arpeggio*: the step sequence supplies rhythm, rests, ties and velocity, the held chord supplies pitches. `JUMP` direction: white keys move the arp position by N, black keys play an absolute chord position then return. `GRID`: walks a 2D grid laid over the chord. Output velocity = sequence velocity × key velocity ([MANUAL.md](https://github.com/rcrogers/yarns-loom/blob/main/yarns/MANUAL.md)). |
| **Torso T-1** | Euclidean pulses/steps/rotate, *repeats* (ratchet-like arpeggiated repeats), *voicing* (moves notes across octaves) and *style* (how voicing changes over time), *harmony* (shifts one chord tone at a time), *range* + *phrase* (melodic motion shape, "from cadence-like patterns to more continuous motion"), groove (probability, accent, swing), looped randomness on every parameter ([T-1 docs](https://docs.torsoelectronics.com/t1/core-concepts/pitch-chords/generating-melodies/), [Juno review](https://www.juno.co.uk/junodaily/2024/09/06/torso-electronics-t-1-review)). Notable: it advises a pool of 2–4 notes for a focused melody. |
| **Make Noise René / O_C Carpeggio** | Cartesian sequencing: notes on a grid, X/Y clocks walk it, so the same note set yields many related orders ([ModularGrid](https://www.modulargrid.net/e/make-noise-rene-mk2), [Carpeggio docs](https://github.com/djphazer/O_C-Phazerville/blob/main/docs/_applets/Carpeggio.md)). |
| **Five12 Numerology** | Chord sequencer by scale degree (type, inversion, spacing, octave) feeding strum, arpeggio or "broken" chord modules ([rekkerd](https://rekkerd.org/five12-updates-numerology-to-v21/)). |
| Not re-checked here | Novation Circuit (per-step probability and micro-steps), Squarp Hermod/Pyramid (Euclid, chance, arp effect per track), Arturia KeyStep Pro (arp with Walk and Pattern modes), Logic Arpeggiator (pattern grid with per-step velocity/ties and "Variation"/"Inversion" modes), Polyend Play (per-step chance and randomizers). Same lane vocabulary as above. |

### 1.3 Mutable Instruments generative pieces

- **Marbles deja vu** ([manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/marbles/manual/),
  [code](https://github.com/pichenettes/eurorack/blob/master/marbles/random/random_sequence.h)). Every random
  decision either draws fresh or replays a 16-slot loop. With knob `d`, `p = (2d − 1)²`: below noon, `p` is the
  chance to write a new value into the loop (noon = locked loop); above noon, `p` is the chance to jump to a random
  slot (fully right = random permutations of the same values). Loop lengths 1–16; each decision stream loops on its
  own, so streams of length 3 and 4 make a 12-step cycle.
- **Marbles X generator**: *spread* (constant → bell → uniform → extremes), *bias*, *steps* (smooth → stepped →
  quantized, then thins the scale by **note weight** until only the root is left). The weighted quantizer
  (`discrete_distribution_quantizer.cc`) is the natural way to prefer chord tones over color tones over scale tones.
- **Grids** ([manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/grids/manual/)): a 5×5
  map of drum patterns, interpolated by X/Y; each step stores a level, and the *fill* knob is a threshold, so a
  sparse pattern is always a subset of a busier one. Code is GPL-3.0 (AVR project); the idea is trivial to restate.

### 1.4 What these share (the mechanisms to explain)

| Mechanism | What it is | Why it sounds musical |
| --- | --- | --- |
| Pattern-as-phrase | A pre-composed figure (rhythm + chord-tone index + octave + accents) re-targeted to each chord. | Human phrasing survives; the figure becomes a motif the ear can follow across chords. |
| Per-step probability / conditions | A step plays with chance X, or on chosen passes (`A:B`, first, last, fill). | Turns a 1-bar loop into a 4–8-bar phrase with planned variation at the right place (the phrase end). |
| Ratchets / repeats | A step re-strikes 2–4 times inside its length. | A grace-note flourish; in lo-fi, a rare 16th double into a target. |
| Accent / velocity lane | Velocity per step, often a separate lane length. | Plucked/struck tones speak through dynamics; uniform velocity is the most "machine" tell. |
| Gate lane | Note length per step, legato over 100%. | Long/short contrast is phrasing (a held note at the top of a line). |
| Octave / transpose lane | Per-step octave, or a per-pass transpose (Ableton, Metropolix accumulator). | Contour over the phrase instead of an endless zigzag in one octave. |
| Polymetric lanes | Lanes of different lengths. | Variation from fixed material; use sparingly in a 4-bar genre. |
| Euclidean rhythms | k onsets spread evenly over n steps, rotated. | Covers tresillo (3 in 8), 3+3+2, son clave; a compact generator for syncopation. |
| Rests and ties | Explicit silence and held steps in the figure. | Space is most of lo-fi's phrasing. |
| Chord-tone vs scale-tone stepping | Index walks the chord (arpeggio) or the scale (line); Loom JUMP/GRID, T-1 harmony. | Chord tones on strong beats, scale steps as passing notes on weak ones. |
| Contour control | A shape (arch, rise, fall, cadence) applied over the phrase (T-1 *phrase*). | Phrases that go somewhere and land. |
| Deja vu / loop-locked randomness | Random choices that repeat over a short loop, with a mutate rate. | Variation that recurs, so it sounds intended. |
| Mutation over time | Slow change to the loop (Marbles below noon, MicroFreak dice, Tuna's invert 10% / alter 30%). | Keeps a long song alive without new material each bar. |
| Humanization / swing | Swing per lane, small timing offsets, velocity spread. | Systematic lateness reads as "played"; large random jitter reads as sloppy (already noted in `docs/music/ideas-from-other-apps.md`). |

## 2. Reusable code

| Code | License | What's reusable | Fit for C99, no malloc |
| --- | --- | --- | --- |
| [Yarns `part.cc` arp + `lut_arpeggiator_patterns`](https://github.com/pichenettes/eurorack/tree/master/yarns) | **MIT** (STM32 projects; the repo's AVR projects are GPL-3.0) | Up/down/up-down/random traversal over a sorted note stack with octave range; 23 16-step rhythm masks (`o-o- oo-o …`) | Trivial port: a few dozen lines and a `uint16_t` table. Keep the copyright and MIT notice. |
| [Yarns Loom `arpeggiator.cc`](https://github.com/rcrogers/yarns-loom/blob/main/yarns/arpeggiator.cc) | **MIT** (file headers; Gillet 2013, Rogers 2019; the repo has no top-level license file) | Sequencer-driven arp (sequence = rhythm/velocity/ties, chord = pitch); `JUMP` and `GRID` traversals; pure function `BuildNextResult(state, chord, step) → (state', note)` | ~190 lines C++, value-type state, no allocation. Its pure-function shape matches VNL-1's "engine is a function of roll and plan" rule well. |
| [Marbles `random_sequence.h`](https://github.com/pichenettes/eurorack/blob/master/marbles/random/random_sequence.h), `discrete_distribution_quantizer.cc` | **MIT** | Deja vu (16-float loop, mutate/jump probability), record/replay; weighted scale quantizer | Fixed arrays, float, no libm in the core. Port to C as a struct. Note it is stateful; see §4.4 for a stateless equivalent. Tuna already ports Marbles bit-exactly, but Tuna is GPL-3.0, so port from upstream. |
| [O_C Phazerville `util_arp.h`, `Carpeggio.h`](https://github.com/djphazer/O_C-Phazerville) | **MIT** for these files (Gillet; Jason Justian, chord library Roel Das); the repo as a whole includes GPL-3.0 parts | Yarns arp adapted to a small MCU; a 55-chord library and a 4×4 Cartesian walk | Small, static arrays; Arduino-flavored C++. Check each file's header. |
| [Super Arp](https://github.com/handcraftedcc/schwung-superarp) | **MIT** (2026) | Index notation with chord dyads, missing-note policy, seeded per-lane modifiers (drop, velocity, gate, octave, note) with a shared loop length | Plain C (~2,000 lines) but allocates its instance and parses JSON with `malloc`; take the ideas and small functions, not the module. One star, AI-assisted: treat as reference, not a dependency. |
| [Grids](https://github.com/pichenettes/eurorack/tree/master/grids) | GPL-3.0 (AVR) | Idea only: per-step levels with a density threshold, X/Y morphing between maps | Restate the idea; don't copy the tables. |
| Tuna (`ref/tuna`), frodhes ([FM-1 Rhodes + arp](https://github.com/leonardoMirabal/frodhes)), Teletype, qmidiarp, LibreArp, Raptor, simple-arpeggiator-lv2 | GPL / LGPL | Ideas only | — |
| Magenta (MusicVAE, Lo-Fi Player) | Apache-2.0 | Data and ideas (already mined in `lofi-hip-hop.md`) | Models need training and a runtime; not a fit for the MCU. |

## 3. Lo-fi: how arpeggios are used

Web sources on lo-fi arp programming are thin (mostly sample-pack listings:
[SampleFocus lo-fi plucked melodies](https://samplefocus.com/collections/lo-fi-plucked-melodies)); the stronger
evidence is already in this repo.

- **Slow and plucked.** Lo-fi arps sit at 70–95 BPM in 8ths, rarely 16ths, on harp, kalimba, nylon or fingerpicked
  guitar, felt piano, vibes (`lofi-hip-hop.md` §4.1). jacbz's generator layers a quiet arpeggio (piano −1 octave or
  harp, level 0.1–0.2) under the melody, which says arps are texture-plus-hook, not foreground solos.
- **Open, wide figures.** jacbz's first-beat arpeggio is scale degrees **1, 5, 8, 9, 10** in 8ths (P1:
  1, 5, 8, 5, 10, 5, 8): a spread voicing over more than an octave that ends on a color tone (9th/10th), not a
  close-position cycle. Fingerpicked guitar figures behave the same (bass, 5th, octave, then the 3rd/9th on top).
- **Repetition is the point.** Sampled jazz loops repeat a 1–2-bar figure for minutes; variation comes at phrase ends
  and section changes (`lofi-hip-hop.md` §1–2; `lofi-audio-arrangements.md`: the lead appears in some blocks only).
- **Phrasing.** Melodies start off beat 1 (beat 2 or an 8th offbeat), rest on 1 of every second bar, and answer a
  2-bar call ("call and response"); swing is light, lateness is systematic (`lofi-hip-hop.md` §2, `docs/music/lofi.md`).
- **Space.** In most bars a lo-fi track is drums + bass + keys; when an arp plays, the comping thins (already in the
  lead spec as "busier lead = sparser comping").

Implied defaults: 8ths or dotted/3+3+2 cells, 4–6 notes per bar with at least one rest, a 1-bar figure repeated
across the chord loop, range about an octave and a 3rd, highest note a color tone, soft velocities with a clear
downbeat accent and a decaying tail, long gate on the last note of the figure.

## 4. Proposal sketch for VNL-1

### 4.1 Shape

A **figure library plus generative transforms**, played over a **lead arp chord** that is not the keys' voicing.

1. **Arp chord.** Per chord, build an ordered set of 4–6 lead tones above the floor: the chord's guide and color
   tones in an open spread (e.g. 5, 9, 3, 7, 9′ or 3, 7, 9, 5′), from tones the keys' voicing contains plus the
   chord tones the bass does not rub. Any tone in the sounding voicing cannot rub the keys; the existing `legal()`
   still screens the bass and avoid notes. Index 0 is the lowest; the figure addresses indices, not pitches, so it
   survives chord changes with its contour intact. Index beyond the set: Super Arp's `fold` policy (reflect back).
   This also answers the spec's "lean to color tones, double the keys' top less".
2. **Figure.** A one-bar 16-step record with lanes: on/rest/tie, index, octave (−1/0/+1), velocity (3–4 levels),
   gate (staccato/normal/legato), and a density rank per onset (§4.3). Author 16–24 figures as data (the
   "readable bench data" idea: 16-character strings compiled to C), covering the six current rhythms plus
   lo-fi cells from §3. A song rolls one main figure and one answer figure.
3. **Transforms** (seeded, from the existing `rng` streams, no new roll fields): rotate the index by one chord tone,
   invert (high ↔ low), retrograde the last half, octave-displace one note, drop to a sub-figure. These are the
   variation vocabulary; the figure itself never changes inside a song.
4. **Phrase plan.** Over the 4-bar phrase: A A A′ B, or A B A B′, where ′ is a transform and B is the answer figure
   or a cadence (fewer notes, long last note on the skeleton tone). Variation lands on bars 3–4, as Elektron users
   place `A:B` and `FILL` trigs. Bigger changes (new transform set) at section boundaries.
5. **Landing.** The figure's final onset in the chord's last bar targets the next chord's skeleton note (or the
   push), so the voice-leading skeleton becomes the figure's goal rather than an overwrite.

### 4.2 Staying clear of keys and bass

Indexing only into the lead arp chord means every pitch is a voiced tone or a bass-safe chord tone, placed at or
above the percussive-keys floor (top + 5); transforms only re-order or move octaves inside that set. Scale-step
passing notes (the sustained line's tool) stay allowed only on weak 16ths through `legal()`. Pushes and late
landings keep checking both chords. The existing clash test runs unchanged.

### 4.3 Motion

Replace the per-note hash with **density by rank** (Grids' threshold idea): each onset in a figure carries a rank
1–8; motion sets the threshold, so motion 20 plays ranks 1–3 (downbeat, the top note, the last note) and motion 80
plays all. A thinner bar is a subset of the busy one, always the same subset, so the figure keeps its identity.
Motion can also scale how often transforms fire and how far the contour lane travels.

### 4.4 Deja vu without hidden state

The engine must stay a pure function of seed and bar. A stateless deja vu: decision `k` in bar `b` reads loop slot
`k mod L`, whose value is `hash(seed, stream, slot, epoch)`, where the epoch increments every `E` passes (mutation
rate) and is per-slot staggered by the hash. A locked song uses `E = ∞`; a drifting one uses `E` of 2–4 phrases.
Same musical effect as Marbles' loop, no state across bars.

### 4.5 The ideas most likely to make it feel musical, ranked

1. **Authored figures over an open lead chord, indexed by chord tone.** A motif that survives chord changes is what
   Yamaha, KARMA and lo-fi loops all have and the hybrid lacks. Wide spread (an octave and a 3rd) with a color tone
   on top fixes the "noodling in one octave" sound.
2. **Phrase-level repetition with planned variation (A A A′ B; looped, seeded transforms).** Repeat first, vary at
   phrase ends; replace bar-by-bar randomness with conditions on passes.
3. **Density by rank tied to motion.** Thinning that keeps the figure's skeleton (same notes drop every time) instead
   of random holes.
4. **Contour across the phrase that targets the next skeleton tone.** An octave/rotation lane that rises through the
   phrase and falls to the landing, so each 4 bars has direction and ends.
5. **Velocity and gate lanes** (downbeat accent, soft inner notes, a long last note; lane length 3 or 6 against an
   8-step rhythm for slow accent drift). Cheap, and the most audible "machine vs player" tell on struck and
   plucked instruments.

Lower priority: ratchets (one 16th double into a target at most once a phrase), Euclidean generation of new
rhythm cells (the authored table covers lo-fi better), polymetric pitch lanes (risk of aimlessness, which is the
current problem).

### 4.6 Cost

A 24-figure table at 16 steps × 3 bytes is about 1.2 KB of flash. Planning stays once per bar in the existing
`lead_plan_bar` shape; per-bar work is a few hundred operations, negligible at 360 MHz.

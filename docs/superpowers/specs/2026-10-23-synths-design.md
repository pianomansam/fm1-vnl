# Synths: design

Status: draft for Sam's review. Builds on the sound engine (Plan 3a, merged; docs/music/lofi.md, docs/cost.md).

## Why

Sam, after the Plan 3a renders: "we have lots of physical instruments (flutes, guitars, keys, etc), but very few
synths." Asked which synths he hears in this music: "all of them" — vintage analog (Juno, Moog), 80s FM/digital, and
chiptune. Today only the two pads are synths; every other patch emulates an acoustic or electric-acoustic instrument.

## Goals

1. Synth instruments for every role, covering the three families Sam named:
   - **Vintage analog:** a Juno-style poly (saw/pulse, a low-pass, the thick chorus), a Moog-style bass (saw/square
     through a steep resonant low-pass with a filter envelope), mono saw and square leads with glide, a filtered
     synth pluck, a soft square organ.
   - **80s FM/digital:** a DX-style electric piano (bell-like tine attack, glassy body), an FM bass (the punchy
     "slap" FM bass), FM synth bells.
   - **Chiptune:** pulse-wave chords (12.5/25/50% duty), a pulse lead with fast vibrato, a triangle bass, noise-tick
     candy blips; stepped pitch and a little bit-crush character.
   - Plus a soft **sine/triangle "whistle" lead** with delayed vibrato (common in lo-fi).
2. **New instrument sets** that pair them, alongside the acoustic sets, some all-synth and some mixed (an analog bass
   under a Rhodes, a synth pluck lead over nylon), so synth songs come up often enough to matter.
3. They behave like their instruments in the song engine: sustained leads (saw, square, sine) sing the line;
   plucked/chip leads arpeggiate (the existing class rule, `lib_ins_sustained`); mono leads glide between legato notes.

## Non-goals

- New effects (the chain, chorus, delay, reverb are done; the Juno chorus is the existing chorus at a Juno-like
  depth and rate).
- Samples (synthesis only, per the spec).
- New song-engine behavior beyond glide (the arpeggiator redesign is its own later piece).

## Anti-goals

- Bright, harsh synths that fight the lo-fi chain: everything warm by default (low-pass, slow-ish attacks on pads,
  dust still colors them).
- Over budget: the CPU stays ≤ 1.0 on the heaviest roll; memory within budget.
- Breaking the approved music rules (no rubs, rootless voicing, registers) or the lead-over-keys level rule.

## Constraints

- The engine's rules: C99, float only, no libm, no malloc, ISR-safe, deterministic per seed, denormal-safe.
- Cost is tracked per component in docs/cost.md (Sam's standing rule).
- Adding sets changes which set a seed rolls (sets are drawn uniformly among mood-compatible ones), so existing seeds
  may come back with a different set. Accepted as part of adding sets.

## Design

### Models (my call: two new models, the rest are patches on existing ones)

| Model | New? | Instruments |
| --- | --- | --- |
| **M_SUB** subtractive | new | Juno poly, Moog bass, saw lead, square lead, synth pluck, organ, sine/triangle lead |
| **M_CHIP** pulse/triangle/noise | new | chip chords, chip lead, chip bass, chip blips |
| M_EP (2-op FM) | existing | DX electric piano (higher ratio, bright fast-decaying index) |
| M_BASS (FM) | existing | FM bass (fast index decay, a "slap" attack) |
| M_BELL (FM) | existing | FM synth bells |

- **M_SUB:** two oscillators (saw, pulse with width, or sine/triangle; polyBLEP as the pads do), a sub-octave, detune,
  a low-pass (state-variable; a 4-pole option for the Moog bass) with its own envelope and key tracking, an amp
  envelope; a mono mode with glide (portamento in ms) for leads and basses. Half-rate where the keys and pads already
  are.
- **M_CHIP:** a pulse at fixed duty steps or a stepped triangle (4-bit, like the NES), a noise channel for blips,
  pitch quantized to semitones except a fast vibrato; no filter (dust colors it). Very cheap.
- Each new instrument is an `INS_*` with a patch; its class (sustained or percussive) follows the instrument.

### Sets (my call; curated by ear later)

About six new sets, for example: "Juno and saw lead" (chillwave), "DX keys and FM bass" (city pop),
"Chip lo-fi" (pulse chords, chip lead, triangle bass, blips), "Organ and sine lead" (warm), "Rhodes and Moog"
(a Moog bass and saw lead under a Rhodes), "Nylon and synth pluck". Each gets moods and a kit like the existing ones.

### Mix

New patches are balanced by the same rules: steady levels across songs, the lead 4–6 dB over the keys (the level
fix in progress), the mud high-pass, the bed under the music.

## Tests

- Per model: the measurable traits (glide time, filter envelope sweep, pulse duty, chip pitch steps, sub-octave
  level, no NaN/denormals, steals fade), as the earlier models were tested.
- Per new patch: the existing patch checks (peak, silence after release, determinism).
- The roll: every new set rolls under its moods; synth sets come up in a reasonable share of songs.
- Cost and memory within budget, recorded in docs/cost.md.
- Renders of every new set for Sam, with and without the bed.

## Calls made for the builder (my calls; from the builder check)

1. **CPU:** the limit is the host ratio ≤ 1.0 (tools/cost-bench.sh), as for Plan 3a, until the FM-1 bench. M_SUB
   is built frugally like the real Juno-106: **one oscillator plus a sub-octave and noise** per voice (not two
   oscillators), a 2-pole low-pass for polys, half-rate for chords and pads. The Moog bass's 4-pole filter runs at
   full rate on its one mono voice. If a patch is over budget, lower its rate or features before cutting voices.
2. **Patch parameters:** `vx_patch_t.p[]` widens from 8 to 16 floats. The patch table is `const` (flash), so RAM
   doesn't grow; the voice union grows only by M_SUB's and M_CHIP's state.
3. **Glide:** engine-only, no song-engine change. A mono patch on the lead role glides when a new note arrives
   while the last one is still held (the bass role's legato rule, extended to lead patches marked mono).
4. **Class:** `lib_ins_sustained` adds the saw, square and sine leads, the organ and the Juno poly; the chip lead,
   synth pluck and FM bells stay percussive (they arpeggiate as leads). No third class.
5. **Half-rate rule:** an M_SUB voice renders at half rate when its filter's highest cutoff (with envelope) stays
   ≤ 8 kHz; M_CHIP always full rate (no filter; its edges are the sound).
6. **Roll share:** sets stay drawn uniformly among mood-compatible ones, so synth sets come up in proportion to their
   number (about a third of songs with six new sets). Test: each new set rolls in ≥ 3% of songs, and sets with any
   synth in ≥ 25%.
7. **Moods:** the four moods (sad, wistful, warm, sunny); each new set takes two, and every mood gets at least one
   all-synth set.

## Left to the builder

Exact patch parameters, the 4-pole filter's design, glide curve, set names and pairings (within the families above),
the kit per set.

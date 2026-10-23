# Lead and accompaniment: how they play together

Status: draft for Sam's review. Builds on the VNL-1 design (`2026-10-22-vnl-1-design.md`) and the lead spikes on
main (`VNL_LEADSPIKE`, `app/song/song.c`).

## Why

The old leads were "really sparse and not musical at all" (Sam). The lead should not be a guest: it doesn't have to
play the whole song, but it should sing when it's present. Listening to spikes, Sam picked the hybrid (C) as the most
promising: a shaped arpeggio that lands on a voice-leading skeleton note at each chord change. The remaining problem
is how the lead and the accompaniment share the space. Today every lead plays the same way whatever its instrument,
and the keys and lead crowd each other in register (measured below).

## Goals

1. **The lead plays like its instrument.** Struck and plucked leads arpeggiate; wind leads sing lines.
2. **The lead leads.** When the lead is busy the comping gets sparser, and fills back in when the lead rests or
   holds (Sam: "busier lead == sparser comping"). The lead's density drives the comping's, never the reverse.
3. **Register follows the keys' attack.** Percussive keys keep the lead clear above them. Sustained, slow-attack keys
   (pads) may sit above, with, or below the lead (Sam: "if keys are sustained, then octave doesn't matter").

## Non-goals

- Timbre. The voices are placeholder FM until Plan 3; these are rules about which notes and when. Sam judges the
  sound by ear once the real instruments exist.
- Bass. Its tones are approved and its rhythm spike (`VNL_BASS`) is parked to revisit.
- New chord or voicing rules. The approved voicer, its voice leading and its no-cluster rules stay as they are.

## Anti-goals (a failure even if it works)

- A semitone rub between lead and keys, anywhere (standing rule).
- A wind lead running 8th- or 16th-note arpeggios.
- Lead and comping both busy at the same time, or both silent at the same time for long stretches inside a lead
  section.
- Comping that changes style abruptly enough to sound like a different song. Thinning is a lighter version of the
  song's own comping, not a switch to another style.

## Constraints

- Engine rules: C99, float only, no libm, no malloc; the song engine stays a pure function of the roll and the plan
  (no hidden state that two songs share). The spike's file-scope statics must move into `song_t` (or equivalent).
- Every choice is seeded per song (one pattern per song, Sam), so a seed replays the same song (`./new-song.sh SEED`).
  New choices draw from new seed streams and add no roll fields, so an old seed keeps its key, tempo, progression,
  map and set; its lead and comping change, as intended (my call).
- The standing music rules hold: no substitutions, rootless keys, one voicing per chord through the song, no
  semitone rubs anywhere, simplest key spelling.
- Tests in `tests/host/` cover every rule below with numbers over 300 songs, like the existing ones.

## Where things stand (300 songs, hybrid lead)

- Keys sound in C3–B4 (MIDI 48–71); the lead in C4–B5 (60–83).
- 6% of lead notes are at or below the keys' top note; 23% sit within a major 3rd above it; 25% are the keys' top
  pitch class (mostly the top note an octave up, because the hybrid arpeggiates the keys' own voicing).
- Comping per song: held 55%, stabs 20%, offbeats 15%, arpeggio in 16ths 10% (approved by ear).

## The design

### 1. Instrument classes

| Class | Keys | Lead |
| --- | --- | --- |
| Percussive (struck or plucked: fast attack, decays) | Rhodes, Wurli, felt piano, nylon | vibes, kalimba, guitar lick |
| Sustained (slow attack or held breath) | pad | flute, sax, muted trumpet |

The class comes from the set's instrument (`lib_set_t.chords`, `.lead`), one lookup each.

### 2. The lead by class

- **Percussive lead: the hybrid (spike C) as it stands.** One arpeggio pattern, rhythm, direction and swap per song;
  the phrase's last bar drops two notes; each chord change lands on the skeleton note in one of six ways (on 1 for an
  8th 15%, on 1 for a quarter 15%, an 8th late 20%, a 16th late 10%, pushed an 8th early 20%, a beat's rest then on 2
  20%); the rhythm favors fewer notes on the snare.
- **Sustained lead: the skeleton line with rhythm (spike A), extended.** The voice-leading skeleton note per chord,
  with the song's two rhythm cells (passing note, re-strike, late arrival, neighbor note, push), suspensions, and a
  breath at the end of each 4-bar phrase. The spike's weakness: in song 1 the skeleton only alternated C5 and Db5.
  The line needs to wander more. Allowed: a slow drift of the skeleton target over a phrase (an arc up and back) and
  scale-step passing notes between skeleton notes, both under the no-rub rule. How far it wanders is the builder's
  call, measured (see Tests).
- Both keep the lead's sections and layers as today (no lead in intro and outro; only where the keys play), and
  both leave out ear candy while the lead plays, as the spike does.

### 3. The comping makes room

- Each bar gets a lead density: the number of lead onsets that start in that bar (a note pushed across the bar line
  counts in the bar it starts in), known at the bar's step 0, since the lead plans a bar at a time.
- **Busy bar** (the builder sets the threshold; start at 4 or more onsets): stabs, offbeats and the comping arpeggio
  thin. Stabs keep the strike on 1 only; offbeats keep two of four; the arpeggio drops to every other 16th or a held
  chord. Held comping and pads are already sparse and stay as they are.
- **Quiet bar** (the lead rests or holds one note): the comping plays in full.
- Thinning never changes the voicing, only how many times it is struck.

### 4. Register

The **keys' class alone decides the register rule**, whatever the lead's class (Sam: "if keys are sustained, then
octave doesn't matter"). So pad + kalimba and pad + guitar lick use the pad placements; felt piano, nylon, Rhodes
and Wurli under a wind lead use the 4th-above floor.

- **Percussive keys:** the lead's lowest note stays at least a perfect 4th (5 semitones) above the keys' top note of
  the chord it plays over. The percussive lead doubles the keys' top pitch class less: the arpeggio's tones lean to
  the chord's color tones (9th, 7th, 13th) over the voicing's top note, without leaving the chord.
- *The floor in practice (my call):* the floor uses every chord the note sounds over (a pushed or late landing
  checks both). The lead's range may extend up to E6 (MIDI 88) to make room; when no legal note exists there, the
  note is dropped. The keys are never lowered for the lead.
- **Sustained keys (pad):** one placement per song, seeded: **below** (the keys where they are now, the lead above),
  **with** (the keys an octave up, sharing the lead's range), or **above** (the keys an octave up and the lead kept to
  the low half of its range, so the pad's top sits over the lead). The keys move as a whole voicing by octaves, so
  voice leading is untouched. Equal weights to start. *(Builder's reading of Sam's "pad an octave higher, lead in the
  normal range"; Sam to confirm by ear.)*
- No semitone rub between the lead and any sounding keys note, in every placement (the clash test already checks
  every step).

## Tests (numbers over 300 songs)

- Wind leads: no run of more than 2 lead onsets a 16th or 8th apart that outline the chord (no arpeggios).
- Percussive keys: lead notes below the keys' top + 5 semitones: 0%. The keys' top pitch class in the lead: well
  under today's 25% (builder measures and sets the bound).
- Sustained-keys songs: all three placements appear, each 33% ± 8%.
- No bar inside a lead section where neither the keys nor the lead sound (a held chord counts as sounding).
- Busy lead bars: the comping's mean strikes per bar at least a third lower than in quiet bars, for stabs, offbeats
  and arpeggio songs.
- Sustained-lead skeleton: it reaches at least 4 distinct pitches per song in most songs (builder sets the share).
- The existing clash, voice-leading and keys-vs-bass rhythm tests still pass.

## Left to the builder

- Where the class table lives (library or harmony); how the lead's per-bar density reaches the comping (song.c
  orchestrates both); the busy threshold; exact thinning per style; how the sustained line wanders; moving the spike
  out of `song.c` statics into the engine proper, and whether `VNL_LEADSPIKE` stays as an exploration switch.
- Sam judges every rule by ear with `./new-song.sh` renders before it is called done.

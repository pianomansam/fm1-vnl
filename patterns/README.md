# VNL-1 patterns

The engine's pattern tables, one pattern per file, as MIDI (`.mid`, for a DAW) and MusicXML (`.musicxml`, for
MuseScore 4). Edit either one, or add a new file beside them, then run `tools/patterns/import.sh` (`--check` to
validate only): it reads them back into the engine's tables, `app/song/patterns_gen.c` and `.h` (generated: never edit
them by hand), and runs the engine suite when they changed.

`tools/patterns/export.sh` writes the files from those tables (it builds `tools/patterns/dump.c` over
`patterns_gen.c` and runs `tools/patterns/export.py`); export then import with no edits regenerates the tables byte for
byte.

## Common conventions

- **Grid:** 4/4, tempo 85, quantized to 16th notes (a step is a 16th; a bar is 16 steps; bar 1 starts at step 0).
- **Files:** `<family>/<id>-<slug>.mid` and `.musicxml`. The id is the part before the first `-` of the slug (the
  lead-arp ids, like `rhythm-2`, keep their own dash). Index-keyed tables (strum, lead-arp, melody-cells) have id =
  the C index + 1.
- **MIDI:** format 1, PPQ 480 (a 16th = 120 ticks), as `app/host/score.c` writes songs: a conductor track (title,
  4/4, tempo, chord symbols as text events), then one track per part. Drum-lane families (drums, kick-phrases, strum)
  are on channel 10, notes 60 ticks long; pitched families on channel 1, notes as long as they sound.
- **MusicXML:** `score-partwise` 4.0, divisions 4 (one division = a 16th), C major. Drum-lane families use a
  **percussion clef with unpitched notes** (each lane its own staff position and instrument, MIDI channel 10);
  pitched families use a **treble clef with pitched notes**. Velocity rides on each note's `dynamics` attribute
  (percent of velocity 90); a ghost note's head is in parentheses.
- **Metadata:** every file carries its entry's fields as JSON (a MIDI text event starting `vnl1 `; a MusicXML
  `miscellaneous-field name="vnl1"`), and its title says what it is. A DAW or MuseScore may drop it on save; the
  file's name (`model.json`'s `file`) still identifies the entry, and `model.json` holds its other fields.
- **`model.json`:** every field of every entry (the source notes, moods, modes, weights, `status`), keyed by id, in
  table order. `status` is `"approved"` (the engine draws it) or `"candidate"` (a new pattern, the
  workbench only until Sam approves it).
- **`.manifest.json`:** the SHA-256 of each `.mid` and `.musicxml` as last written, so the importer can tell which
  file of a pattern Sam edited (either may be edited alone).
- **Names are stable, and export never deletes:** an entry's file name is fixed when it is first exported and kept
  in `model.json` (`file`); a later change to its notes never renames it. Export never deletes a file, and never
  overwrites one whose SHA-256 differs from `.manifest.json` (an edited file): it skips it and says so. Importing is
  the path for edits.

## Importing

- **Which file:** `.manifest.json` says which file of a pattern changed since it was last written. Only the `.mid`
  changed: imported from it; only the `.musicxml`: from that. Both changed: they must say the same, else the import
  is refused naming both. After an import the pattern's other file is rewritten from the imported entry (the file
  Sam edited is never rewritten) and both hashes go in the manifest.
- **New patterns:** a file whose name is not in `model.json` is a new pattern, as either file alone (the other is
  written for it). It keeps its file name, takes the next free id and is a **candidate** (the workbench only, until
  approved). A new lead-arp file's name starts with `rhythm-` or `pattern-`. The comp rhythms (held, half, stabs)
  and the six landings are fixed: edit them, never add one. A strum pattern added past the fourth skips id 5 (the
  ballad's fill). **For now** a new drum pattern, progression or melody cell is refused ("new ... patterns can be
  imported once candidates are workbench-only (next step)"): the firmware still draws every entry of those tables;
  edits to their existing entries import. The importer notes a new file that sounds the same as an existing entry
  (a renamed or saved-as copy: it would import as a new candidate) and an entry whose files are missing (it stays).
- **What is read from the notes:** onsets (on the 16th grid: any other onset is refused), lengths (rounded to 16ths),
  pitches or lanes, and ghosts (a drum note at velocity 60 or less, or a MusicXML notehead in parentheses). Durations
  count in each file's own divisions or PPQ (MuseScore rewrites them); tied notes are one note; rests, empty staves
  and trailing rests-only bars are ignored. **MIDI:** a drum-lane family reads channel 10 (else every channel), a
  pitched family channel 1 (else every channel but 10), so MuseScore's chord-symbol playback (its own channel) and
  other parts don't count. Chord symbols (`<harmony>`, MIDI text) are labels: the notes are the chord.
- **What is read from the metadata:** the fields notes can't hold (`src`, a drum pattern's `family`, written
  `pattern_family` in the metadata since its `family` is the folder's, and `half_time`, the comp rhythms' `vel_range`, `second_vel_pct`, `busy_over` and `busy_steps`, weights), when the
  file still has its `vnl1` metadata; else `model.json`'s. A progression's `kind`, `moods` and `modes` come from its
  title when it parses (`Progression N: ... (loop; warm, sunny; Dorian, Aeolian)`; MuseScore keeps the title, a
  DAW's MIDI may not). Each chord keeps the metadata's or `model.json`'s `{degree, alter, quality}` when it still
  sounds the same (a mode's own chord and a forced quality look alike), else it is read in the first mode listed.
  The comp rhythms' velocities in a file are placeholders: if they differ, the importer notes that they are ignored.
- **The rules** (a refusal names the file, the bar and the rule, and nothing is written): a drum pattern has a kick
  and a snare in every bar and plays only the six lanes; a progression's chords are maj7, 7, m7 or m7b5 over the
  lowest note (written extensions are ignored, with a note: the engine colors every chord itself, a dominant always
  with its 9th or 13th), each the same whole number of bars, 1-4 chords in a 2, 4 or 8-bar cycle; no substitutions
  (a vamp's or loop's chords lie in each of its modes; no dominant on bII or bVI or a half step above the next
  chord); no dominant the engine would leave bare (no 9th or 13th fits); no Phrygian; strum up strokes only on the "and"s, one stroke a
  step; an arpeggio pattern's notes are chord tones 1-4 of the reference Cmaj7 (a chord-tone index over 4 is
  refused); the landings' weights add up to 100; a melody cell stays in its beat; `src` is printable ASCII with no
  backslash and no `??` (it becomes a C string).
- **The hashes:** the importer lists every entry that changed or was added; when an approved firmware entry changed,
  the engine suite's pinned song hashes will move (a candidate never moves them): re-pin them in `tests/host/song_test.c` with a
  comment naming the edit. Kick phrases are the workbench's (`workbench/options.json`), so they never move them.

## drums

`LIB_PATTERNS` (8; `app/song/patterns_gen.c`, declared in `library.h`). Two bars, the GM drum map: kick 36, snare 38, closed hat 42, open hat 46,
shaker 70, rim 37 (the drum staff: kick F4, rim B4 x, snare C5, shaker E5 triangle, closed hat G5 x, open hat A5
circle-x). Velocity > 60 is a hit (written 90), ≤ 60 a ghost (written 40, head in parentheses). Bar 2 is bar B of a
two-bar pattern, else bar A again. Pattern 5's shaker is a period-3 polymeter (`poly3`: a hit every 3rd 16th, counted
from the song's first step), written out over the two bars, with `poly3` in the metadata. Bar 1's shaker is the
table's shaker mask (`hit[D_SHAKER]`, 0x9249: steps 0, 3, 6, 9, 12, 15); bar 2's shaker differs only because the
polymeter runs on, so it doesn't make the pattern two-bar (`two_bar` stays 0; the other lanes decide that).
`family` (as `pattern_family`), `half_time` and `src` are metadata.

## kick-phrases

`kickPhrases` in `workbench/options.json` (5). Two bars, kick only (note 36): bar 1 is `a`, bar 2 is `b`.

## progressions

`LIB_PROGS` (44; `app/song/patterns_gen.c`). Chord symbols in C, one chord per its `bars_per_chord` bars (a 2-bar chord
is tied across the bar line), as MusicXML `<harmony>` and MIDI text events, over the chord's tones (root, 3rd, 5th,
7th, root between F3 and E4). The symbols are spelled in the first mode the progression lists (`symbols_mode`; a
`Q_DIATONIC` chord takes that mode's 7th chord). `kind`, `moods` and `modes` are in the title and the metadata, and
the exact `{degree, alter, quality}` of each chord is in the metadata (`Dm7` from `Q_MIN7` or from the mode's own
chord look the same on paper).

## comp-rhythms

The keys' comping rhythms (`COMP_HELD`, `COMP_HALF`, `COMP_STABS` in `patterns_gen.c`; `harmony.c` plays them):
held, half and stabs, over a reference Cmaj7 (C4 E4 G4 B4; the pitches are placeholders, only the rhythm counts). The
velocities are written at the middle of `vel_range` (metadata).

- **held:** one strike on step 0, held the bar.
- **half:** strikes on steps 0 and 8, a half note each, the second at 90% velocity.
- **stabs:** a stab on step 0 and a second stab, an 8th long. The second stab's candidates (`second`: steps 6, 10,
  4, 12, tried in that order where the kick leaves the step free) are written as alternates, one bar each: bar 1 is
  the first candidate, bar 2 the second, and so on. Over a kick of more than 3 hits a bar (`busy_over`), the stabs
  are steps 0 and 6 (`busy_steps`), in the metadata.

## strum

`STRUM_PAT` (4) and `STRUM_BALLAD_FILL` (`patterns_gen.c`; `strum.c` plays them). One pitch per stroke type, as drum lanes (the bossa is 2 bars,
the rest 1): T thumb E4 (64), B thumb and pinch F4 (65), D accented down G4 (67), d down A4 (69), X chuck B4 (71, x
head), U up C5 (72), u soft up D5 (74), P pinch E5 (76). A rest is no note. Up strokes (U, u) go only on the "and"s
(steps 2, 6, 10, 14). Id 5 is the ballad's fill-bar variant (`fill_of` 4).

## lead-arp

`patterns_gen.c` (`lead.c` plays them), over a reference Cmaj7 voicing C4 E4 G4 B4 = chord tones 1-4 (the lowest note is the chord's 1st
tone, the next its 2nd, ...).

- **rhythm-1 to rhythm-6** (`LEAD_RHY`): the arpeggio's onsets and lengths in a bar; the pitch is a placeholder (C4).
- **pattern-1 to pattern-6** (`LEAD_PAT`): the order of chord tones, one 8th note each.
- **landing-1 to landing-6** (`LEAD_LANDING_AT`, `_LEN`, `_W`): how a chord change lands on the skeleton note: the note at `at`
  for `len` 16ths (C4, a placeholder), `weight` the share of chord changes in percent. Landing 5 (pushed) is played
  from the and of 4 of the bar before; its `at`/`len` are the table's values, which the engine doesn't use.

## melody-cells

`MELODY_CELLS` and `MELODY_CELL_W` (7; `patterns_gen.c`, `melody.c` draws them). One beat of rhythm (the first beat of a 4/4 bar; the rest of the bar
is a rest), each note held to the next onset or the end of the beat, on a placeholder C4; `weight` (how often the
melody draws it) is in the metadata.

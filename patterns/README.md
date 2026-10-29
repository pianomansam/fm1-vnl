# VNL-1 patterns

The engine's fixed pattern tables, one pattern per file, as MIDI (`.mid`, for a DAW) and MusicXML (`.musicxml`, for
MuseScore 4). Edit either one, or add a new file beside them; the importer (`tools/patterns/import.sh`) reads them
back into the engine's tables.

Regenerate from the engine with `tools/patterns/export.sh` (it builds `tools/patterns/dump.c` with the host compiler
and runs `tools/patterns/export.py`).

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
  file name's id and `model.json` still identify the entry.
- **`model.json`:** every field of every entry (the source notes, moods, modes, weights, `status`), keyed by id, in
  table order. All exported entries are `"approved"`.
- **`.manifest.json`:** the SHA-256 of each `.mid` and `.musicxml` as last written, so the importer can tell which
  file of a pattern Sam edited (either may be edited alone).
- **Names are stable, and export never deletes:** an entry's file name is fixed when it is first exported and kept
  in `model.json` (`file`); a later change to its notes never renames it. Export never deletes a file, and never
  overwrites one whose SHA-256 differs from `.manifest.json` (an edited file): it skips it and says so. Importing is
  the path for edits.

## drums

`LIB_PATTERNS` in `app/song/library.c` (8). Two bars, the GM drum map: kick 36, snare 38, closed hat 42, open hat 46,
shaker 70, rim 37 (the drum staff: kick F4, rim B4 x, snare C5, shaker E5 triangle, closed hat G5 x, open hat A5
circle-x). Velocity > 60 is a hit (written 90), ≤ 60 a ghost (written 40, head in parentheses). Bar 2 is bar B of a
two-bar pattern, else bar A again. Pattern 5's shaker is a period-3 polymeter (`poly3`: a hit every 3rd 16th, counted
from the song's first step), written out over the two bars, with `poly3` in the metadata. Bar 1's shaker is the
table's shaker mask (`hit[D_SHAKER]`, 0x9249: steps 0, 3, 6, 9, 12, 15); bar 2's shaker differs only because the
polymeter runs on, so it doesn't make the pattern two-bar (`two_bar` stays 0; the other lanes decide that).
`family`, `half_time` and `src` are metadata.

## kick-phrases

`kickPhrases` in `workbench/options.json` (5). Two bars, kick only (note 36): bar 1 is `a`, bar 2 is `b`.

## progressions

`LIB_PROGS` in `app/song/library.c` (44). Chord symbols in C, one chord per its `bars_per_chord` bars (a 2-bar chord
is tied across the bar line), as MusicXML `<harmony>` and MIDI text events, over the chord's tones (root, 3rd, 5th,
7th, root between F3 and E4). The symbols are spelled in the first mode the progression lists (`symbols_mode`; a
`Q_DIATONIC` chord takes that mode's 7th chord). `kind`, `moods` and `modes` are in the title and the metadata, and
the exact `{degree, alter, quality}` of each chord is in the metadata (`Dm7` from `Q_MIN7` or from the mode's own
chord look the same on paper).

## comp-rhythms

The keys' comping rhythms in `app/song/harmony.c`: held, half and stabs, over a reference Cmaj7 (C4 E4 G4 B4; the
pitches are placeholders, only the rhythm counts).

- **held:** one strike on step 0, held the bar.
- **half:** strikes on steps 0 and 8, a half note each, the second at 90% velocity.
- **stabs:** a stab on step 0 and a second stab, an 8th long. The second stab's candidates (`SECOND`: steps 6, 10,
  4, 12, tried in that order where the kick leaves the step free) are written as alternates, one bar each: bar 1 is
  the first candidate, bar 2 the second, and so on. Over a kick of more than 3 hits a bar (`busy_over`), the stabs
  are steps 0 and 6 (`busy_steps`), in the metadata.

## strum

`PAT` (4) and `BALLAD_FILL` in `app/song/strum.c`. One pitch per stroke type, as drum lanes (the bossa is 2 bars,
the rest 1): T thumb E4 (64), B thumb and pinch F4 (65), D accented down G4 (67), d down A4 (69), X chuck B4 (71, x
head), U up C5 (72), u soft up D5 (74), P pinch E5 (76). A rest is no note. Up strokes (U, u) go only on the "and"s
(steps 2, 6, 10, 14). Id 5 is the ballad's fill-bar variant (`fill_of` 4).

## lead-arp

`app/song/lead.c`, over a reference Cmaj7 voicing C4 E4 G4 B4 = chord tones 1-4 (the lowest note is the chord's 1st
tone, the next its 2nd, ...).

- **rhythm-1 to rhythm-6** (`RHY`): the arpeggio's onsets and lengths in a bar; the pitch is a placeholder (C4).
- **pattern-1 to pattern-6** (`PAT`): the order of chord tones, one 8th note each.
- **landing-1 to landing-6** (`AT`, `LEN`, `W`): how a chord change lands on the skeleton note: the note at `at`
  for `len` 16ths (C4, a placeholder), `weight` the share of chord changes in percent. Landing 5 (pushed) is played
  from the and of 4 of the bar before; its `at`/`len` are the table's values, which the engine doesn't use.

## melody-cells

`CELLS` and `CELL_W` in `app/song/melody.c` (7). One beat of rhythm (the first beat of a 4/4 bar; the rest of the bar
is a rest), each note held to the next onset or the end of the beat, on a placeholder C4; `weight` (how often the
melody draws it) is in the metadata.

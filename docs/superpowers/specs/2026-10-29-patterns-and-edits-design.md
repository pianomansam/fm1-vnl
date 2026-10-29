# VNL-1: patterns out and in, piano-roll edits, the lead-doubling switch

Status: draft for Sam's review (2026-10-29). Builds on the workbench designs (2026-10-28, 2026-10-29); their
constraints hold.

## Intent

Sam wants to shape the music himself, not only judge rolls. Two paths, by how the music is made:

- **Patterns that are data** (fixed tables): export them as MIDI and MuseScore files he edits and adds to, and import
  them back into the engine.
- **Parts made by rules** (bass lines, voicings, pads, melodies): edit the notes in the workbench's piano roll, and say
  why, so Claude can turn the edits into rules.

Plus one switch: let the lead double notes the other voices play (the rules that prevent it, off).

## 1. Patterns out and in

**Families exported** (each one file per pattern, `.mid` + `.musicxml`, in `patterns/<family>/`, with a README giving
the conventions):

| Family | Source | Written as |
|---|---|---|
| drums | LIB_PATTERNS (8) | 2 bars, GM drum map (kick 36, snare 38, closed hat 42, open hat 46, shaker 70, rim 37); velocity > 60 a hit, ≤ 60 a ghost. Pattern 5's shaker (every 3rd 16th) written out. |
| kick-phrases | workbench options.json (5) | 2 bars, kick only |
| progressions | LIB_PROGS (44) | chord symbols in C (degree + quality), one chord per its bars; mood/mode/kind in the file's title/metadata |
| comp-rhythms | held, half, stabs | rhythm over a reference Cmaj7 (pitches are placeholders) |
| strum | strum.c PAT (4) + ballad fill | one pitch per stroke type (D, d, U, u, X, T, P, B), as drum lanes |
| lead-arp | lead.c RHY (6), PAT (6), landings (6) | over a reference Cmaj7 voicing: the lowest note = the chord's 1st tone, the next its 2nd, … |
| melody-cells | melody.c CELLS (7) | one beat of rhythm |

**Import:** `tools/patterns/import.sh` reads every file in `patterns/` (MusicXML from MuseScore, or MIDI from a DAW;
when both exist for a pattern, the newer one), validates it against the engine's rules, and regenerates the engine's
tables from it (a generated C file per family, replacing the hand-written table). Errors name the file and the rule,
and change nothing. Rules checked include: a drum pattern has a kick and a snare; a progression uses allowed chords
only (no substitutions, dominants at the 9th or above); strum up-strokes on "and" steps; quantized to 16ths.

**After import:** the engine suite runs; new patterns appear in the workbench as **candidates** (Sam approves them
there); the pinned hashes move only when an existing pattern changed or one was added, and the importer says so.
Export of today's tables, then import with no edits, regenerates byte-identical tables (the round-trip test).

## 2. Piano-roll editing (parts made by rules)

- **Parts:** keys, bass, lead, pad (drums keep the drum screen).
- **Edits:** add a note (click), delete (select + Delete), move (drag in pitch/time, snapped to 16ths), resize (drag
  the end). Velocity: up/down keys on a selected note.
- **What they change:** what is heard. Applied after the engine makes the part (a default-off workbench filter: delete
  these, add these); the engine's other rules (the keys' duck, clash checks) don't react to them.
- **Why:** saving a part's edits asks "why?" (required, free text). The entry goes to feedback (`about: "edit:<part>"`)
  with the notes before and after and the full snapshot, so Claude can replay it and turn it into rules.
- **Lifetime (Claude's call):** edits belong to the part's current roll and live in its history entry: ◀ ▶ bring them
  back with their roll. Re-rolling an edited part, or a new seed, asks before setting edits aside (they stay in
  history). Mute/solo, section jumps and loop work as before.

## 3. The lead-doubling switch

- A per-song override, `lead_double` (0 = rules on, the default; 1 = off): every rule that keeps the lead from
  playing a pitch, or its octave, that another voice (keys, bass, pad) plays at the same time is skipped. The builder
  lists those rules in the report. Rules about rubs (semitones), avoid notes and range stay on.
- Workbench: a row "Lead may double other voices" (on/off), in snapshots and feedback.
- Firmware: unchanged (default on). Turning it on in firmware would be a later decision.

## Non-goals

- Editing rule-made parts by file (they have no table).
- Editing velocity curves, swing, or humanization.
- Anything on the FM-1 beyond the regenerated tables (which the firmware compiles as before).

## Constraints

As the workbench designs: local only, plain HTML/JS, Python stdlib only for tools, default-off hooks, approved
options only, US English, never push or merge without Sam asking.

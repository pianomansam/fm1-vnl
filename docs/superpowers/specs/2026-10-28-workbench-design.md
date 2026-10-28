# VNL-1 Workbench: design

Status: draft for Sam's review (2026-10-28).

## Intent

Sam keeps needing to hear a decision in context before he can make it: which kit, which kick phrase, which
pairing of keys and lead. So far every question has meant Claude rendering WAVs by hand, an hour or more a round,
with approximations (drums swapped in from MIDI, an EQ standing in for the song's chain). The workbench removes
that loop: a local web page that plays real VNL-1 songs, lets Sam change any decision the engine made or re-roll
one aspect, and sends his feedback to Claude in a form Claude can reproduce exactly.

It exists to unblock decisions **now**. It is not the FM-1 firmware and does not have to run on the device.

Success: Sam opens it, plays a song, swaps the kit or re-rolls the lead, hears the change within a bar, says what
works, and Claude can read that feedback and rebuild the exact song he heard.

## Goals

### Main page (layout A, "the mixing desk")

- **Transport:** play, stop, "re-roll everything unlocked", the song's position (time and bar) and a progress bar.
- **Decision rows**, grouped. Each row: its current value in a dropdown, 🎲 (re-roll just this, by the engine's own
  rules), 🔒 (keep it when re-rolling others).
  - **Song:** seed, key/mode, tempo, swing, mood, motion, arrangement map, progression, color.
  - **Instruments:** family (acoustic, synth; electric goes with either), keys, comping style, lead, bass, pad,
    ear candy.
  - **Drums:** kit, pattern, kick phrase, ride section. A button opens the drum screen.
  - **Texture:** the bed (dust, vinyl, rain).
- **Changes land at the next bar** without restarting the song (Claude's call; Sam agreed the plan "sounds good").
  Seed and the arrangement map regenerate the song and restart it (they redraw everything).
- **Feedback:** 👍 / 👎 and a note, for the whole song or for one row. Every entry is saved with a full snapshot of
  every decision and the playback position, so Claude can reproduce it.
- **Families:** the family rule Sam approved (2026-10-28) applies: a song's supporting roles (bass, pad, ear candy)
  and its drums follow its family. Re-rolling respects the family; a locked or hand-picked row overrides it.

### Drum screen

- The current kit's lanes: kick, snare, closed hat, open hat, rim, shaker, ride, toms/crash. Each lane: a sound
  picker drawn from **any** kit's sounds, 🎲, an audition button (the sound alone) and a level.
- A **pattern grid**: two bars of 16th steps per lane, so two-bar kick phrases (k2-k6) fit. Clicking a step cycles
  empty → hit → ghost. The song keeps playing and picks up edits at the next bar.
- **Save as new kit** / **Save as new pattern** write the edit to a file Claude reads and turns into real kits and
  patterns.
- Per-hit velocity editing: not in v1 (Sam: "sounds good" to the grid without it).

### What it plays

- The real engine: the song engine (app/song) and the voices (app/engine) compiled for the browser, as the
  existing browser emulator already does for the whole app (base/web/emu: wasm in an AudioWorklet). What Sam hears
  is what the engine makes, not a stand-in.
- The new drum candidates are playable in it: poketrack 808 and CR-78 (zlib), Optimist LO-FI (GPL-3.0-only), and
  Gogodze and Big Rusty (sticks, brushes) as shipped (CC0). The workbench may use more CPU and memory than the FM-1
  allows.

## Non-goals

- Running on the FM-1, or fitting its CPU, flash or RAM budgets.
- The song timeline (layout B: sections across the top, feedback pinned to a moment). Next, after this.
- Decisions made inside the engine per chord or per note (a single chord's voicing, one lead note).
- The drummer layer (8th hats with accents, two-bar variation, tom fills, crash). Designed separately; the
  workbench gains a toggle for it when it exists.
- Phones, other people, hosting it anywhere.

## Anti-goals

- **Sounding different from the engine.** No approximations: if a sound isn't in the engine yet, the workbench
  doesn't fake it.
- **Offering removed sounds as if they were options** (the upright bass, X0X 808, poketrack 909, Optimist BOOMBAP
  and vintage, our synth 808/Dusty/Soft, the mixed sampled kits, the arpeggio comping). Sam has been burned by demos
  using removed parts twice. Retired options appear only behind a "show retired" switch, off by default, and are
  marked as retired.
- Losing feedback, or feedback Claude can't reproduce.
- Waiting: no render step between a change and hearing it.

## Constraints

- Local only: a page served from the repo by a small local server that also writes the feedback and saved kits and
  patterns to files in the repo.
- GPL-3.0-or-later for VNL-1's own code; copied Optimist code is GPL-3.0-only and keeps its header (the license
  survey's note). Credits in CREDITS.md.
- US English. Commits use the noreply address; nothing is pushed or merged without Sam asking.
- Subagents never delete anything under app/build/new except folders they create.

## Decided by Claude (Sam can overrule)

- **Where:** `workbench/` at the repo root (page, server, build script); its wasm build goes to
  `app/build/workbench/`.
- **Feedback file:** `workbench/feedback.jsonl`, one JSON line an entry: time, the full decision snapshot (every
  row's value, locks, the seed), the song position (seconds and bar), the row it's about (or "song"), 👍/👎 and
  the note. Saved kits and patterns: `workbench/kits/*.json`, `workbench/patterns/*.json`.
- **Sample quality:** the shipped acoustic kits play at full quality in the workbench (the device would play them
  at 16 kHz ADPCM once packed); a "device quality" switch is a later nice-to-have, not v1.
- **Dropdowns list current, approved options only**; "show retired" adds the rest, marked.
- **No framework:** plain HTML, CSS and JavaScript, no build step for the page; the engine's wasm build follows the
  existing emulator's pattern.
- **How a change lands (re-generate and catch up):** any change rebuilds the song with the new decisions and runs
  the engine silently from bar 1 up to the next bar line (the engine is deterministic and fast), then sound resumes
  from there. So voice leading, the lead's phrase and every other state are exactly as if the song had always had
  that decision; the only seam is notes that were sounding at the switch. No engine refactor for mid-song changes.
  Seed and arrangement-map changes restart from bar 1.
- **🎲 changes only its row.** Other rows keep their values, unless the new value makes one impossible (a lead
  outside the new family, an instrument's range); then that row re-derives too and is highlighted so Sam sees what
  else moved. Locked rows never move; a locked row that the new value conflicts with blocks the roll and says why.
- **Locks and hand edits survive a regenerate** (seed, map). Dropdowns list the engine's library entries (the
  maps, progressions, sets, instruments, patterns in app/song/library.c), not free-form structures.
- **Reproducing feedback:** a snapshot replays in the workbench (or a host build of the same engine code Claude can
  run headless); the offline new-song.sh path needn't accept it. Each feedback entry also carries the list of
  changes made since the song started (time, bar, row, old → new), so a mid-song switch is visible.
- **Approved vs retired lists** live in one hand-maintained file, `workbench/options.json`, seeded from the
  ledgers (docs/superpowers/plans/*-ledger.md); each option is marked approved, candidate or retired.
- **The drum data model is the workbench's to define** (the drum reset isn't built yet): a kit is per-lane sounds
  and levels; a pattern is two bars of 16 steps per lane with hit/ghost; the kick phrase is part of the pattern.
  The firmware's drum reset will later adopt it.
- **New drum candidates are workbench-only** until Sam picks: poketrack, Optimist LO-FI and the shipped acoustic
  kits go under `workbench/` (with their licenses; Optimist files keep GPL-3.0-only headers), not into `app/`.
  Sampled kits are pre-mixed to mono per hit at 44.1 kHz with a few velocity layers and round robins, a few MB in
  all; the wasm memory grows as needed.
- **Browser:** Chrome or Safari on macOS; no wasm threads (the emulator runs without them).

## Left to the builder

- How decisions reach the engine (likely: the roll fields plus per-role overrides, exported from the wasm build).
- How a kit, pattern or kick phrase becomes data the engine plays (the drum reset will need this anyway).
- How 🎲 re-rolls one aspect with the engine's own rules (likely: a fresh seed stream for that field only).
- The server (likely a few dozen lines of Python's standard library).

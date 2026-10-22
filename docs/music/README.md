# VNL-1 music knowledge: a map

What VNL-1 knows about music, split by **why** a rule exists. When a song sounds wrong, the first question is
which kind of rule failed: good musicianship (fix it whatever the genre) or a lo-fi / taste choice (retune it, or
argue with it).

| File | Holds | Changes when… |
| --- | --- | --- |
| [general.md](general.md) | Genre-independent musicianship: chord-scales and avoid notes, extensions, voice leading, bass lines, rhythm and humanization, form and variation, melody (motif, phrasing, repetition), register and mix. Each principle with its status in VNL-1 (file and function) and its source. | Rarely. A fix here would hold for jazz, pop or film music too. |
| [lofi.md](lofi.md) | Lo-fi hip hop genre choices and Sam's taste: tempo, swing, song length, maps, one loop per song, instrumentation, harmony vocabulary, the no-substitution and diatonic-only rules, dust and bed, novelty between songs. Each with its source and status. | Whenever VNL-1 would become another genre, or Sam's ear disagrees. |
| [ideas-from-other-apps.md](ideas-from-other-apps.md) | Ideas from MMA, Impro-Visor, JJazzLab, Strudel/iReal, Tonal.js, the Dias–Guedes walking-bass paper, autocomp, accompy, jacbz Lofi and other FM-1 firmwares that VNL-1 has **not** adopted, classified general / fits lo-fi / does not fit lo-fi, with cost and priority. | As ideas are adopted (move them into general.md or lofi.md) or rejected. |

Raw sources stay in [`docs/research/`](../research/): the genre study, measured arrangements (19 audio tracks,
147 MIDI files), the firmware notes and the auto-accompaniment comparison. The files here distill them; when the two
disagree, the research is the evidence and these files are the current reading of it. Decisions and their reasons
live in the plan ledger ([`2026-10-22-song-engine-ledger.md`](../superpowers/plans/2026-10-22-song-engine-ledger.md),
rulings R1–R30 and "To revisit") and in commit messages.

## How to use it

- **Listening to a render.** The host simulator writes `app/build/scenarios/songs.log` (every roll and one line
  per bar) and per song `songs-N.mid` (keys, bass, lead, candy, drums on separate channels) and `songs-N.musicxml`
  (a quantized score with chord symbols). Find the bar in the log, open the MIDI or score, then look up the rule
  in general.md or lofi.md by part (harmony, voicing, bass, rhythm, melody, form).
- **Tuning by ear in the curation bench.** General rules are guard rails: keep them on and tune inside them
  (the clash checks in `tests/host/clash_test.c` enforce most). Lo-fi rules are the knobs: tempo and swing ranges,
  map shapes, progression weights, comp and bass styles, candy density. A rating of "fix" on a library entry usually
  lands in lofi.md's territory; a rating of "wrong note" usually means a general rule has a hole.
- **Proposing a change.** Say which file the rule belongs in and give the measurement (how often, over how many
  songs), as the ledger and commits do. Sam wants the reasoning and the numbers, not just the change.
- **Spelling.** Keys and notes in their simplest spelling everywhere, in docs, logs and screen text: Bb not A#,
  Db Lydian not C# Lydian, no double sharps or flats.

## Labels used in these files

- **Done** — implemented; the file and function are named. **Partial** — some of it. **Not yet** — agreed or
  proposed, not built.
- **(Sam)** — Sam decided it, usually after listening. **(call)** — Claude's call, open to change.

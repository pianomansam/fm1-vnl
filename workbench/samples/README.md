# The workbench's sampled kits

Karoryfer's Gogodze Phu Vol II and Big Rusty Drums (sticks, and its brushes), both CC0 1.0, played as their makers ship
them: each SFZ program's default mic mix, every piece mixed to mono per velocity layer and round robin. Credited in
`CREDITS.md`. The sample files are git-ignored; `kits.json` (their index) is generated with them.

    python3 -I workbench/tools/prep_kits.py FETCH_DIR workbench/samples

fetches each source (shallow and sparse, at the commit pinned in `prep_kits.py`: the programs, the license, then only
the samples the pieces use, about 160 MB with the git objects) into FETCH_DIR (any folder outside the repo; a second run
reuses it), and writes the float32 mono 44.1 kHz files here (about 110 MB) and `kits.json`. Big Rusty's samples are
FLAC: the script decodes them with `flac`, or with macOS's `afconvert`. Its test: `python3 -I
workbench/tools/test_prep_kits.py`.

- **Gogodze** (the page's Dusty): kick, snare, sidestick (the rim lane), closed and open hat. No ride: the ride lane
  plays its closed hat. No shaker.
- **Big Rusty (sticks)** (the page's Room - big): kick, snare, sidestick, closed and open hat, the 22" ride. No shaker.
- **Big Rusty (brushes)** (the page's Room - brushes): the same kick, the brush snare, hats and ride; no brush
  sidestick, so the rim lane plays the brush snare at 45% of the velocity. No shaker.

Each kit's level, and its kick's against its snare, is set in `workbench/engine/wb_drums.c` (`SMP_DB`,
`SMP_KICK_DB`): kick and snare at the Soft brushes kit's. A kit whose files aren't all here shows as unavailable in
the page's kit picker and can't be chosen; the page loads a kit's files the first time it is chosen.

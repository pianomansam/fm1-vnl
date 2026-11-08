# Lead loop: the lead as a sampled record (design)

> **Superseded in part (2026-11-04):** every lead style loops (23ed21a), Bossa records a riff (3732a7d), the loop has
> a pitch floor (797877d), and the loop is on by default (8c230af). See the song-engine ledger's lead loop entry.

Date: 2026-11-03. Ledger entry: "Hip hop record-style repetition for leads" (song-engine ledger). Prototype: branch
`worktree-agent-ac7ec4ef3495a7e8f` (d8adab7 the loop, f0d5936 Jazz-Hop's riff bar; f73c9b5's hook floor is already
on workbench as d5c4d84). Renders: app/build/new/lead-loop-2026-11-03/ (INDEX.md).

## Intent

Make the lead sound like a phrase a producer lifted from a record and looped: recorded once, replayed identically,
varied only by what a producer does to a loop (filter it in, mute a bar, drop it before a change, let a few notes
stick and skip). Sam (2026-10-29): "Loops are actually good. And that's behind my request to have less variance."
Sam on the prototype (2026-11-03): Ambient "works well. It's not too much." Jazz-Hop was too sparse until its hook
got a riff bar and a pitch floor ("I guess this is better. Keep the change").

## Goals

- A per-song switch, lead loop on/off. Off: the style plays exactly as it does today (byte-identical renders).
- With it on, for Jazz-Hop and Ambient: the prototype's behavior, kept as built and measured (below).
- A workbench row "Lead loop" (Default / Off / On, like the lead-style row; Default follows the firmware constant)
  so Sam can A/B any song. Claude's call. The pinned-hash tests force the switch off, so they hold whichever way the
  default is set.
- A firmware default (a constant), set by ear at the end of the build.

## Non-goals

- Other lead styles. They ignore the switch for now; each can be added later by ear (Claude's call: the plan
  said start with Jazz-Hop and Ambient, and only those were heard).
- A device control for the switch (no front-panel UI); the default constant covers the device.
- Re-pitching the whole loop to follow the chords, muting whole sections, 1-bar loops (from the ledger idea; not in
  the prototype, not heard).

## Anti-goals

- New notes on replay. Every replayed note is a recorded note (moved at most 2 scale steps for harmony, or dropped).
- A loop so busy it's "too much" (Sam's Ambient bar).
- Any change to a song with the switch off.

## The behavior (as the prototype built it; keep)

- **Record:** the phrase is recorded the first time the lead reaches each bar, L bars long, L = the chord loop's
  length (2 or 4; 4 at most). Each later lead bar replays slot bar % L exactly: notes, onsets, lengths, velocities,
  triplet lateness.
- **Jazz-Hop's riff:** while recording, its hook stays inside its bar (no tie over), and bar 2 of each 4-bar block
  restates the hook a pentatonic step under its held tone (never on it), softer, its first note chopped when it has
  3+. About 2.9 notes per bar (Ambient 2.2).
- **Harmony on replay:** each note is checked against the bar it plays over (that section's chord, keys and bass,
  a chord tone on beats 1 and 3, the bar's range). A rubbing note moves to the nearest passing note within 2 scale
  steps, else drops. A note ringing over a bar line is cut there if it would rub over the next chord.
- **Producer moves** (from the song's phrase seed; rates per the prototype's loop.c header):
  - entry: the song's first lead entry filtered in 40% (low-pass ~600 Hz for 2 bars, opening over 2); later entries
    held back 4 bars 20% or filtered in 35%; a block straight after another filtered in 15%;
  - a muted hook bar in 12% of 8-bar passes, never the first pass;
  - before a section change, 50%: the whole last bar out, or its last 2 beats;
  - the stuck-note stutter: on the turnaround's hook bar 40%, on a block's last bar 30%, never in the first lead
    pass; one note 55%, 2 notes 30%, 3 notes 15%; repeats 2-4 (fragments 2-3) in 16ths or 8ths, each 12% softer;
    then the phrase resumes where it would have been. About 0.75 stutters per song. Made bolder and more frequent
    (Sam, 2026-11-05; leads/loop.c): 70% and 50%, not where the section already filters the lead; a note 4-7 more
    16ths (its last beat in 32nds 1 in 3), a fragment to about 2 beats; full level, the lead's low-pass closing to
    0.75 by the last repeat. About 0.86 a song, 36% of songs none (was 0.68 and 49%).
- **The lead filter:** a low-pass on the lead bus driven by the song (the prototype's `song_lead_lp` hook, one line
  each in app.c and wb.c).

## Live changes on the device

When the player changes key, mode, chords or set mid-song (the request lands on the next 4-bar line), the loop
**re-records** (Sam, 2026-11-03): the old phrase is dropped, the style records a fresh phrase from that bar, and its
first replay enters as a new sample would (the entry moves above, e.g. filtered in). No transposing. The prototype's
recording signature (phrase seed, key, mode, chords, set, style) already does this.

## Changes from the prototype

- **No stutter in the bar right after a stutter** (prototype seed 119 stuttered at 1:49 and 1:53). Claude's call.
- **The switch is a real per-song override** (song_ov, like lead style), not an environment variable or a global;
  the workbench preview instance and the playing instance keep their own move counters (the prototype shared them).
- **Stats and the per-bar move log** are test/workbench-only, not in the firmware image.
- Jazz-Hop's turnaround variation is lost while looping (the loop replays bar 8 as recorded). Accepted: that's the
  record feel. Claude's call; revisit by ear.

## Constraints

- Flash: the prototype measured +3,296 B for the loop; Jazz-Hop's riff bar adds ~0.3 KB (estimate). The image is
  399,240 B of 409,600 at d5c4d84, so about 6.5 KB stays free. Measure with `./build.sh`; record in docs/cost.md.
- RAM: about +576 B (the recorded phrase). CPU: no measurable change in the prototype; re-measure with tick_bench.
- Renders go through the workbench engine (wb.wasm in Node); the firmware's kits are retired.
- GPL-3.0-or-later for new files. No artist names anywhere.

## Tests (the builder's to shape)

- Off is byte-identical to today (pinned rolled-song hashes unchanged with the switch off).
- On: replayed bars equal their recorded slot except for harmony moves, drops, cuts and the moves above.
- No replayed note fails the legality check; the moved/dropped rates stay near the prototype's (300 songs: about
  169 nudged, 1 dropped).
- Stutter count per song near 0.75 (0.75-1.0 since the bolder stutter) and never two stutters in adjacent bars; none in the first lead pass.
- Styles other than Jazz-Hop and Ambient are unaffected by the switch.

## Left to the builder

The file layout (porting loop.c from the prototype is expected), how the override is stored, the workbench row's
placement, and the test structure.

## Decided at the end, by ear

The firmware default (on or off), from one A/B of each style with the default each way.

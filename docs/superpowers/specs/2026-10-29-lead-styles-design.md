# VNL-1: the lead styles, built into the engine

Status: draft for Sam's review (2026-10-29). Sam's decisions are in the song-engine ledger
(docs/superpowers/plans/2026-10-22-song-engine-ledger.md, "Leads").

## Intent

Sam auditioned lead styles prototyped as throwaway spikes (research in docs/research/lead-*-2026-10-29.md and
lead-impressionism-2026-10-29.md; code on `worktree-agent-*` branches; renders in app/build/new/lead-*-2026-10-29/) and
shortlisted eight. He judged them "light years ahead" of today's leads. This builds those eight into the engine as
the only lead generators, removes the old ones, and fixes what the spikes exposed: a lead squeezed by the keys, leads
he no longer wants, and flash.

## Goals

1. **Eight lead styles**, by their genre names (Sam's), each a real, tested engine component:

   | Style | From spike | Best lead instruments (from the spikes) |
   |---|---|---|
   | Impressionism | Impressionism, `aaf267bc` (lead_style 2) | flute, vibes |
   | Romanticism | Romanticism, `aeec8cac` | flute |
   | '60s Jazz | the '60s jazz pianist, `a4802cdb` | felt piano, vibes |
   | Modal Jazz | the modal-jazz trumpeter (the modal-jazz album), `a8cba358` | flute |
   | Jazz-Hop | jazz-hop, `acdb178f` | flute, felt piano, vibes |
   | Film Score | film score, `a58dfa51` | flute (Knight truest) |
   | Bossa Nova | bossa nova, `a8d4733e` | flute, vibes |
   | Ambient | ambient, `aedeac7d` | felt piano, vibes |

   Each style keeps its spike's rules as Sam heard them. Sam: "Don't go back and rework any motifs." The fixes below
   apply to all of them.
2. **Stay on the motif.** "Loops are actually good" (Sam), and "too much variance after the initial line ... sounds
   like it's wandering". Every style restates its opening motif recognizably and varies one thing at a time. Each
   style's restatement shares are measured and tested at the levels its spike reached.
3. **One motif format shared by all styles** (rhythm, contour and scale steps), so a later feature can hand one style's
   motif to another: "Hybrid lead styles" in the ledger's To revisit list. Only the format is built now, not the
   handoff.
4. **The keys make room for the lead** (Sam: "go with the first"). The fixed floor goes: today the lead must stay a 4th
   over the keys' top note (`lead.c`, `c.lo = top + 5`), which squeezed several spikes. Instead, when the lead needs
   room, the keys voice in a lower window. The voice-leading rules (rootless, one voicing per chord, smallest moves,
   no semitone rubs) still pick the smoothest path inside that window.
5. **Remove the old lead engines** (Sam): the phrase planner and the hybrid arpeggio. Also removed:
   - the lead-arp pattern family (patterns/lead-arp, its tables, export/import support and workbench rows);
   - the struck-melody path (melody.c, the motif and the melody-cells family), which never sounds;
   - the lead-doubling override (its one rule lives in the hybrid).
6. **Lead instruments: flute, felt piano, sampled vibes, Knight** (Sam removed kalimba, Emilyguitar, saw lead and synth
   pluck as leads; Emilyguitar stays on keys). The 10 sets that lose their lead get a new one from these four:
   - 5: kalimba;
   - 10, 17, 20: synth pluck;
   - 13, 18, 19, 33: saw lead;
   - 28, 29: Emilyguitar.

   No style may put the same instrument on keys and lead.
7. **Which style a song plays** is rolled per song, weighted by its lead instrument (the table's best fits) and the
   set's flavor (e.g. Bossa Nova mostly with bossa sets, Jazz-Hop common everywhere). The workbench's lead-style row
   lists the eight, and the lead part's re-roll stays.
8. **Flash back under budget.** The image must be ≤ 409600 B. With two spikes merged it was 413776 B before any new
   instrument. Each style's code size is measured; the removals above are counted first; anything further is
   proposed to Sam with bytes, not cut silently.
9. **The name scrub** (Sam): no composer or performer behind any lead style stays anywhere in the repo's lead-style work.
   - Covered: code, comments, UI strings, docs, render folders and file names, branch names, commit messages, and the
     unpushed `workbench` branch's history.
   - Research docs are rewritten under genre names, describing traits without the person or their pieces.
   - The spike branches and worktrees are deleted.
   - Scope (Sam): lead-style names only. Older research and drum pattern source notes stay.

## Non-goals

- The hybrid handoff between styles (ledger To revisit). Only its motif format is built now.
- New lead instruments: guitar, B3 and legato strings are parked.
- Post-Bop (its v3 rules are in the ledger), Cool Jazz and Neoclassical. Not shortlisted.
- Piano-roll editing (paused).

## Anti-goals

- A style that sounds different from the spike Sam approved. Any change to a style's notes beyond the shared fixes
  needs an A/B he hears.
- A lead that breaks the rules: no semitone rubs against keys, bass or pad; no avoid notes on strong beats; velocities
  25-110; chords never changed.
- Any artist name left behind after the scrub.

## Constraints

- **Engine:** C under app/, the -Werror host suite, and the firmware build (`./build.sh`).
- **Tests:** every style gets host tests for the rules above, its motif restatement, density and range. The clash
  tests run over all eight.
- **Pinned hashes** re-pinned once, with a comment naming this change.
- **Workbench** keeps working: rows, part re-rolls, feedback replay, snapshots. Old snapshots naming a removed lead
  style or instrument restore onto a stand-in, as retired instruments do.
- **Never pushed or merged without Sam asking.** Rewriting the unpushed history for the scrub is done only after Sam
  confirms at that step.

## Decided by Claude (Sam can overrule)

- **Spikes are reference, not merged.** Each style is re-implemented cleanly from its research doc and spike: one file
  per style under app/song/leads/, with a shared motif/phrase helper. The 8 spike branches each hold renumbered
  copies of lead.c with debug counters, so merging them would be messier than rewriting.
- **Order:**
  1. the shared format and the keys' room;
  2. the styles, two at a time, each A/B-checked against its spike's renders (same seeds, same notes where the rules
     are unchanged);
  3. the removals;
  4. the sets and the style roll;
  5. flash;
  6. the scrub, last.
- **Style weights by instrument:**
  - flute: all eight;
  - felt piano: '60s Jazz, Jazz-Hop, Ambient, Film Score;
  - vibes: Impressionism, Jazz-Hop, Bossa Nova, Ambient, '60s Jazz;
  - Knight: Film Score, '60s Jazz, Ambient.
- **Lost sets' new leads:** synth sets get vibes (electric), acoustic sets flute or felt. The vibes share is kept near
  its rebalanced level (5-7 sets).
- **Keys' room:**
  - the lead claims a band of about an octave around its motif's center;
  - the keys voice below the band's floor when they would reach into it, within their instrument's range;
  - if that's impossible, the lead's band moves up instead (today's behavior).

- **Spike fidelity (answered by Sam: "Don't go back and rework any motifs").** All eight are ported as the spikes
  were and tested at the restatement levels their spikes reached; no style is tightened. The spikes' open quirks
  (e.g. Impressionism rarely striking beat 1, triplets not taking the swing, the felt piano's ties decaying,
  unplaceable notes dropped) carry over unchanged and go to the ledger's To revisit list.
- **Register without the floor:** each style keeps a center per lead instrument, set to its spike's measured median
  lead pitch in the five A/B songs (clamped to the instrument's range); the lead's band is about an octave around it.
  This keeps the A/B against the spike renders meaningful.
- **Keys' room over time:** the keys' window is set once per song from the lead's band and holds all song, lead playing
  or not, so voicings stay one per chord (cached as today). It applies to percussive keys only; sustained keys (pad,
  organ) keep their placement path. The pad layer's chord part follows the keys' voicings, so it moves with them.
- **Candy fields:** the ear candy part is gone, so a set's candy field doesn't constrain its lead.
- **Families of the lost sets:** Rhodes and Wurli are electric (either); nylon is acoustic.
- **Style roll:** weight = instrument fit × flavor weight. Instrument fit: 1 for the instrument's styles above, 0
  otherwise. Flavor weights (×): lofi — Jazz-Hop 3, the rest 1; neo-soul — '60s Jazz 2, Modal Jazz 2, Jazz-Hop 2;
  city pop — Film Score 2, Jazz-Hop 2; chillwave — Ambient 3, Impressionism 2; bossa — Bossa Nova 4, others 1 (Bossa
  Nova 0.25 outside bossa sets). Sam tunes these by ear in the workbench.

## Left to the builder

- The exact module boundaries, and the size of each style's code.
- How the style roll draws (its stream, and keeping the other streams undisturbed where possible).
- Which flash trims to propose if the removals aren't enough.

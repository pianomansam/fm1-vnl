# Lo-fi hip hop and Sam's taste

Everything here would change if VNL-1 played another genre, or if Sam's ear said otherwise. Genre-independent
musicianship is in [general.md](general.md). Each entry says whether it comes from the **genre** (measured or
sourced in `docs/research/`) or from **taste** (Sam's call, usually after listening), with its status.

**Refreshed 2026-10-30 against the code** for harmony, comping, bass, drums and the pad layer: each rule is written
**rule → its numbers**. **Not refreshed:** the lead and melody (being rewritten as lead styles) and the sound
sections (instruments, synths, chain, mix), kept as of 2026-10-24 and marked. Where the code and the ledger disagree,
the case is listed under "Open questions for Sam" at the end, not fixed.

Research short names: *genre* = `lofi-hip-hop.md`, *audio* = `lofi-audio-arrangements.md` (19 tracks), *midi* =
`lofi-midi-arrangements.md` (147 files), *accomp* = `auto-accompaniment-comparison.md`, *firmwares* =
`other-firmwares.md`, *amb* = `amb-1-and-vnl-1.md`.

Code paths are under `app/song/`. "Step" means a 16th note (0 = beat 1, 4 = beat 2, 6 = the "and" of 2, ...).
Percentages are the share of songs (or sections) the roll gives, computed from the weights in the code.

## What changed since 2026-10-24

- **Ear candy: removed** (b4e00ff, Sam). No song plays it; `L_CANDY` is never set.
- **The pad layer is optional:** in about 2 songs in 3, and in about 2 sections in 3 of those, with a one-bar fade
  at each section line (Sam, 2026-10-29).
- **Upright bass: removed** (Sam). Its four sets play the electric bass, which still walks there.
- **Comping by instrument:** electric and synth keys play held chords or half notes only (the 16th arpeggio is off,
  stabs are off on them). The Knight piano adds stabs and strikes without a roll. The nylon and the Emilyguitar
  strum (only under a sustained lead), stab, hold or play half notes.
- **A strum engine:** four strum patterns on guitar shapes, voice-led.
- **The keys make room for the lead:** percussive keys, sustained keys (pad below the lead) and strummed guitar
  shapes voice under the lead's band (the last two since 2026-10-30).
- **Drums:** five sampled kits (one pool of hits) beside the synth kits, about half the songs each; a ride in
  sampled-kit songs, quarter notes at most; sampled hats play 8ths at light swing.
- **Patterns as files:** drum patterns, progressions and comp rhythms are edited as MIDI or MusicXML in `patterns/`
  and imported with checks (no substitutions, no bare dominant).
- **Families (ledger, 2026-10-29):** the acoustic/synth divide applies to drums only. **Not in the engine yet** (see
  Open questions).

## Purpose: soothing, never pulling attention

Sam listens while studying and coding **(Sam)**. This purpose drives several choices below that a jazz or pop
engine would not make: zero tolerance for semitone rubs, no substitutions, diatonic bass, no startling transitions
(record skips, backspins, stutters, tape stops excluded), true silence of 2–4 s between songs. Status: **Done**
(clash checks, `app/app.c` gap, steady levels between sections and songs: `test_levels_steady`).

## Length, tempo, feel

| Choice | Rule → numbers | Kind | Status | Source |
| --- | --- | --- | --- | --- |
| Song length | A target of 120–210 s (triangle, peak 150 s), turned into bars: 36–76, a multiple of 4. | genre | **Done** (`roll.c`) | *genre* §1.1; R3 **(Sam)** |
| Tempo | 70–90 BPM. Weights rise from 1 at 70 to 10 at 79, 11 for 80–84, then fall from 6 at 85 to 1 at 90 (80–84 BPM: about 42% of songs). | genre | **Done** (`roll.c`) | *genre* §1.2 |
| Meter | 4/4 only; triplet feel through swing. | genre | **Done** | *genre* §1.3 |
| Swing | 52–62% on 16ths, a triangle peaking at 52.5%, mean about 55.5%. | genre | **Done** (`roll.c`, `groove.c`) | *genre* §3.2; *midi* §5 |
| Lateness | Snare 10–30 ms late, hats 0–70 ms, kick on the grid (numbers in [general.md §4](general.md#4-rhythm-and-humanization)). | genre | **Done** (`groove.c`, R13) | *genre* §3.3 |
| Lead swing, lead phrase starts | Lead rules: not refreshed (lead rewrite). | — | — | R16; ledger |

## Form

Not refreshed in detail; still matches `plan.c` and `library.c`, with the ear candy removed.

| Choice | Value | Kind | Status | Source |
| --- | --- | --- | --- | --- |
| One chord loop per song | 1–4 chords, cycle 2, 4 or 8 bars; no B section, no key change in the plan. | genre | **Done** (`LIB_PROGS`, R5) | *audio* §1 **(Sam)** |
| Song maps | Classic, Slow bloom, Straight in, Breather, Drift, equally likely; skeleton fixed, small moves rolled with Motion (20–80). | genre shapes **(Sam)** | **Done** (`LIB_MAPS`, `plan.c`, R9, R11, R12) | *audio* §4, §7 |
| Layers in and out on 4/8-bar boundaries | At most 2 roles per boundary; map-mandated entries exempt. | genre | **Done** (`plan.c: moves`, R9) | *genre* §1.4 |
| Intro | Keys alone, softer and filtered, 4–8 bars by map (Straight in: none). | genre | **Done** (`PB_DARK`) | *audio* §4.2 |
| Middle event | Classic: one drum break of 4 or 8 bars at 43–65% through (4 bars twice as often), the bass kept in half of them. Breather: one 8-bar block with kick and bass out. Straight in: one hat drop-out. Drift: drums in at most 60% of the middle. 1-bar drum drops: 40% × Motion of eligible 8-bar blocks. | genre | **Done** (`plan.c: skeleton`) | *audio* §4.4; *midi* §0 |
| Outro | Classic and Breather: keys alone, 4–8 bars. Slow bloom and Drift: fade. Straight in: ring out. | genre | **Done** (`PB_FADE`) | *audio* §4.6 |
| Novelty between songs | A new roll differs in at least 5 main choices; tempo by 6 BPM or more. | product + taste **(Sam)** | **Done** (`roll.c: novelty`, R7) | *genre* §8.1 |
| DJ key flow | The next song a 4th or 5th away, or the relative key when the mode family flips. | genre | **Done** (`roll.c: draw_key`, R7) | spec |
| Variation speed | Slow, per pass or block, never per bar. | genre | **Not yet** (fixed per song) | *accomp* §1, §8.4 |

## Harmony

| Choice | Rule → numbers | Kind | Status | Source |
| --- | --- | --- | --- | --- |
| Mood steers the mode | Weights by mood (only modes with a legal progression count). Sad: Aeolian 50%, Dorian 33%, Ionian 8%, Mixolydian 8%. Wistful: Dorian 38%, Aeolian 38%, Mixolydian 15%, Ionian 8%. Warm: Ionian 36%, Mixolydian 36%, Dorian 14%, Lydian 7%, Aeolian 7%. Sunny: Ionian 43%, Lydian 29%, Mixolydian 21%, Dorian 7%. Phrygian never; Locrian not in the engine. Moods equally likely. | genre + taste **(Sam)** | **Done** (`LIB_MOODS`, `roll.c`) | *genre* §2.3 |
| Mood steers the color | 7 / 9 / rich. Sad 10 / 40 / 50%. Wistful 20 / 50 / 30%. Warm and sunny 30 / 50 / 20%. Every chord is at least a 7th; dominants at least a 9th. | genre (jazz-sample lineage) | **Done** (`LIB_MOODS`, `theory.c`) | *genre* §2.1 |
| Progression library | 44 progressions, all approved: 15 vamps, 18 loops, 11 jazzy. The roll weighs the kinds 40 / 45 / 15, each kind's weight spread evenly over the entries legal for the song's mood and mode. Cycles of 2, 4 or 8 bars, 1 or 2 bars a chord. | genre **(Sam: curated)**; mix (call) | **Done** (`patterns_gen.c: LIB_PROGS`, `roll.c`) | *genre* §2.1–2.2; R6, R8, R26 |
| **No substitutions, no tritone subs, anywhere** | Reharmonizing gives one chord of a later pass a richer color, with a per-song chance of 25–50% a pass (songs below rich only). Never a new root, 3rd or 7th. The importer refuses a dominant on bII or bVI, or a dominant a half step above the next chord. | taste **(Sam)** | **Done** (`harmony.c: spec_at`; `tools/patterns/import.py`) | 167b256, aa0ce3f |
| Chords stay in the mode | A vamp's or loop's chords lie in each mode it lists, at every color. Only jazzy entries borrow (a forced quality or an altered root, for example VI7, III7, iv7 and I7 in major, V7 in minor, bVII7 and bVImaj7 in Dorian, bIImaj7). | taste (stay in key) | **Done** (`lib_prog_t.modes`, R4, R25, R26) | 1aeaa20 |
| Dominants never bare | At least a 9th; the 13th where the 9th is an avoid note or in rich songs. No clean extension: the chord drops its 7th to a major triad. The importer refuses a progression with a dominant that would end up bare. | jazz practice as a hard rule **(Sam)** | **Done** (`harmony.c: dominant_colors`) | cd9e5aa |
| Darker intro and outro | The same chords and voicings, 6 velocity softer, low-passed by the section filter. | genre (filtered intro) | **Done** | *audio* §4.2 |

## Comping (the keys' rhythm)

One style per song (seed stream 121), weighted by the set's keys instrument. The workbench can force one.

| Choice | Rule → numbers | Kind | Status | Source |
| --- | --- | --- | --- | --- |
| Electric and synth keys (Rhodes, Wurli, felt piano, DX EP, organ, pads) | Held 55%, half notes 45% (weights 44 / 36). Stabs, offbeats and the arpeggio: 0. Half notes play as held over sustained keys (pad, vox pad, organ) and over a kick on exactly beats 1 and 3 (the brushes pattern). | genre; **Sam**: stabs "just not working" on synth keys, the arpeggio "sounds very synthetic" (2026-10-28) | **Done** (`harmony.c: style_draw`) | 3025034, fe0b0f1 |
| The Knight (acoustic piano) | Held 41%, stabs 29%, half notes 29% (weights 35 / 25 / 25). No arpeggio. | **Sam**: stabs return on sampled instruments | **Done** | 9b51427 |
| Guitars (nylon, Emilyguitar), 84 BPM and up | Strum 35%, held 24%, stabs 24%, half notes 18% (weights 30 / 20 / 20 / 15). Under a struck lead the strum's weight is 0: held 36%, stabs 36%, half 27%. Never an arpeggio. | **Sam**: strum only under a sustained lead; no guitar arpeggio | **Done** | 30aec14, cf0dca0 |
| Guitars under 84 BPM | Half notes 41%, strum 35%, stabs 24% (weights 35 / 30 / 20); never held (too sparse at slow tempos). Under a struck lead: half notes 64%, stabs 36%. | **Sam** | **Done** | 9b51427 |
| Held | One strike on beat 1 of each chord, held to the chord change (less 10 ms). Velocity 40–58, the lowest note +3. A held chord of 2+ bars restrikes each bar (a little softer, at most 54) in a drum entry with snare but no kick, no bass and no lead, over struck keys. | genre (sampled-loop feel); soft touch **(Sam)** | **Done** (`COMP_HELD`) | 1498185, 1dee3a8 |
| Half notes | Strikes on beats 1 and 3, each held to the next, the second at 90% velocity. Velocity 40–58. The chord keeps one voicing for both strikes. | **Sam**: "they could also do half notes" | **Done** (`COMP_HALF`) | fe0b0f1 |
| Stabs | Beat 1 and one more strike, 60% of a quarter long, velocity 60–85. The second answers the kick: over a kick of 3 hits a bar or fewer, the first of steps 6, 10, 4, 12 the kick leaves free; over a busier kick, step 6. | **Sam**: keys and kick share a beat only when both aren't sparse | **Done** (`COMP_STABS`) | 9b51427 |
| Strum | One of 4 approved patterns, equally likely: D-DU-UDU (`D...d.u...U.d.u.`), chucks on 2 and 4 (`D.u.X.U.d.u.X.U.`), the bossa (two bars: thumb and pinches), the ballad (three strokes a bar; slow in half its songs). Up strokes only on the "and"s. Velocities: accented down 76, down 64, up 58, soft up 50, chuck 62 (60 ms), thumb 66, pinch 54, ±4. An up stroke fades 4% a string. The hand is 0–3 ms late. In a fill bar the ballad moves its up stroke to the "and" of 2. | **Sam** (instrument spike: quick strums, slow ones on the ballad) | **Done** (`strum.c`, `STRUM_PAT`) | ee807a4 |
| Room for the lead | A busy lead (4+ onsets a bar): stabs on beat 1 only, half notes on beat 1 held a bar. Percussive keys voice under the lead's band. The keys duck 3 dB while the lead sounds. Details in [general.md §7](general.md#7-register-and-mix-the-accompaniment). | **Sam**: "the lead leads" | **Done** | 54e2723 |
| Section touch | Intro and outro 6 velocity softer; a block with neither kick nor lead (the keys carry it) 6 firmer. | genre | **Done** (`harmony.c: emit_chord`) | e6e21af |
| Keys register by set | The Chillwave pad set an octave lower. Sustained keys below, with or above the lead (a third each); with or above, an octave up. | **Sam** | **Done** | 197b269 |
| **Removed:** the 16th arpeggio, offbeats | The arpeggio (one chord tone a 16th, velocity 50–70) is off on every instrument; offbeats (the "and"s) retired. Both stay in the code at weight 0. | **Sam** (2026-10-28: "may bring it back later") | **Removed** | 3025034 |
| **Not yet:** the piano broken-chord climb | A rising broken chord over 2–3 octaves, meant to take the rolled-chord role on the Knight. Parked. | **Sam** (2026-10-27: "save the piano climb for later") | **Not yet** | ledger |

## Bass

| Choice | Rule → numbers | Kind | Status | Source |
| --- | --- | --- | --- | --- |
| Styles | Roots on the kick; pedal (a root per chord change, held); walk. No root-fifth line (Sam, 2026-11-04: "cheesy bass lines that go from 1 to 5"): its songs play roots on the kick. Rules in [general.md §3](general.md#3-bass-lines). | genre | **Done** (`groove.c`) | *genre* §4.1; R13, R14, R21 |
| Which style | Drawn per song (stream 112). Drift map or sad mood: pedal 50%. Else, in a walking set: walk 35%. Else roots. So: a sad song in a walking set is pedal 50%, walk 17.5%, roots 32.5%; a happy one walk 35%, roots 65%; any other set roots (pedal 50% first when sad or Drift). | genre; (call) | **Done** (`groove.c: bass_style_of`) | R13 |
| Walking sets | Sets 1 (Rhodes dusty), 4 (Felt and vibes), 8 (Wurli and vibes), 9 (Nylon and flute): the four that had the upright. | **Sam** (upright removed, 2026-10-24) | **Done** (`library.c: lib_set_walks`) | 408a271 |
| Where the walk plays | One 8-bar window in four: bars where (bar / 8) mod 4 equals a per-song draw. It recurs every 32 bars (up to 3 windows in a 76-bar song). Roots on the kick elsewhere. | genre; R14 accepted | **Done** (`groove.c: bass_style_at`) | R13, R14 |
| **Diatonic only** | The walk's approach is a scale step, never chromatic. | taste **(Sam)**: nothing out of the key | **Done** (`groove.c: approach`) | 8dde786 |
| Register | E1–B2, the tonic in A1–Ab2. | genre | **Done** (`groove.c: bass_place`) | *genre* §4.1 |
| Bass rhythm spike | Rhythm cells, anticipations and ghost notes for roots songs. Host only (`VNL_BASS`), parked by Sam. | (spike) | **Not yet** | ledger |
| **Removed:** the upright | The Karplus-Strong upright bass. | **Sam**: "I don't like the upright's sound" | **Removed** | 408a271 |

## Drums

### Patterns

8 approved patterns (`patterns_gen.c: LIB_PATTERNS`, from `patterns/drums`). The roll draws one per song, **all
equally likely, whatever the set, mood or kit** (12.5% each). All are one bar, repeated. Hits listed by beat
(1 e & a); ghosts in parentheses.

| # | Family | Kick | Snare | Hats | Other |
| --- | --- | --- | --- | --- | --- |
| 1 | boom-bap | 1, &2, &3, &4 (&1) | 2, 4 (a1, &4) | 8ths (16ths) | — |
| 2 | boom-bap | 1, a2, 3, &3, a4 | 2, 4 (a2) | 8ths (16ths) | open hat a4 |
| 3 | boom-bap | 1, a1, &3 (e2) | 2, 4 (a1, a3) | 8ths | open hat a4; shaker 8ths (16ths) |
| 4 | half-time | 1, &2 (&3) | 3 (4) | 8ths (16ths) | — |
| 5 | half-time | 1, 2, 4 (&3) | 3 | quarters (the "and"s) | open hat a4; shaker every third 16th (3 against 4) |
| 6 | sparse | 1, &2 | 3 | quarters (the "and"s) | rim on 4 |
| 7 | brushes | 1, 3 (&3) | 2, 4 (&1, 3) | 8ths (16ths) | — |
| 8 | bossa | 1, a1, 3, a3 | 4 (2) | 8ths (16ths) | rim 1, a1, &2, &3, 4 (clave) |

| Choice | Rule → numbers | Kind | Status | Source |
| --- | --- | --- | --- | --- |
| Pattern choice | Equal weights over the approved patterns. Novelty: the next song's pattern family must differ when the novelty passes need it. | genre; (call) | **Done** (`roll.c`) | *genre* §3.1; *midi* §3.4 |
| Velocities and timing | Numbers in [general.md §4](general.md#4-rhythm-and-humanization). | genre | **Done** | — |
| Sampled hats at light swing | With a sampled kit and swing under 55%, the hats and shaker drop their odd 16ths (8ths only). | **Sam** (set 26) | **Done** (`groove.c`) | cf0dca0 |
| Fills | Only at the last bar of a block whose next block changes kick, snare or bass: snare 16ths on beat 4 (velocity 60 to 90), a kick on the "and" of 4. No crashes, no tom fills, no rolls into drops. | genre + taste (nothing startling) | **Partial** | R13 |
| 1-bar drops | The last bar of an 8-bar block with no layer change, 40% × Motion of the eligible blocks; never in Slow bloom or Straight in. | genre | **Done** (`plan.c`) | *audio* §4.4 |
| Polymeter | A 3-against-4 shaker in pattern 5. | **Sam** (approved) | **Done** (`poly3`) | spec |
| Kick phrases | Sam's two-bar kick phrases k2–k6 (2026-10-28) exist in the workbench only; no engine pattern is two-bar. | **Sam**: "k2-k6 could all work" | **Not yet** in the engine | ledger |

### Kits

| Choice | Rule → numbers | Kind | Status | Source |
| --- | --- | --- | --- | --- |
| The song's kit | Each set names a synth kit (Dusty, Soft, 808 or Brushes) and a sampled kit. A song plays one or the other, 50 / 50 (stream 192). Sets with no sampled kit (the 808 sets 5 and 10, the Brushes sets 2, 9, 17, 29, 32) always play their synth kit. | (call) | **Done** (`roll.c: lib_song_kit`) | 81efce6 |
| The sampled kits | One pool of hits, processed per kit. Warm: Gogodze kick and closed hat, the sidestick backbeat, the ride; down 1 semitone, low-passed at 3.2 kHz. Dark: Gogodze kick, its snare and hats down 3 semitones and low-passed. Rusty: Big Rusty kick, Frankensnare poplar, Big Rusty foot chick. Gentle: Big Rusty kick, VCSL rope snare (snares off), VCSL pedal hats. Clean: Gogodze kick, Frankensnare poplar, VCSL closed hats. | **Sam** (drum lineups, 2026-10-25) | **Done** (`app/engine/skits.c`) | f61712c |
| The ride | Sampled-kit songs only. The Warm kit rides throughout. Other sampled kits: throughout 15%, one block 50% (drawn evenly from the lead blocks, the blocks where the bass walks and the last groove block), never 35%. In a ride bar the hats move to the ride on the beat only (quarter notes at most); off-beat hats drop; open hats become ride accents, +14 velocity. Synth kits never ride. | **Sam**: the ride never plays 8ths | **Done** (`groove.c: groove_ride`; `song.c`) | 7773f9d, a162c90 |
| **Families: drums only** | Ledger (Sam, 2026-10-29): the acoustic/synth divide applies to drums only; keys, bass, pad and lead may mix across families, and a song's family chooses its drum kit. The engine has no family: the kit is the set's, as above. | **Sam** | **Not yet** (see Open questions) | ledger |
| Kick ducking | Subtle 1, medium 3 or obvious 6 dB, a third of songs each. | genre; **Sam**: yes | **Done** (Task 7b) | *genre* §5 |

## Pad layer

A quiet background on its own bus (V_PAD): a **chord part** or a **solo note**, one or the other in a section, never
both. Strings under most sets; the warm synth pad under sets 13, 18, 19, 20, 25 and 30–33. `pads.c`, `song.c:
pads_group`.

| Choice | Rule → numbers | Kind | Status | Source |
| --- | --- | --- | --- | --- |
| In the song | About 2 songs in 3 have a pad layer (67%, stream 195). The workbench can force it on or off. | **Sam** (2026-10-29: "make it possible for the pad part to not be in a song") | **Done** (`roll.c: song_pad_on`) | b4e00ff |
| In a section | Only where the keys play. Chance in by section: intro 45%, groove 75%, under the lead 85%, break 40%, outro 60%. Never out for more than 2 blocks in a row: the third is in. | **Sam** (2026-10-29: "drop out of certain song sections") | **Done** (`pads.c: song_pad_in`) | b4e00ff |
| Fades | Over a section line, linear over one bar: out across the last bar before a block without the pad, in across the first bar back. | **Sam** | **Done** (`song.c: pads_group`, `song_pad_level`) | b4e00ff |
| The solo voice | No solo note in 40% of songs. Otherwise the choir (vox pad), the flute, a soft organ or the strings, equally likely; never the flute under a flute lead, nor the organ over organ keys. | **Sam** (revision 2); the 40% (call) | **Done** (`pads.c: pads_solo_voice`) | b6301cd |
| Chord part or solo, per section | Over struck keys: the chord part, or the solo note in 35% of intros, 25% of grooves, 15% under the lead, 35% of breaks, 30% of outros. Over sustained keys (pad, vox pad, organ): the solo note in 50% of intros, 60% of grooves, 30% under the lead, 50% of breaks and outros; otherwise nothing (no chord part over a pad). A song with no solo voice never takes the solo; over sustained keys it then has no pad at all. | **Sam** (revision 2) | **Done** (`pads.c: pads_section_part`) | b6301cd |
| The chord part | 2 notes, chosen at each chord change: the 3rd and the 7th first (a missing 3rd costs 60, a missing 7th 40; a 5th or 9th 20, another color 40). G3–C5 (55–72). Never the root. No note a semitone (or minor 9th) from a keys note, a lead note (of the 4-bar group's first bar) or its partner, none a minor 2nd over the bass's root, no doubled pitch class. Voice-led: common tones hold, the top voice costs 8 a semitone, a glide over 4 semitones restrikes instead. The first chord's top near G4. | **Sam** ("a mid-range chord throughout"); 2 notes for the CPU budget | **Done** (`pads.c: pads_chord`) | pads design |
| The solo note | One note a bar on beat 1, held while the note stays: the scale's 1st, 3rd, 2nd or 7th (ties in that order). C5–C6 (72–84); the strings C6–C7. At least 3 semitones over the chord part's top. No avoid note, no rub with the keys, none a minor 2nd over the bar's bass notes (nor a major 7th over a non-root bass). Kept while legal, else the smallest move. Not checked against the lead (to revisit). | **Sam** ("often tonic or near it") | **Done** (`pads.c: pads_solo`) | pads design |
| Levels | The chord part at velocity 29 (about 8.5 dB under the keys), the solo note at 58, both 8 softer in intros and outros. The layer dips 1 dB under the lead. | **Sam** ("a bit loud", 3 dB down) | **Done** (`song.c: pads_step`) | 49703b0, 7cd158c |
| A/B switch | `PADS=double` (host renders): the chord part doubles the keys' voicing instead. | **Sam** | **Done** (host only) | pads design |

## Melody and ear candy (not refreshed: the lead is being rewritten)

The 2026-10-24 rows here described the struck melody, the sustained lead's phrase planner and the ear candy.
**Removed:** the ear candy (b4e00ff, Sam: "Remove the ear candy part completely"), and the phrase planner, the hybrid
lead and the struck melody (lead styles Task 6: every song plays a lead style, `app/song/leads/`). Document the lead
styles when that build lands.

## Sound: instruments, dust and bed (not refreshed on 2026-10-30)

These sections are as of 2026-10-24. Known to be stale: the ear candy (removed), the upright (removed), the lead
instruments (changing with the lead rewrite; see the ledger's "Leads"), the sets (now 29, ids 1–11,
13, 17–33), the sampled instruments (the Knight, the sampled nylon, the Emilyguitar) and the kits (above).

| Choice | Value | Kind | Status | Source |
| --- | --- | --- | --- | --- |
| Roles | chords, drums, bass, lead, the pad layer, bed. **Removed:** ear candy. | genre | **Done** (`voices.c`, `drums.c`, `fx.c`) | spec |
| Curated instrument sets | Not refreshed: 29 sets in `library.c: LIB_SETS`, each naming keys, bass, lead, pad, a synth kit and a sampled kit. Sets, not free mixing **(Sam)**. | genre **(Sam)** | **Done** | *genre* §4 |
| Background pad layer | Moved to [Pad layer](#pad-layer) above and refreshed. | — | — | — |
| Dust | low-pass about 1.5–5 kHz, wow and flutter, soft saturation, bit/rate grit; warm vinyl or wobbly tape | genre | **Done** (`fx.c`, four levels) | *genre* §5 |
| Surface noise, ambience | vinyl crackle or tape hiss or none; rain or none; steady, opens and closes the song | genre **(Sam: three independent layers; quieter, Task 8)** | **Done** (`fx.c` bed, about 35 dB under the music) | *genre* §5; MAKU, LOF-1 |
| Automation arcs, evolving weather, noise swell before drums | slow filter / wobble / reverb arcs over 16–32 bars; rain builds and passes | neighboring genres, approved **(Sam)** | **Partial**: section filter moves and rain gusts; no long arcs yet | spec |
| Self-sampling chops, vocal chops, reverb freeze | later features | neighboring genres **(Sam: later)** | **Not yet** | spec |

### The instrument models (Plan 3a; `patch.c` documents every parameter)

- **Keys:** Rhodes, Wurli and felt piano are one 2-op FM model with a decaying index (the tine's bell into a round
  body), a strike, a noise bark (Wurli) and a tone low-pass (felt). Nylon is a Karplus–Strong pluck.
- **Pads:** two detuned polyBLEP saws into a low-pass (pad) or AMB-1's two formant filters (vox pad).
- **The pad layer (M_ENS):** a polyBLEP saw and a partner a few cents sharp into a fixed 2-pole low-pass, per lane
  (lanes 1–3 carry the chord part's 2 notes, 4 the solo note; a held lane glides to its next note), at a quarter of
  the rate with its bus's filters, through the chorus at its own depth and the plate. The solo lane takes the song's
  rolled voice instead when it has one (revision 2): the choir and the organ at a quarter of the rate beside the
  ensemble, the flute at half, each trimmed to the strings' loudness.
- **Basses:** sub and electric bass are FM with a struck index plus a sub-octave sine (legato retriggers); the
  upright is a Karplus–Strong pluck with a thump.
- **Bells:** AMB-1's 2-op FM bell (vibes with the motor's tremolo, kalimba, music box, bells), the decay tracking the key.
- **Winds:** flute, sax and muted trumpet blend sine, triangle and pulse into a low-pass that opens with the
  envelope, with a mute, breath noise, a chiff and a delayed vibrato. The guitar lick is a Karplus–Strong pluck.
- **Drums:** four kits (dusty, soft, 808, brushes), each lane a swept sine or filtered noise.
- Keys and pads whose partials stay under 9 kHz render at half the rate (Task 7c, a cost move; inaudible by test).

### The synths (synths plan, 2026-10-23; Sam's cut, 2026-10-24)

Six synths, every role covered but the ear candy, all warm by default (low-passed or band-limited; the dust still
colors them). Sam kept these six after hearing the first renders; the Juno, the Moog bass, the square and sine leads,
the chip chords, lead and blip, and the FM bell are gone.

- **Vintage analog (M_SUB):** frugal as a Juno-106: one oscillator (saw or pulse) and a sub-octave square into a
  2-pole low-pass with its own envelope, mono with glide on the lead. A note glides from a held note, or from one that
  ended under 25 ms before it when it is a perfect 4th or less away (the 2026-10-23 ruling: the lines rarely overlap,
  so held-only glides almost never sounded; over rolled songs of the saw-lead sets 13, 18 and 19 about a fifth to a
  half of the lead's note changes glide). A note after a longer rest, or a wider leap, starts clean.
  - **Organ** (keys): a square and its sub (16' and 8'), no envelope, a steady drawbar tone.
  - **Saw lead:** mono, a 90 ms glide, a singing line.
  - **Synth pluck** (lead): a pulse snapped open and shut by its filter envelope.
- **80s FM (the existing FM models, new patches):**
  - **DX electric piano** (keys): the 14x tine on the attack over a clean, faster-decaying body.
  - **FM slap bass:** the index strikes high and is gone in 150 ms.
- **Chiptune (M_CHIP):** the **triangle bass**, a NES-style triangle at the full rate with no filter, stepped to 16
  levels, on the semitone. A 7-bit crush over each note's own level gives the grit and steppy decays.

**The sets** (each rolls about 6% of songs; any synth instrument in about 30%):

| Set | Keys | Bass | Lead | Ear candy | Kit | Flavor | Moods |
| --- | --- | --- | --- | --- | --- | --- | --- |
| DX keys and FM bass | DX EP | FM slap bass | saw lead | vibes | soft | city pop | warm, sunny |
| Organ and saw lead | organ | triangle bass | saw lead | music box | soft | neo-soul | sad, wistful |
| Rhodes and saw lead | Rhodes | FM slap bass | saw lead | none | dusty | lo-fi | sad, wistful |
| DX and pluck | DX EP | triangle bass | synth pluck | kalimba | soft | chillwave | sad, warm |
| Nylon and synth pluck | nylon | sub | synth pluck | kalimba | brushes | bossa | wistful, sunny |

**Character:** DX keys and FM bass the bright 80s city pop; the organ over the triangle bass the churchy neo-soul,
with a game-soundtrack bottom; DX and pluck the hazy chillwave one; Rhodes and saw lead and nylon and pluck mix a
synth into the classic sets. The first, second and fourth are all synth in the keys, bass and lead, so every mood has
one. Every set keeps the lead 8–10 dB over its keys while it plays (Sam, 2026-10-24, twice: the comping quieter; was 4–6), the keys ducking 3 dB under it; the keys 4 dB down in all.

### The chain, in order (`voices.c`, `fx.c`)

1. **Voices into role buses** (chords, bass, lead, candy, drums). The keys' level follows the song's comping.
2. **The mud high-pass** on the chords', the lead's and the ear candy's sums: a Butterworth at 135 Hz (150 Hz on a
   pad), so the bass and the kick own the low end **(Sam: "add both")**.
3. **The drums' compressor**: 4:1, instant attack, 120 ms release, a threshold per kit (3–6 dB off a full-velocity
   kick or snare in every kit).
4. **Section filters** per bus: the plan's moves (a low-pass from 18 kHz to 200 Hz, a high-pass build to 600 Hz).
5. **Sends:** reverb (Rings', Sam's pick; keys 0.30, lead 0.35, candy 0.45, drums 0.06, bass dry), the chords' bus into
   the chorus (Rings'; Rhodes deepest), a tape delay (a dotted 8th; leads and candy wet, keys a touch, basses dry).
6. **Dust** on the music: wow and flutter, saturation, grit, the dust low-pass.
7. **The pump** (the kick ducking, per song) and **the glue compressor** (below), as one gain.
8. **The soft clip** (linear to 0.4, a knee to 0.7), then the wet returns over it, then **the bed**.

### The mix (Task 8)

- **Balance:** the keys and the lead are the focus; the bass sits about 1–5 dB under them, the drums 0–6 dB under
  (the 808 about level with the keys), measured as the 90th percentile of 50 ms levels in full-band sections; the
  ear candy an ornament under the lead; the bed about 35 dB under the music. The upright sits lower in body (6–15
  dB under the keys): its pluck's peak, near the soft clip, caps it. Role gains set the balance; each patch's level evens a role's instruments
  out (nylon up, pad and felt down, the basses brought together, the leads together), so every set mixes alike.
- **The keys per comping:** struck keys held a bar are lifted 3.75 dB, held two bars 4.5 dB (nylon 2 dB more: it
  decays fastest), half notes 0.75 dB (they restrike; half notes that hold sit as held), an arpeggio −0.25 dB; a pad's
  arpeggio 3 dB (pads revision 2, retuned with half notes in). Without it, held songs sat 3–6 dB under stab songs.
- **The drums:** within each kit the hats sit about 10 dB under the snare per hit after the chain's low-pass, the
  open hat 5, the shaker 10, the rim 8; each kit has its own level (the 808's sub a little hotter).
- **The glue compressor (Sam: "yes"):** on the mix bus, 2:1 above −26 dBFS with a 4 dB knee, 30 ms attack, an auto
  release (200 ms after a short peak, 400 ms while it keeps compressing), 4.5 dB of make-up (the mix's level, the
  reverb's return included). Over 50 songs it takes 1.1–3.1 dB off full sections and under 1 dB off intros.
- **Steady levels:** over 50 rolled songs every song's full band sits within 3 dB of the median, and no section
  jumps more than 6 dB from the last full one (intros, outros, breaks and fades may be quieter).
- **The bed (Sam: "the noise layer needs to be quieter"):** 33–38 dB under each song's music (it was 22–29).
- **Pads at a chord change:** a held pad gets 8 chord voices and a 2 s release, so the old chord releases under the
  new one's swell (a 2.8 dB dip, was 17.6 dB); a pad's swell takes at most half its note, so a pad arpeggio speaks.
- The choices to judge by ear are in the song-engine ledger's "Sound" list.


## Excluded on purpose (genre or taste)

- EDM drops and risers, dub throws, trap hat rolls, abrupt beat switches (spec).
- Record skips, backspins, stutters, tape-stop endings: none in the measured tracks, and startling **(call)**. (The
  lead loop's "stuck-note stutter" is a planned exception, ledger "To revisit".)
- Chord substitutions, tritone subs, borrowed chords outside "jazzy" entries **(Sam)**.
- Chromatic approach tones in the bass **(Sam)** and in the melody (R25).
- 8th notes on the ride **(Sam)**.
- Two-movement 5-minute songs (an early research proposal): replaced by 2–3.5 minute songs **(Sam)**.

## Open questions for Sam

Cases where the code and the ledger disagree. Nothing here was changed. (The voicing questions, rootless and the
keys' semitone check, are in [general.md](general.md#open-questions-for-sam).)

1. **Families and kits.** The ledger (2026-10-29) says the acoustic/synth divide applies to drums only, and a
   song's family picks its kit. The engine has no family: each song plays its set's synth kit or its set's sampled
   kit, 50 / 50. It still draws the kits the ledger dropped on 2026-10-28 (the Dusty, Soft and 808 synth kits: "None
   of our 3 kits should remain"; the five mixed sampled kits, retired in the workbench). The approved kits (poketrack
   808 and CR-78, Optimist LO-FI, Gogodze, Big Rusty sticks and brushes, Brushes) exist only in the workbench. Bring
   the approved kits and the family rule into the engine?
2. **The workbench still makes bass and pad follow the family.** `workbench/options.json` lists `"follow": ["bass",
   "pad", "kit"]`, the 2026-10-28 rule. The 2026-10-29 ruling says drums only. Which is right?
3. **Comping mix on electric and synth keys.** With the arpeggio off (2026-10-28), the draw is held 55% and half
   notes 45%. The ledger's last stated mix is held 55 / half 25 / arpeggio 20, and "Approved by ear" still lists
   held 55 / stabs 20 / offbeats 15 / arpeggio 10. Is about 45% half notes what you want?
4. **Settled (Sam, 2026-10-30): they do now (fixed).** Was: **Strummed guitars and sustained keys don't make room for the lead.** You asked "why does it matter what the
   nylon keys are doing?" and decided "duck the keys instead" (2026-10-29). The keys' window applies to percussive
   keys only: a strummed guitar shape keeps its place (top near E4, up to E5) and so do pad and organ keys, so there
   the lead still moves up over them. Should strums (and sustained keys) voice under the lead too?
5. **Drum patterns ignore the set and the family.** Any of the 8 patterns plays with any set and kit, equally
   likely (a bossa pattern under a synth set, the brushes pattern with the 808). The ledger left open "should the
   drum patterns split by family too". Your two-bar kick phrases (k2–k6, 2026-10-28) are workbench-only; no engine
   pattern is two-bar. Tie patterns to family or flavor, and bring the kick phrases in?

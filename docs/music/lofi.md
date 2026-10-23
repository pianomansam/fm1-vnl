# Lo-fi hip hop and Sam's taste

Everything here would change if VNL-1 played another genre, or if Sam's ear said otherwise. Genre-independent
musicianship is in [general.md](general.md). Each entry says whether it comes from the **genre** (measured or
sourced in `docs/research/`) or from **taste** (Sam's call, usually after listening), with its status.

Research short names: *genre* = `lofi-hip-hop.md`, *audio* = `lofi-audio-arrangements.md` (19 tracks), *midi* =
`lofi-midi-arrangements.md` (147 files), *accomp* = `auto-accompaniment-comparison.md`, *firmwares* =
`other-firmwares.md`, *amb* = `amb-1-and-vnl-1.md`.

## Purpose: soothing, never pulling attention

Sam listens while studying and coding **(Sam)**. This purpose drives several choices below that a jazz or pop
engine would not make: zero tolerance for semitone rubs, no substitutions, diatonic bass, no startling transitions
(record skips, backspins, stutters, tape stops excluded), true silence of 2–4 s between songs. Status: **Done**
(clash checks, `app/app.c` gap), except level checks between sections (planned).

## Length, tempo, feel

| Choice | Value | Kind | Status | Source |
| --- | --- | --- | --- | --- |
| Song length | 2–3.5 min, 36–76 bars in multiples of 4 | genre | **Done** (`roll.c`, `RF_BARS`, `roll_seconds`) | *genre* §1.1 (median 2:26, n = 117); *audio* (median 158 s); R3 **(Sam)** |
| Tempo | 70–90 BPM | genre | **Done** (`roll.c`, `RF_TEMPO`) | *genre* §1.2; *audio* median 85 |
| Meter | 4/4 only; triplet feel through swing | genre | **Done** | *genre* §1.3 |
| Swing | 52–62% on 16ths, mean about 55.5% | genre | **Done** (`roll.c`, `RF_SWING` 520–620; `groove.c`) | *genre* §3.2; *midi* §5 (0.54–0.60) |
| Lateness | snare 10–30 ms late, hats up to about 70 ms; kick on the grid | genre (Dilla lineage) | **Done** (`groove.c`, R13) | *genre* §3.3 |
| Lead swing | lead and candy straight, only late | (call), to hear | **Done** as R16; reversal proposed | R16; *accomp* §6.3 |
| Lead phrase starts | how syncopated: proposed 15% beat 1, 25% other beats, 35% 8th offbeats, 10% 16ths, 15% pickups | genre amount (the need to vary is general) | **Not yet** (ledger "To revisit") | ledger; Impro-Visor grammars |

## Form

| Choice | Value | Kind | Status | Source |
| --- | --- | --- | --- | --- |
| One chord loop per song | 2–4 chords, cycle 2, 4 or 8 bars; no B section, no key change in the plan | genre | **Done** (`library.c: LIB_PROGS`, R5) | *audio* §1 ("no track has a B section") **(Sam)** |
| Song maps | Classic, Slow bloom, Straight in, Breather, Drift: skeleton fixed, small moves rolled with Motion | genre shapes, curated **(Sam)** | **Done** (`library.c: LIB_MAPS`, `plan.c`, R9, R11, R12) | *audio* §4, §7; *midi* §5 |
| Layers in and out on 4/8-bar boundaries | at most 1–2 roles per boundary; map-mandated entries exempt | genre (subtractive arrangement) | **Done** (`plan.c: moves`, R9) | *genre* §1.4; *audio* §1; *midi* §0 |
| Intro | keys alone, darker, about 4 bars (0–8); bed alone 1–3 bars | genre | **Done** (`PB_DARK` marks it: softer now, filtered in Plan 3; same voicings as the song); bed **Not yet** | *audio* §4.2 |
| Drum entry | drums and bass together on a 4-bar line; hats 4–8 bars later | genre | **Done** | *audio* §4.3 |
| Middle event | hats out 4–8; kick and bass out 8; one drum break 4–8 bars at 43–65% through; 1-bar drops on an 8-bar phrase's last bar; ~40% of tracks never drop drums | genre | **Done** (`plan.c: skeleton`, `PB_DROP_LAST`) | *audio* §4.4; *midi* §0 |
| Outro | drums leave 4–10 bars early, keys alone mirror the intro; ring out or fade | genre | **Done** (`PB_FADE`; fades sometimes on a 4-bar block, deferred) | *audio* §4.6 |
| Steady state | drums + bass + keys; lead some blocks only | genre | **Done** | *audio* §1 (68% of bars) |
| Novelty between songs | a new roll differs in at least half of the main choices; tempo by ≥ 6 BPM; dice never return the current value | product + taste **(Sam)** | **Done** (`roll.c: novelty`, R7) | *genre* §8.1 |
| DJ key flow | next song in a related key (relative or circle-of-fifths neighbor), nearby tempo | genre (playlist feel) | **Done** (`roll.c: draw_key`, R7) | spec neighboring genres |
| Variation speed | slow: per loop pass or block, scaled by Motion, never per bar (the loop should feel sampled) | genre | **Not yet** (fixed per song today) | *accomp* §1, §8.4 |

## Harmony

| Choice | Value | Kind | Status | Source |
| --- | --- | --- | --- | --- |
| Vocabulary | every chord at least a 7th; colors 7 / 9 / rich (13 dom, 11 min7, #11 Lydian maj7) | genre (jazz-sample lineage) | **Done** (`theory.c: th_chord_pcs`, `RF_COLOR`) | *genre* §2.1 |
| Dominants never bare | a dominant always carries a 9th (13th in rich songs or when the 9th clashes); if no clean extension fits, the chord drops its 7th to a triad | jazz practice made a hard rule **(Sam)** after hearing a bare Eb7 | **Done** (`harmony.c: dominant_colors`); 5,692 bare dominants → 0 | cd9e5aa |
| **No substitutions, no tritone subs, anywhere** | reharmonize-on-repeat only gives one chord of a later pass a richer color; never changes root, 3rd or 7th; no progression has a written tritone sub | taste: stay in key, soothing **(Sam)** after hearing a tritone-sub D7 in Db Lydian and a borrowed Bm7 in Gb major as wrong | **Done** (`harmony.c: spec_at`; progressions 15 and 23 rewritten) | 167b256, aa0ce3f |
| Chords stay in the mode | vamps and loops list only modes where every chord, at every color, lies in the scale; borrowing only in "jazzy" entries, and the melody follows the chord-scale there | taste (stay in key) | **Done** (`lib_prog_t.modes`, R4, R25, R26) | 1aeaa20 |
| Progression library | classic lo-fi shapes as roman numerals: vamp / loop / jazzy (one twist chord), mix about 40 / 45 / 15; 18 new entries (27–44) await curation by ear | genre **(Sam: curated)**; mix (call) | **Done** (`library.c: LIB_PROGS`, R6, R8, R26) | *genre* §2.1–2.2 |
| Mood steers mode and color | sad ↔ happy: minor/Dorian ↔ major/Lydian; Phrygian and Locrian never | genre + taste **(Sam)** | **Done** (`library.c: LIB_MOODS`, `roll.c`) | *genre* §2.3, §8.1 |
| Chord rhythm | one comp style per song: held, stabs, offbeats, arpeggio; rolled 10–60 ms | genre (sampled-loop feel) | **Done** (`harmony.c: harmony_style`, R15); no anticipations | *genre* §2.4; LOF-1 (*firmwares*) |
| Darker intro and outro | the same voicings, played softer, low-passed by the lo-fi chain (Sam: lo-fi keeps its voicings through a song) | genre (filtered intro) | **Partly** (softer: `emit_chord`); filter **Plan 3** | *audio* §4.2 |

## Bass

| Choice | Value | Kind | Status | Source |
| --- | --- | --- | --- | --- |
| Styles | roots on kick, root-fifth, pedal (restrike per chord), walk (upright sets only, one 8-bar window in four) | genre | **Done** (`groove.c: groove_bass_style`, R13, R14, R21) | *genre* §4.1, §8.5; LOF-1 |
| **Diatonic only** | the walk's approach is a scale step, never chromatic | taste **(Sam)**: nothing out of the key. Chromatic approach is standard in jazz walking bass | **Done** (`groove.c: approach`) | 8dde786 |
| Register | E1–B2, centered on the tonic | genre (deep, warm bottom) | **Done** (`groove.c: bass_place`) | *genre* §4.1 |

## Drums

| Choice | Value | Kind | Status | Source |
| --- | --- | --- | --- | --- |
| Pattern families | boom-bap, half-time, sparse, brushes and ride, bossa; snare on 2 and 4 (or 3 in half-time); kick on 1 plus 1–3 offbeats; hats in 8ths at about half velocity; ghosts | genre | **Done** (`library.c: LIB_PATTERNS`); some kicks to check by ear (Task 3 minor) | *genre* §3.1; *midi* §3.4 |
| Polymeter | a brief 3-against-4 shaker | neighboring genre, approved **(Sam)** | **Done** (`poly3`) | spec |
| Fills | only at boundaries; no crashes, no rolls into drops | genre + taste (nothing startling) | **Partial** (before layer changes only) | spec; R13 |
| Kick ducking | light, −2 to −4 dB on chords and bass | genre | **Not yet** (Plan 3) | *genre* §5 |

## Melody and ear candy

| Choice | Value | Kind | Status | Source |
| --- | --- | --- | --- | --- |
| Lead role | a guest: some blocks only; chords are the star | taste **(Sam)** | **Done** (`plan.c`) | spec The band |
| Motif | 1–2 bars, about a third of beats, pentatonic-leaning, slightly late | genre | **Done** (`melody.c`) | *genre* §8.5 |
| Plan | head–solos–head; restate / variant / new 50 / 15 / 35 | genre (measured) | **Done** (`melody.c: melody_plan`) | *midi* §0 |
| Trading fours | lead and candy alternate 4-bar phrases | neighboring genre (jazz), approved **(Sam)** | **Done** (R10, R16) | spec |
| Ear candy | a few notes, answers, sparse figures; ≥ 3 gestures in ≥ 70% of songs; never over the lead | genre | **Done** (R19, 0996e03) | spec |

## Sound: instruments, dust and bed

| Choice | Value | Kind | Status | Source |
| --- | --- | --- | --- | --- |
| Six roles | chords, drums, bass, lead, ear candy, bed | genre | **Done** in the engine; bed **Not yet** | spec |
| Curated instrument sets | Rhodes, Wurli, felt piano, nylon, pad; upright, sub, electric bass; kalimba, vibes, bells, sax, muted trumpet; flavors lo-fi, bossa, city pop, neo-soul, chillwave | genre **(Sam: curated sets, not free mixing)** | **Partial** (`library.c` sets; placeholder voices only, Plan 3) | *genre* §4; Tuna, LOF-1 (*firmwares*) |
| Dust | low-pass about 1–5 kHz, wow and flutter, soft saturation, bit/rate grit; warm vinyl or wobbly tape | genre | **Not yet** (Plan 3) | *genre* §5 |
| Surface noise, ambience | vinyl crackle or tape hiss or none; rain or none; steady, opens and closes the song | genre **(Sam: three independent layers)** | **Not yet** (`L_BED` unset, Plan 3) | *genre* §5; MAKU, LOF-1 |
| Automation arcs, evolving weather, noise swell before drums | slow filter / wobble / reverb arcs over 16–32 bars; rain builds and passes | neighboring genres, approved **(Sam)** | **Partial** (`PB_SWELL` flagged; sound in Plan 3) | spec |
| Self-sampling chops, vocal chops, reverb freeze | later features | neighboring genres **(Sam: later)** | **Not yet** | spec |

## Excluded on purpose (genre or taste)

- EDM drops and risers, dub throws, trap hat rolls, abrupt beat switches (spec).
- Record skips, backspins, stutters, tape-stop endings: none in the measured tracks, and startling **(call)**.
- Chord substitutions, tritone subs, borrowed chords outside "jazzy" entries **(Sam)**.
- Chromatic approach tones in the bass **(Sam)** and in the melody (R25).
- Two-movement 5-minute songs (an early research proposal): replaced by 2–3.5 minute songs **(Sam)**.

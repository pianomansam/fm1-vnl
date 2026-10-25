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
(clash checks, `app/app.c` gap, and steady levels between sections and songs: `test_levels_steady`, Task 8).

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
| Chord rhythm | one comp style per song, as heard: held about 55%, half notes about 25%, arpeggio 20% (weights held 44, arpeggio 20, half notes 36; offbeats and stabs retired, **Sam**); rolled 10–60 ms. Half notes: the voicing struck on beats 1 and 3, the second strike a little softer (90% velocity), each held to the next, never revoiced within a chord; a busy lead (4+ onsets a bar) thins them to the chord changes. They hold instead over sustained keys (pad, vox pad, organ: a pad never restrikes) and over a kick on exactly beats 1 and 3 (**Sam's kick rule**: the keys and the kick share a beat only when both aren't sparse) | genre (sampled-loop feel); half notes **Sam** ("they could also do half notes") | **Done** (`harmony.c: harmony_style`, `harmony_comp_steps`, R15; half notes pads revision 2); no anticipations | *genre* §2.4; LOF-1 (*firmwares*); pads design rev. 2 |
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
| Kick ducking | light, −2 to −4 dB on chords and bass | genre; depth per song **(Sam: yes)** | **Done**: the pump, subtle 1 / medium 3 / obvious 6 dB, a third of songs each (Task 7b) | *genre* §5 |

## Melody and ear candy

| Choice | Value | Kind | Status | Source |
| --- | --- | --- | --- | --- |
| Lead role | a guest: some blocks only; chords are the star | taste **(Sam)** | **Done** (`plan.c`) | spec The band |
| Motif | 1–2 bars, about a third of beats, pentatonic-leaning, slightly late | genre | **Done** (`melody.c`) | *genre* §8.5 |
| Plan | head–solos–head; restate / variant / new 50 / 15 / 35 | genre (measured) | **Done** (`melody.c: melody_plan`) | *midi* §0 |
| Trading fours | lead and candy alternate 4-bar phrases | neighboring genre (jazz), approved **(Sam)** | **Done** (R10, R16) | spec |
| Sustained lead's phrase (flute, sax, muted trumpet, saw lead) | one 1–2-bar motif per song; an 8-bar question and answer: bars 1–2 the motif, 3 nudged, 4 a held 3rd or 5th (the question), 5–6 the motif again (same rhythm), 7 nudged otherwise, 8 a step down to a held root (the 3rd over home or a maj7 chord: the arrival); half a bar's breath after 4 and 8; about 2–3 onsets a bar, starts on beat 1 about 60% of phrases and strikes beat 1 in about two bars of three (**Sam**: beat 1 is the norm, varied, not avoided), leaps answered by a step back; strong-beat notes snapped to chord tones, weak ones kept as scale steps where legal; a note may tie over the bar line (its push, legal over both chords); motion under 40 drops passing and neighbor notes and the note before holds through their time; velocity arcs with the line's height and softens into the arrival | genre (Tuna, LOF-1, wind phrasing), **Sam** (fewer notes held longer) | **Done** (`phrase.c`, `lead.c: plan_phrase`) | sustained-lead design 2026-10-24; *research* arpeggiators §3–4 |
| Ear candy | a few notes, answers, sparse figures; ≥ 3 gestures in ≥ 70% of songs; never over the lead | genre | **Done** (R19, 0996e03) | spec |

## Sound: instruments, dust and bed

| Choice | Value | Kind | Status | Source |
| --- | --- | --- | --- | --- |
| Six roles | chords, drums, bass, lead, ear candy, bed | genre | **Done** (`voices.c`, `drums.c`, `fx.c`) | spec |
| Curated instrument sets | Rhodes, Wurli, felt piano, nylon, pad; upright, sub, electric bass; kalimba, vibes, bells, music box, vox pad, flute, sax, muted trumpet, guitar lick; the synths (below); flavors lo-fi, bossa, city pop, neo-soul, chillwave | genre **(Sam: curated sets, not free mixing)** | **Done** (`library.c` 16 sets, 5 of them synth sets; `patch.c`) | *genre* §4; Tuna, LOF-1 (*firmwares*) |
| Dust | low-pass about 1.5–5 kHz, wow and flutter, soft saturation, bit/rate grit; warm vinyl or wobbly tape | genre | **Done** (`fx.c`, four levels) | *genre* §5 |
| Surface noise, ambience | vinyl crackle or tape hiss or none; rain or none; steady, opens and closes the song | genre **(Sam: three independent layers; quieter, Task 8)** | **Done** (`fx.c` bed, about 35 dB under the music) | *genre* §5; MAKU, LOF-1 |
| Automation arcs, evolving weather, noise swell before drums | slow filter / wobble / reverb arcs over 16–32 bars; rain builds and passes | neighboring genres, approved **(Sam)** | **Partial**: section filter moves (intro and outro dark, breaks, drops, swells, a high-pass build into a groove; Task 7b) and rain gusts; no long arcs yet | spec |
| Background pad layer | a quiet layer on its own bus (V_PAD), **one part or the other per section, never both** (**Sam**, revision 2). The chord part: over percussive keys (Rhodes, Wurli, felt, DX, nylon) a mid-range chord, **2 notes, the 3rd and the 7th** (MIDI 55–72), voice-led, changing only at chord changes, about 8.5 dB under the keys (7–11 by set; **Sam**: "a bit loud", 3 dB down). The solo note: one high note a bar on beat 1, held, the scale's 1st/2nd/3rd/7th (the tonic about 60%), kept while legal, else the smallest step. **Optional:** 40% of songs never play it (**Sam**: "it can even NOT show up in a song"); in the rest its voice is rolled per song, equally likely, from the choir (vox pad), the flute and a soft organ at MIDI 72–84 and the strings an octave up (84–96), never the flute under a flute lead nor the organ over organ keys, all level-matched (within 0.5 dB at the songs' velocity) and under the chord part's level. Over struck keys the chord part is the default and the solo takes a section in about 25% of sections (intro and break 35%, groove 25%, under the lead 15%, outro 30%); over sustained keys (pad, vox pad, organ; no chord part) the solo note keeps the first design's shares (groove 60%, intro/break/outro 50%, under the lead 30%). Strings under the acoustic sets, a warm pad under the synth sets. | genre (background strings) + taste **(Sam: "a mid-range chord throughout", "a solo note an octave above", "often tonic or near it", "sometimes cutting out during a break"; revision 2: "the solo note to be an option when the chord voice wasn't playing", the choir, flute and soft organ "at this octave", the strings an octave up and quiet, not a plain sine)**; 2 notes, not the design's 3, to fit the CPU budget (**Sam**, 2026-10-24: the design's cut (c)); the 40% no-solo share (call) | **Done** (`pads.c: pads_section_part`, `pads_solo_voice`; `song.c: pads_group`; `voices.c` M_ENS and the solo lane's own voice; host A/B: `PADS=double` doubles the keys' voicing instead) | pads design 2026-10-24 and its revision 2 |
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
one. Every set keeps the lead 4–6 dB over its keys while it plays, the keys ducking 3 dB under it.

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
- Record skips, backspins, stutters, tape-stop endings: none in the measured tracks, and startling **(call)**.
- Chord substitutions, tritone subs, borrowed chords outside "jazzy" entries **(Sam)**.
- Chromatic approach tones in the bass **(Sam)** and in the melody (R25).
- Two-movement 5-minute songs (an early research proposal): replaced by 2–3.5 minute songs **(Sam)**.

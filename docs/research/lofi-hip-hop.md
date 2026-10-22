# Lo-fi hip hop: design research for a generative FM-1 firmware

Research date: 2026-10-22. Target: an M-VAVE FM-1 (single-core MCU, ~96 KB RAM, ~336 KB buffer pool, ~580 KB flash) that generates ~5-minute "songs" from probability, each one deliberately different from the last.

**How to read this.** Claims carry a source link. Anything marked **[inferred]** is my own derivation or synthesis, not a sourced fact. Tutorial and blog numbers are producer heuristics, not measurements, and I say so where they disagree. No corpus-scale academic study of lo-fi tempo, key, or form turned up. The one academic paper on the genre, [Winston & Saywood, IASPM Journal 9(2), 2019](https://iaspmjournal.net/index.php/IASPM_Journal/article/view/949), is qualitative. So the hard numbers below come from three places: track durations I scraped from Bandcamp, two open-source generators I read line by line, and MIDI files I parsed myself.

---

## 1. Song form

### 1.1 Track length (measured)

I pulled the per-track `duration` fields from Bandcamp album pages (the scrape used curl; nothing was executed):

| Release | n | Mean | Range |
|---|---|---|---|
| [Lofi Girl, *Patterns*](https://lofigirl.bandcamp.com/album/patterns) | 5 | 2:33 | 2:12–3:07 |
| [Lofi Girl, *Breakfast EP*](https://lofigirl.bandcamp.com/album/breakfast-ep) | 5 | 1:45 | 1:37–1:52 |
| [Lofi Girl, *Eternal Winter*](https://lofigirl.bandcamp.com/album/eternal-winter) | 7 | 2:19 | 1:49–3:01 |
| [Lofi Girl, *Harmony with Nature*](https://lofigirl.bandcamp.com/album/harmony-with-nature) | 9 | 2:23 | 2:12–2:53 |
| [Chillhop Essentials Winter 2023](https://chillhop.bandcamp.com/album/chillhop-essentials-winter-2023) | 28 | 2:34 | 1:51–3:32 |
| [Chillhop Essentials Summer 2022](https://chillhop.bandcamp.com/album/chillhop-essentials-summer-2022) | 24 | 2:50 | 1:50–3:59 |
| [tomppabeats, *Habits*](https://tomppabeats.bandcamp.com/album/habits) | 9 | 2:32 | 0:57–3:29 |
| [tomppabeats, *Harbor LP*](https://tomppabeats.bandcamp.com/album/harbor-lp) | 38 | **0:55** | 0:22–1:35 (sketch-length beat tape, excluded from pooled stats) |
| [potsu, *Ivy League*](https://potsu.bandcamp.com/album/ivy-league) | 10 | 2:53 | 1:44–3:56 |
| [Philanthrope, *Quintessenz… Instrumentals*](https://philanthrope.bandcamp.com/album/quintessenz-und-ihri-nuance-instrumentals) | 20 | 2:36 | 1:21–4:28 |

**Pooled (n=117, excluding Harbor):** mean 2:35, median 2:26, p10 1:44, p90 3:29, max 4:29.

That confirms your hunch: real tracks run 1.5–3.5 minutes, and almost none reach 5. Tutorials agree. [Native Instruments](https://blog.native-instruments.com/lo-fi-hip-hop/) says "about 1 to 3 minutes", and a [lo-fi production checklist](https://www.allesgelingt.de/p/lofi-beats-creation-wbz21) targets "about 2 minutes".

**What that means for a 5-minute target [inferred].** A 5-minute song is about two real tracks long. A single loop held for 5 minutes will sound like one track with the repeat button on. Two better options:
- **"Two-movement" form:** loop A (about 2.5 min) leads into a transition (a breakdown or a tape stop), then a related loop B (new progression or new lead instrument, same tempo and key family). Return to A briefly, or end on B.
- **"Beat-tape medley" form:** two or three short pieces of 60–100 s each, joined by the genre's own transitions (a filter sweep, a drum drop, a tape stop, or a vinyl-noise-only bridge). This matches how lo-fi is actually consumed: [Lofi Girl's 24/7 stream](https://iaspmjournal.net/index.php/IASPM_Journal/article/view/949) is a continuous playlist of short tracks.

### 1.2 Tempo

| Source | BPM |
|---|---|
| [Native Instruments](https://blog.native-instruments.com/lo-fi-hip-hop/) | 60–90 (demo at 65) |
| [Mode Audio](https://modeaudio.com/magazine/lofi-hip-hop-5-production-essentials) | 60–80 |
| [Production checklist](https://www.allesgelingt.de/p/lofi-beats-creation-wbz21) | 70–90, 80 default, 75 for a "sleepy" feel |
| [Big Fish Audio *Lo-Fi Nights*](https://www.bigfishaudio.com/Lo-Fi-Nights-Loops-Melodies) | loops at 70–100 |
| [jacbz/Lofi generator](https://github.com/jacbz/Lofi) (code) | clamps to 70–100, in steps of 5 |
| [Magenta Lo-Fi Player](https://github.com/magenta/lofi-player) (code) | default 75, presets at 75/90/100 |
| Nujabes on Hooktheory | [Aruarian Dance](https://www.hooktheory.com/theorytab/view/nujabes/aruarian-dance) 100, [After Hanabi](https://www.hooktheory.com/theorytab/view/nujabes/after-hanabi) 83, Luv(sic) Pt 3 95, Battlecry 98. The [UCLA Lofi Lab](https://lofilab.humspace.ucla.edu/analysis) measured Aruarian Dance at 95, so sources disagree by ±5. |

The center of mass is **70–90 BPM**, with a jazzier, Nujabes-lineage tail at 90–100. Some producers count the same groove at double time (140–180) and put the snare on beat 3. The Magenta player's pattern 0 does exactly that (see §3).

### 1.3 Meter
4/4 everywhere. Every MIDI file I parsed carries a 4/4 time-signature event, and both generators assume 4/4. Triplet or 12/8 feel arrives through swing, not through the time signature.

### 1.4 Section lengths and templates
- **Loops are 4 or 8 bars.** [Tunee](https://www.tunee.ai/music-generator/lofi) says "loops of 4–8 bars that repeat with subtle variations", and all the Magenta MIDI files are 8 bars (a 4-bar progression played twice).
- **Checklist template** ([allesgelingt](https://www.allesgelingt.de/p/lofi-beats-creation-wbz21)): Intro 8 bars (textures plus filtered chords), Main 32 (full drums, bass, melody), Breakdown 8 (drums removed, ambient noise kept), Outro 8 (fade). That is 56 bars, about 2:48 at 80 BPM.
- **A blog-observed form** ([jdtorian](https://jdtorian.bearblog.dev/lofi-girl-synthwave-is-candy-and-im-fine-with-that/), described as AI-assisted, so weak): intro 4–8, A 8, A2 8, B 8, A3 8, breakdown 8, outro 8.
- **NI:** an intro, a main loop that builds slightly, and sometimes a breakdown where elements drop out and come back ([NI](https://blog.native-instruments.com/lo-fi-hip-hop/)). Another guide calls this "change through subtraction and re-entry" ([Violet Recording](https://violetrecording.com/how-to-make-lofi-beats/)).

### 1.5 Arrangement moves (with sources)
- **Filtered intro:** chords pass through a low-pass, often with no drums ([checklist](https://www.allesgelingt.de/p/lofi-beats-creation-wbz21)). Automate the cutoff slowly over 8 or 16 bars ([Violet Recording](https://violetrecording.com/how-to-make-lofi-music/)).
- **Reversed pad swell** into the intro ([Loopmasters](https://www.loopmasters.com/articles/4298-How-To-Make-Chilled-LoFi-Hip-Hop-Beats-Sounds-Samples-Loops)).
- **Drum drop-outs at loop boundaries.** jacbz's generator (code) leaves the first bar of the main section drumless, and the last bar of every pass through the progression too. The padding is 2 bars when the progression is longer than 8 chords. Magenta's "autoBreak" (code) checks every 2 bars: if the drums are playing, it mutes them with p=0.2; if they are muted, it unmutes them with p=0.95. So drums usually drop for exactly 2 bars.
- **Breakdown:** drums out, keys plus vinyl or ambience stay in ([checklist](https://www.allesgelingt.de/p/lofi-beats-creation-wbz21)).
- **Outro:** jacbz plays the first 2 chords of the progression, fades out over twice their duration, and finishes with 1 bar of silence. Vinyl or rain stops half a bar before the end (code).
- **Tape stop:** a common lo-fi ending or transition, but I found no sourced duration. The [Yum Audio LoFi Tapestop](https://musictech.com/news/gear/tape-stop-effects-in-your-daw-with-yum-audios-lofi-tapestop/) syncs its stop time to beats. **[inferred]** Use 1–2 beats for a transition and 2–4 beats for an ending, landing on a downbeat.
- **Call and response:** the melody calls in bars 1–2 and answers in bars 3–4 ([checklist](https://www.allesgelingt.de/p/lofi-beats-creation-wbz21)).

### 1.6 Arrangement maps
I could not find any published bar-by-bar maps of named lo-fi tracks. Tutorials teach templates, not transcriptions. What I can give:

**(a) Generator maps (exact, from source code):**
- **jacbz/Lofi** (`client/src/producer.ts`): `[1 bar silence] + [main = progression × ceil(24 / progLen)] + [first 2 chords as fade-out] + [1 bar silence]`. With a 4-chord, 1-chord-per-bar progression, that is 1 + 24 + 2 + 1 = **28 bars**, about 1:24 at 80 BPM. Drums are off in bar 1 of the main section and in the last bar of every pass, so they play 3 bars on and 1 bar off.
- **Magenta Lo-Fi Player:** an 8-bar transport loop with a 4-bar bass loop and an 8-bar chord MIDI, so all layers line up at 8 bars. Every 2 bars the drum Markov toggle above runs. The "randomChange" step fires only after more than 8 drum-on decisions, which is about 16 bars or more. It then changes melody, chords, instrument, or background at the probabilities in §7. After 400 such cycles, about 800 bars, everything resets.

**(b) Inferred maps from duration × tempo [inferred]:**
- A 2:12 track at 80 BPM (3.0 s per bar) is 44 bars, for example 4 intro + 32 main (as 4 × 8) + 4 breakdown + 4 outro.
- *Aruarian Dance* (ii9–V13 vamp, 95–100 BPM): Hooktheory lists 32 beats of melody, which suggests an 8-bar repeating section, consistent with a 2-chord vamp held for long stretches.
- **5 minutes at 70/80/90 BPM = 87.5/100/112.5 bars.** A good grid is 96 bars (24 × 4), or 104 at faster tempos.

---

## 2. Harmony

### 2.1 Chord vocabulary
- **Extended chords replace triads:** maj7, m7, 9ths ([Violet Recording](https://violetrecording.com/lofi-chord-progressions/)). Examples: Cmaj7 = C E G B, Cmaj9 = C E G B D, Dm9 = D F A C E.
- **ii–V–I** (Dm7–G7–Cmaj7) is called the most common cadence, inherited from jazz samples ([Violet Recording](https://violetrecording.com/lofi-chord-progressions/), [checklist](https://www.allesgelingt.de/p/lofi-beats-creation-wbz21)). The minor version is Dm9–G7(b13)–Cm9 ([Elephorm](https://www.elephorm.com/formation/audio-mao/guide-de-production-du-beatmaker-le-cheatsheet-par-style/harmonie-melodie-et-arrangement-lo-fi-chill-et-ambient), partly paywalled).
- **Common loops** ([Orphiq](https://orphiq.com/resources/lofi-chord-progressions), [checklist](https://www.allesgelingt.de/p/lofi-beats-creation-wbz21)): Cmaj7–Am7–Dm7–G7; Am7–Dm7–G9–Cmaj7; Am9–Dm7–G13–Cmaj7.
- **A sampled jazz loop** ([Melodics](https://melodics.com/blog/how-to-create-lofi-chord-progressions)): Bm7–E7–Amaj7–F#7, which is ii–V–I–VI7 (a secondary dominant back to ii).
- **Nujabes:** *Aruarian Dance* is a **ii9–V13** vamp in F# major, G#m9–C#13 ([Hooktheory](https://www.hooktheory.com/theorytab/view/nujabes/aruarian-dance)), with Hooktheory's chord-complexity score at 95. A guitar tab adds Bm7(add9), C#m7, Dmaj7, E7, and F#7sus4 ([tab](https://pg.zachmanson.com/tab/nujabes/aruarian-dance-tabs-967199), unverified).
- **Magenta's preset progressions** (filenames in [the repo](https://github.com/magenta/lofi-player/tree/master/midi)): IV–IV–I–I in C, I–V–vi–IV in C, i–III–iv–v in Am, VI–i–VI–v in Am.
- **lofi-engine's next-chord rules** ([docs](https://mintlify.wiki/meel-hd/lofi-engine/technical/music-generation)): I→{ii, iii, IV, V, vi, vii}, ii→{iii, V, vii}, V→{I, iii, vi}. Its voicings are 7-note stacks, e.g. I = [0, 4, 7, 11, 14, 17, 21] (maj13 territory) and V = [0, 4, 7, 10, 14, 17, 21].
- **Not directly sourced, but standard jazz/neo-soul practice and consistent with the above [inferred]:** iv-minor borrowing (IVmaj7 → ivm7 → Imaj7), chromatic descending ii–V chains, tritone-sub bII7, and the "Just the Two of Us" pattern (IVmaj7–III7–vim7–v7–I7).

### 2.2 Progression lengths
2-chord vamps (Nujabes ii–V) or 4-chord loops with **1 chord per bar**, repeated to fill 8 bars. One Magenta file holds each chord for 2 bars (IV IV I I). jacbz uses 1 chord per bar. **[inferred]** Weights to start from: 4 chords × 1 bar ≈ 60%, 2 chords × 2 bars ≈ 25%, 8-chord or 2-bars-per-chord ≈ 15%.

### 2.3 Keys and modes
No corpus data exists. Sourced examples: C, Am, F# (Aruarian), and Ab minor (UCLA's reading of the same track). jacbz offers all 7 church modes and picks **rain over vinyl** when the mode is Aeolian and valence is below 0.5 (code). **[inferred]** Any key is fine. Bias toward flat-side and minor or Dorian centers, and toward relative-major loops built around ii and IV.

### 2.4 Voicings (measured from the Magenta MIDI files)
I parsed the files with a stdlib-only Python reader. They live under `/private/tmp/claude-501/-Users-samueloltz-Documents-fm-1/dfd13d60-fb03-409f-86fe-9daa1a019026/scratchpad/midi/`.
- **Mostly 4-note 7th-chord voicings** in the C3–E4 range, e.g. Fmaj7 = F3 A3 C4 E4 and Cmaj7 = C3 E3 G3 B3. Some keep the root in the low octave (F2 + F3) and add the 9th (Fmaj9 = F A C E G).
- **Chords are rolled, not block.** Note onsets inside one chord spread **0.05–0.3 beats**, which is about 40–240 ms at 75 BPM.
- **About half the chords land late:** on beat 1.5, a full 8th note behind the downbeat. That is a lazy "behind the bar" comp.
- **Velocities run 26–100** in the hand-played files, while the "quantized" files use a flat 100. Durations are about 3.4–4.0 beats (legato, bar-long).
- **Rootless and spread voicings:** standard jazz practice, letting the bass carry the root ([general piano refs](https://jazzedge.academy/paths/learn-your-rootless-chords)). Lo-fi-specific sources say nothing on this. jacbz inverts the chord by moving its lowest note up an octave (code).

### 2.5 Jazz samples
The genre is built on sampled jazz chord loops: [Luv(sic) samples Aki Takase's "Minerva's Owl"](https://en.wikipedia.org/wiki/Luv(Sic)_Hexalogy), and [Melodics](https://melodics.com/blog/how-to-create-lofi-chord-progressions) analyzes a sampled loop. **[inferred]** A sampled loop behaves like this: a fixed 2–4 bar harmonic cycle, one instrument's timbre "baked in", and a slightly off-grid loop point. The firmware can imitate all three by rendering the chord part once per section and treating it as a loop.

---

## 3. Rhythm

### 3.1 Kick and snare placement
- **Kick on 1 (and 3), snare or rimshot on 2 and 4,** with an occasional ghost kick just before 3 ([checklist](https://www.allesgelingt.de/p/lofi-beats-creation-wbz21), [Mode Audio](https://modeaudio.com/magazine/lofi-hip-hop-5-production-essentials)). Patterns are sparse, often just 2–3 elements.
- **[MusicRadar](https://www.musicradar.com/tutorials/you-can-easily-construct-your-own-bespoke-beat-rather-than-relying-on-stock-loops-how-to-make-fresh-lo-fi-beats)** (via search snippet): kick on the downbeat, a grace kick on the last 16th, an extra kick on 16th #10 (the "and" of 3, i.e. step 9 counting from 0). The bar-1 and bar-3 kicks are nudged late, the bar-2 and bar-4 kicks very slightly early.
- **Magenta's three patterns** (exact code, 16 steps per bar, step 0 = beat 1):
  - P0: kick on step 0, snare on step 8 (half-time backbeat), hat on every even step (8ths).
  - P1: kick on steps 0 and 20 of a 2-bar cycle (step 4 of bar 2 = beat 2), snare on step 8, 8th hats **delayed 70 ms**.
  - P2: kick on steps 0 and 10, plus step 11 in bar 2; snare on steps 4 and 12 (2 and 4); 8th hats delayed 70 ms.
- **Ghost notes:** low-velocity swung ghosts ([Loopmasters](https://www.loopmasters.com/articles/4298-How-To-Make-Chilled-LoFi-Hip-Hop-Beats-Sounds-Samples-Loops)). Ghost hats bring out the swing, and snare ghosts stay "as subtle as possible" ([Attack Magazine](https://www.attackmagazine.com/technique/beat-dissected/lo-fi-electro/)).
- **Hats:** 8ths, or 16ths with lower velocity on the "ands" ([MusicRadar](https://www.musicradar.com/tutorials/you-can-easily-construct-your-own-bespoke-beat-rather-than-relying-on-stock-loops-how-to-make-fresh-lo-fi-beats)). Vary velocity on every hit ([Violet Recording](https://violetrecording.com/how-to-make-lofi-beats/)).
- **Drum source:** jacbz uses pre-recorded drum loops picked by BPM and energy, not step patterns. Magenta uses 3 one-shots (kick, snare, hat). **[inferred]** Shakers and rimshots stand in for hats or snare in softer sections.

### 3.2 Swing
- **Roger Linn's definition:** swing % = the ratio of the first 16th's duration to the full 8th. 50% is straight, 66% is a perfect triplet, and **54% "loosens up" straight 16ths** ([Linn via Brettworks](https://brettworks.com/2013/07/23/roger-linn-on-drum-machine-groove-and-j-dillas-off-beat-sound/)).
- **What producers use:**
  - Hip-hop/funk: 55–65% ([MPC Forums](https://www.mpc-forums.com/viewtopic.php?p=412404)).
  - Classic boom-bap (Premier): about 65–68%. Above 70% is the "freaky Madlib" feel (same thread).
  - Lo-fi hats: 55–60% ([checklist](https://www.allesgelingt.de/p/lofi-beats-creation-wbz21)).
  - Ableton "MPC 16 Swing-59" used in a lo-fi tutorial; Attack Magazine's lo-fi electro uses 55–65% (search snippets).
  - Ethan Hein's experiment requantized Herbie Hancock's "Chameleon" to 66% ([Hein](https://ethanhein.com/wp/2022/dilla-time-in-chameleon/)).
- **jacbz turns swing on with only p = 0.1** (code), so most of its tracks are straight. Magenta applies "swing" as random per-note jitter: chords get up to 0.1 × swing × (75/BPM) s, melody up to 0.3 × swing × (75/BPM) s. At swing = 1 that is up to 100 ms on chords and 300 ms on melody at 75 BPM.

### 3.3 Dilla time and microtiming
- **Dilla time** means several feels at once: some parts straight, some swung, some ahead of the grid, some behind ([Hein summarizing Charnas](https://ethanhein.com/wp/2022/dilla-time/)).
- In "Chameleon," the hats drag, the snare is anticipated by a 16th, and kick and snare swing different amounts, all varying bar to bar ([Hein](https://ethanhein.com/wp/2022/dilla-time-in-chameleon/)).
- Charnas's book cites a Dilla snare about **65 ms** off the grid ([LRB review](https://www.lrb.co.uk/the-paper/v44/n20/francis-gooding/basement-beats)). That is a secondary source.
- Drummers told to play "laid-back" at 96 BPM delayed the snare by **17.4 ms on average** ([Câmara thesis, UiO](https://duo.uio.no/bitstream/handle/10852/79965/MP3801_01_Camara.pdf?sequence=1)).
- The checklist suggests a snare **5–10 ms late** ([allesgelingt](https://www.allesgelingt.de/p/lofi-beats-creation-wbz21)). Magenta delays hats **70 ms** in two of its patterns (code).
- Sean Peterson's microtiming study calls the style "tipsy": a mix of stability and instability ([UO ScholarsBank](https://scholarsbank.uoregon.edu/xmlui/handle/1794/23759)). The abstract gives no numbers.

**Timing budget [inferred from the above]:**

| Element | Offset |
|---|---|
| Kick | 0 to −10 ms (early or on grid), ±5 ms jitter |
| Snare | +10 to +30 ms late (up to +65 for "drunk"), ±8 ms jitter |
| Hats | +0 to +70 ms late, with independent swing of 54–62% |
| Chord rolls | 10–60 ms between notes; whole chord sometimes an 8th late |
| Bass | locked to the kick (Hein: bass aligns closely with drums) |

### 3.4 Velocity
Magenta's played MIDI uses velocities 26–100. **[inferred]** Accents at 90–110, backbeat snare at 80–100, ghosts at 25–45, hats alternating about 70/45.

---

## 4. Instruments and timbres

### 4.1 What appears and how it's layered
- **Keys:** Rhodes or electric piano, felt piano, piano. Melody: kalimba, nylon or fingerpicked guitar, muted electric guitar. Low end: upright or sub bass. Textures: vinyl crackle, rain, distant vocal chops ([pack listing](https://pluginfox.com/collections/kontakt-instruments/lofi), plus aggregate prompt examples, weak sources).
- **jacbz presets** (code), chosen by energy and valence:

| Preset | When | Bass | Chords | Arpeggio | Melody |
|---|---|---|---|---|---|
| P1 | energy ≥ 0.6 | bass guitar @0.6 | EP @0.8 | piano, −1 oct @0.1 | electric guitar @0.6 |
| P2 | energy < 0.6 | bass guitar @0.6 | EP @0.8 | harp @0.2 | EP +1 oct, in octaves, @0.9 |
| P3 | energy < 0.3 and valence < 0.5 | bass guitar @0.5 | EP @0.6 | none | soft piano +1 oct |

  Every jacbz track has vinyl **or** rain under the whole song.
- **jacbz bass patterns** (beat, duration):
  - [(0, 4)] — whole note
  - [(0, 2), (2, 2)] — two halves
  - [(0, 3), (3, 1)]
  - [(0, 3.5), (3.5, 0.5)] — push into the next bar
  - [(0, 1.5), (1.5, 1.5), (3, 1)] — tresillo

  All play the chord root, and the pattern is chosen at random per bar.
- **First-beat arpeggio:** scale-degree pattern [1, 5, 8, 9, 10] in 8ths (P1 uses [1, 5, 8, 5, 10, 5, 8]).
- **Magenta:** square-wave bass through a **200 Hz LPF**, whole-ish notes (0.7 bar). Instruments: synth, piano, acoustic guitar, electric guitar. Background loops: rain, waves, street, kids.

### 4.2 What FM can plausibly do on the FM-1 [mostly inferred]
| Timbre | FM feasibility | Notes |
|---|---|---|
| Rhodes / EP | **Excellent.** This is the DX7's signature. | DX7 E.PIANO 1 uses algorithm 5: three 2-op stacks; 4→3 and 6→5 give the body, 2→1 the tine ([Muzines](https://www.muzines.co.uk/articles/yamaha-dx7/4733)). One recreation uses carriers at 1.0 and tine modulators at 14.0 with fast-decaying modulator envelopes ([DecentSampler guide](https://decentsampler-developers-guide.readthedocs.io/en/1.21.1/topic-how-to-use-fm-synthesis.html)). Velocity maps to brightness: soft is mellow Rhodes, hard is bell ([MPW](https://musicproductionwiki.com/articles/how-to-recreate-the-dx7-electric-piano)). |
| Sub / upright bass | Good | 2-op at 1:1, low index, fast index decay for the pluck; LPF ~200–400 Hz (Magenta uses 200 Hz). |
| Kalimba, music box, vibes, bells | Excellent | Inharmonic ratios (e.g. 1:3.5, 1:7) with short decays. |
| Soft pads | Good | Low index, slow attack, detuned pair plus chorus. |
| Felt / acoustic piano | Fair | FM pianos sound "DX". After LPF and wow they pass as felt piano. |
| Nylon / muted jazz guitar | Fair to weak | A plucked 2–3 op with fast decay works as a muted guitar. Nylon realism wants samples or Karplus-Strong. |
| Flute | Fair | Sine plus breath noise and slow vibrato. |
| Sax, vocal chops | **Need samples** | |
| Drums | Fair | FM or analog-style synthesis gives "808-ish" drums. Dusty boom-bap kicks and snares are much better as short 12-bit samples. |
| Vinyl crackle, hiss, rain | **Procedural, very cheap** | Crackle = sparse random impulses (Poisson) through a band-pass. Hiss = filtered white noise. Rumble = brown noise or LP-filtered noise ([MODE Audio](https://modeaudio.com/magazine/lofi-essential-production-tools)). Rain = dense filtered noise with random droplet clicks. |

**Memory [inferred]:** 336 KB of buffer at 16-bit/22 kHz mono holds about 7.6 s of audio. At 12-bit-equivalent 8-bit µ-law/26 kHz it holds about 13 s. That fits a small drum kit (kick, snare, rim, hat, shaker: about 1–1.5 s total) plus 2–4 short vocal or sax one-shots. Everything else should be synthesized.

---

## 5. Effects and the lo-fi chain

| Stage | Sourced values | Notes |
|---|---|---|
| **Low-pass (high cut)** | Master 3–5 kHz ([checklist](https://www.allesgelingt.de/p/lofi-beats-creation-wbz21)). 2–4 kHz for an obvious lo-fi sound ([Yamaha Hub](https://hub.yamaha.com/proaudio/recording/how-to-create-lo-fi)). Chords and melody 12 dB/oct from 1.5–2 kHz plus HPF at 150–200 Hz ([Mixed In Key](https://mixedinkey.com/captain-plugins/wiki/how-to-make-lofi-hip-hop/)). Drums down to ~2 kHz ([Mode Audio](https://modeaudio.com/magazine/lofi-hip-hop-5-production-essentials)). Gentle 12–16 kHz for subtle ([UJAM](https://ujam.com/blog/top-5-production-mixing-tips-for-lofi)). | Magenta (code): instrument bus LPF **1000 Hz** into reverb; drums LPF 200–10,200 Hz (default ~5.2 kHz); bass LPF 200 Hz. lofi-engine: piano LPF **1000 Hz**, hats LPF **2400 Hz** ([docs](https://mintlify.wiki/meel-hd/lofi-engine/technical/music-generation)). |
| **Wow & flutter** | Wow is below about 4–6 Hz; flutter is about 6–100 Hz ([SOS](https://www.soundonsound.com/node/4922148), [Vintage Digital](https://www.vintagedigital.com.au/glossary/wow-and-flutter/)). Real cassette decks measure 0.08–0.2%+ weighted ([Lindos](https://lindos.co.uk/articles/wow-and-flutter-measurement)). RC-20 rates: wow 0.1–4 Hz, flutter 6–20 Hz ([XLN manual](https://assets.xlnaudio.com/documents/rc-20-retro-color_manual.pdf)). Checklist: pitch depth 1–3% ([allesgelingt](https://www.allesgelingt.de/p/lofi-beats-creation-wbz21)). | 1% is about 17 cents, so 1–3% is a heavily "warped" tape; real decks sit near 0.1%. **[inferred]** Wow 0.3–1.5 Hz at 3–15 cents, flutter 8–14 Hz at 1–3 cents, with random-walk drift on the wow rate. Wow is cheap: modulate the playback or oscillator pitch. |
| **Bit / rate reduction** | SP-1200: **12-bit, 26.04 kHz**, drop-sample pitch shifting, SSM2044 filter ([Wikipedia](https://en.wikipedia.org/wiki/E-mu_SP-1200), [Rossum](https://www.rossum-electro.com/products/sp-1200/)). SP-404MKII records 48 kHz/16-bit; lo-fi comes from its effects: 303/404 VinylSim, Cassette Sim, Lo-fi ([Roland support](https://support.roland.com/hc/en-us/articles/4408189989147-SP-404MK2-Can-I-sample-sounds-at-a-specified-sampling-frequency-or-bit-rate)). | **[inferred]** On the FM-1, running drums or samples at 26 kHz with 12-bit quantization is nearly free and authentic. Drop-sample pitching (no interpolation) is cheaper still and gives the SP-1200 alias "crunch". |
| **Vinyl sim** | SP-404 VinylSim has 3 parameters, 0–100 each: **FREQUENCY** (playback frequency response), **NOISE** (level), **WOW FLUT** ([Roland manual](https://static.roland.com/manuals/sp-404mk2_reference/eng/17805541.html)). | A good minimal model for a "dust" macro. |
| **Noise levels** | No dB figure sourced. "Barely audible" ([checklist](https://www.allesgelingt.de/p/lofi-beats-creation-wbz21)). Noise that's too loud buries the drums ([Audient](https://audient.com/cn/tutorial/5-lo-fi-production-tips/)). | **[inferred]** Crackle peaks about −24 to −18 dBFS, hiss −45 to −36 dBFS RMS, rain −30 to −22 dBFS. Raise noise 3–6 dB in breakdowns, since it is the "glue". |
| **Saturation** | Tape emulation; drive drums hard into saturation ([UJAM](https://ujam.com/blog/top-5-production-mixing-tips-for-lofi)). | **[inferred]** Soft-clip (tanh or a cubic table) on the drum bus and master. |
| **Sidechain** | Kick ducks other parts, ratio 2:1–3:1, fast attack and release ([UJAM](https://ujam.com/blog/top-5-production-mixing-tips-for-lofi)). Kick ducks the bass ([checklist](https://www.allesgelingt.de/p/lofi-beats-creation-wbz21)). | **[inferred]** No compressor needed: apply a gain envelope on kick hits (−2 to −4 dB, 80–150 ms release) to the chords and bass. |
| **Reverb** | Magenta: instrument reverb 8.5 s decay, 0.1 s pre-delay, 30% wet, behind a 1 kHz LPF and a chorus (4 Hz, 2.5 ms delay, 0.1 depth). Master reverb 1.0 s decay. Master compressor −15 dB, 7:1 (code). | A long dark reverb at low wet is the signature. **[inferred]** On an MCU, use a small Schroeder/FDN at low sample rate with heavy damping. |
| **Stereo** | lofi-engine widens piano 0.5 and hats 0.7 ([docs](https://mintlify.wiki/meel-hd/lofi-engine/technical/music-generation)). | **[inferred]** Keep it mono-ish: bass, kick, and snare center, a gentle chorus on keys, crackle slightly wide. |
| **Tape-speed darkening** | Slowing 15 → 7.5 ips cuts highs ([ProducerSphere](https://www.producersphere.com/how-to-make-lofi/)). | |

---

## 6. MIDI examples

### 6.1 Inspected directly (parsed with stdlib Python, never executed)
**[magenta/lofi-player `midi/`](https://github.com/magenta/lofi-player/tree/master/midi)**, Apache-2.0 (repo license). Files are 247–467 bytes, format 0, 96 PPQ, 4/4, all **8 bars**.
- `IV_IV_I_I_C_{1,2,3}`, `progression_75_C`: Fmaj7(9) for 2 bars, then Cmaj7(9) for 2 bars, ×2. Voicings are 4 notes, F3–E4 and C3–B3. Variant 1 adds a top-line melody inside the chord part (C5, D5, G4, B4). Variant 3 has bass roots in the 3rd octave (C3, F3) and passing D4/A4.
- `I_V_vi_IV_C`: Cmaj7–G7(sus feel: G2 F3 B3 D3)–Am7(add C3 E3 G3)–Fmaj7(F3 A3 C4), ×2.
- `i_III_iv_v_Am`: Am(add9)–Cmaj7–Dm7–Em7(add9), ×2. The last chord is pushed to beat 1.5.
- `VI_i_VI_v_Am`: Fmaj9 (F2 F3 A3 C4 E4 G4)–Am–Fmaj7–Em7(11) (E2 E3 A3 B3 D4 E4).
- `melody/m_1_C`: a sparse 8th-note melody in the C5–G6 range. Phrases start on beat 2, rest on beat 1 of every 2nd bar, and repeat a 2-bar motif with a varied ending: call and response.
- **Common traits:**
  - Chords are hand-rolled, with onsets spread 0.05–0.3 beats.
  - About 40% of chord changes land an 8th late.
  - Durations are near full-bar legato.
  - Velocities run 26–100 in the "played" files.

  Parser script: `/private/tmp/claude-501/-Users-samueloltz-Documents-fm-1/dfd13d60-fb03-409f-86fe-9daa1a019026/scratchpad/tools/midiparse.py`

### 6.2 Other sources (not downloaded)
| Source | What | License (as stated) |
|---|---|---|
| [Beach Roads free lofi piano MIDI](https://beachroads.gumroad.com/l/lofi-piano-midi-files-pack-chill-melody-and-chord-progressions) | ~20 MIDI chords and melodies | "100% royalty free" (seller claim) |
| [Beach Roads Foundations](https://beachroads.gumroad.com/l/free-midi-pack-foundations-chords-and-melodies-pack) | chords and melodies | royalty free (seller claim) |
| [Unison free Emotional MIDI chord progressions](https://unison.audio/chord-midi/) | 24 voiced progressions, minor and slow | free; check terms |
| [Cymatics free MIDI list](https://cymatics.fm/blog/midi-files/) | free Python MIDI collection (51 loops, hip-hop/R&B) | free; check terms |
| [Big Fish *Lo-Fi Nights*](https://www.bigfishaudio.com/Lo-Fi-Nights-Loops-Melodies) | 40 MIDI files, 70–100 BPM | paid, royalty-free |
| [midipacksforsale 2,500 lo-fi loops](https://midipacksforsale.gumroad.com/l/thwtzh) | 1,200 chord loops, 1,200 melody loops, 100 drum loops, all 12 keys | paid ($9), royalty-free |
| [Hooktheory TheoryTab](https://www.hooktheory.com/theorytab/view/nujabes/aruarian-dance) / [Trends API](https://hooktheory.com/api/trends/docs) | Roman-numeral chords for Nujabes etc.; next-chord probabilities | site ToS; API requires an account |
| [callaunchpad/lofi-bytes](https://github.com/callaunchpad/lofi-bytes) | YouTube lo-fi converted audio-to-MIDI to train an LSTM or Music Transformer | repo; check license |
| [Melo04/lofi-generator-LSTM](https://github.com/Melo04/lofi-generator-LSTM) | LSTM over a lo-fi MIDI set (attributed to "Zachary Katnelson"; I could not locate that dataset) | check repo |
| [jacbz/Lofi](https://github.com/jacbz/Lofi) | VAE trained on Hooktheory chord and melody data; outputs key, mode, BPM, energy, valence, chords, melodies | Apache-2.0 |
| [Lakh MIDI](https://colinraffel.com/projects/lmd/) / [MidiCaps](https://arxiv.org/pdf/2406.02255) | general MIDI corpora, not lo-fi-specific | CC (per MidiCaps) |

**What the MIDI shows about structure:** 4-chord, 1-or-2-bars-per-chord progressions stretched to 8 bars. 4-note 7th or 9th voicings within about 1.5 octaves above C3, with the bass doubled an octave below in some files. Hand-rolled timing. Melodies sparse, an 8th-note grid, call and response over 2 bars.

---

## 7. Existing generative lo-fi projects and their rules

1. **Magenta Lo-Fi Player** ([blog](https://magenta.tensorflow.org/lofi-player), [code](https://github.com/magenta/lofi-player)), by Vibert Thio, 2020. Lo-fi was chosen because its "relatively simple music structure" keeps generated output making sense. MusicVAE interpolates between melodies and MelodyRNN continues them.
   - **Fixed layers:** 3 one-shot drums, square bass, preset chord MIDI, background loops.
   - **Variation rules (code):**
     - Drums use a Markov toggle every 2 bars (on→off p=0.2, off→on p=0.95).
     - After 8 or more drum-on decisions, `randomChange()` runs. It swaps the chords instrument (p 0.05), then the melody instrument (p 0.05), then the melody interpolation index (p 0.5, stopping there). Next it either changes the melody preset (p 0.1) or asks the RNN to continue the melody (p 0.1). If neither fired, it may change the chord progression (p 0.05) or the background sound (p 0.05).
     - Full reset after about 800 bars.
   - **Mix:** 75 BPM default, an 8-bar global loop, instrument bus chorus into HPF into 1 kHz LPF into 8.5 s reverb, master compressor at −15 dB and 7:1.
2. **jacbz/Lofi** ([code](https://github.com/jacbz/Lofi)), Apache-2.0. A VAE produces key, mode, BPM, energy, valence, chords (scale degrees, 1 per bar), and melodies (8 steps per bar). Production is **deterministic** given those parameters.
   - BPM is rounded to a multiple of 5 within 70–100.
   - Presets are chosen by energy and valence (§4).
   - Swing is on only 10% of the time.
   - Main section = progression repeated ceil(24 / len) times.
   - Drums are left out of the first bar and the last bar of each pass.
   - The outro is the first 2 chords faded out.
   - Rain is used if Aeolian and sad, otherwise vinyl.
   - Chords are diatonic triads promoted to the 7th-chord alias (e.g. "maj7"), with the lowest note inverted up an octave.
3. **lofi-engine** ([docs](https://mintlify.wiki/meel-hd/lofi-engine/technical/music-generation)). Rule-based Tone.js.
   - Allowed-next-chord table (§2.1).
   - Progressions of 4–8 chords.
   - 7-note extended voicings: root stays at the bottom, upper notes shuffled and stacked ascending.
   - Melody note weights by chord tone: root 0.10, 3rd 0.30, 7th 0.20, 5th 0.15, extensions about 0.025–0.05.
   - Piano LPF at 1 kHz, hats LPF at 2.4 kHz.
4. **wheelibin/synaesthesia** ([code](https://github.com/wheelibin/synaesthesia)). A generic Tone.js generator that picks key, progression, bass line, drum pattern, BPM, and swing per piece. Synths only, no samples.
5. **ML projects:** lofi-bytes (LSTM or Music Transformer on audio-to-MIDI), Melo04 (LSTM). These are less useful for firmware, but they confirm the community treats lo-fi as **chords + melody + fixed drum loop + texture**.

**Song-to-song variation in practice.** Every project picks the global parameters once per song: key, mode, tempo, preset or instrumentation, texture, and progression. Within a song, variation is mostly **layer mute/unmute on 2–8 bar boundaries** plus small melody changes. None of them does real multi-section form. That is the gap a 5-minute generator has to fill.

---

## 8. Design implications for a generative FM-1 firmware

All parameter values below are **[inferred]** design recommendations drawn from the sourced facts above.

### 8.1 Global song parameters (drawn once per song, constrained to differ from the previous song)
- **BPM:** 68–96, triangular distribution peaking at 78–82. Allow a "Nujabes" mode at 90–100 with p ≈ 0.15. Quantize to integers.
- **Feel:** half-time (snare on beat 3 of a 16-step bar, as in Magenta P0) vs. standard backbeat (2 and 4) at 50/50.
- **Meter:** 4/4 always. Tracks are built from 4-bar phrases and 8-bar sections.
- **Key:** any of 12 tonics. Mode weights: Ionian 0.30, Dorian 0.25, Aeolian 0.25, Lydian 0.10, Mixolydian 0.10. Avoid Phrygian and Locrian.
- **Global swing (16th):** 52–62%, centered at 56%. Use 0–2% independent offsets per drum voice. About 10% of songs are straight (50–52%), matching jacbz.
- **Instrumentation preset:**
  - Chords: EP-FM 0.6 / felt piano 0.25 / pad 0.15.
  - Lead: kalimba/bell, muted guitar, EP octave, flute-ish, or none.
  - Bass: sub or upright.
  - Texture: vinyl or rain or both.
- **"Dust" macro (0–1):** scales LPF cutoff, wow depth, crackle, and bit reduction together, like the SP-404 VinylSim's FREQUENCY, NOISE, and WOW FLUT.
- **Song-to-song novelty rule:** store the last N = 3 songs' feature vectors (tempo bucket, mode, progression ID, chords instrument, lead instrument, drum pattern family, texture, form template). Reject or redraw candidates whose Hamming distance from the previous song is below K, e.g. at least 4 of 8 features differ, and the tempo differs by at least 6 BPM.

### 8.2 Form (5 minutes ≈ 88–112 bars)
- **Compute bar count:** `bars = round(300 s × BPM / 240)` = 87.5 at 70 BPM, 100 at 80, 112.5 at 90. Snap to a multiple of 4, then lay out sections as multiples of 4, mostly 8.
- **Use 2 harmonic "movements"**, each about 2–2.5 minutes, the length of a real track. B differs from A in at least one of: progression, lead instrument, key (up a 4th, or relative major/minor), or drum pattern. A third short "coda" movement is optional.
- **Example template** (100 bars at 80 BPM):

| Section | Bars | Contents |
|---|---|---|
| Intro | 4–8 | noise + filtered chords |
| A1 | 16 | drums + bass + chords |
| A2 | 16 | + lead |
| Break | 4 | keys + noise only |
| B1 | 16 | new progression or lead |
| B2 | 16 | |
| A3 or B3 | 8–16 | |
| Outro | 4–8 | filter close + fade, or tape stop |

- **Layer entries and exits fall only on 4- or 8-bar boundaries.** Add or remove at most 1–2 layers per boundary.
- **Drum drop-outs:** at each 8-bar boundary, drop the drums for the last 1–2 bars with p ≈ 0.2–0.3. Use the Magenta Markov values: on→off 0.2, off→on 0.95, evaluated every 2 bars inside "busy" sections only.
- **Transitions** (pick one per section change): a low-pass sweep over 4–8 bars; a drum drop with a reversed-swell or noise rise; a 1–2 beat tape stop into the downbeat; or a vinyl-only gap of half to 1 bar.
- **Endings:** filter close plus fade over 4 bars (≈50%), tape stop of 2–4 beats (≈25%), last chord ringing with noise tail (≈25%). The noise stops half a bar after the music.

### 8.3 Harmony generator
- **Progression length:** 4 chords at 1 per bar (p 0.6), 2 chords at 2 bars each (p 0.25), 4 chords at 2 bars each or an 8-chord loop (p 0.15). The phrase is always 4 or 8 bars.
- **Build from a weighted library of degree templates,** not a free Markov chain:
  - ii–V–I–vi
  - I–vi–ii–V
  - IV–iii–vi–(v or I7)
  - ii–V vamp
  - IVmaj7–ivm7–I
  - vi–ii–V–I
  - i–iv–VII–III
  - VI–i–VI–v
  - i–III–iv–v

  Allow substitutions with small probabilities: secondary dominant (VI7 before ii), tritone sub, borrowed iv. Keep lofi-engine-style allowed-next tables as a fallback generator.
- **Chord quality:** always 7th or richer. Weights: 7 → 0.35, 9 → 0.45, 11/13 → 0.2 (13 on dominants, 11 on minors).
- **Voicing:**
  - 4 notes in C3–G4 (MIDI 48–67).
  - Rootless or shell when the bass plays the root.
  - Voice-lead to minimize total movement from the previous chord.
  - The top voice moves by a step or less where possible.
- **Comping:**
  - Roll each chord with 10–60 ms between notes.
  - Delay the whole chord by an 8th with p ≈ 0.3–0.4.
  - Hold to the bar end (legato).
  - Velocity 50–90 with ±10 random.

### 8.4 Rhythm generator
- **Grid:** 16 steps per bar, apply swing, then per-voice microtiming:

| Voice | Offset |
|---|---|
| Kick | 0 to −8 ms, ±4 ms jitter |
| Snare | +10 to +30 ms (a "drunk" setting allows +65), ±8 ms |
| Hats | +0 to +70 ms, ±10 ms |
| Bass | follows the kick |

- **Pattern families** (one per section, mutate per 4 bars):
  - Kick on 0 plus optional kicks on {7, 10, 15}, at p 0.2–0.5.
  - Snare on {4, 12} (or {8} in half-time).
  - Hats on 8ths with 16th ghosts at p 0.2.
  - Optional rim or shaker layer.
- **Velocities:** main hits 90–110, hats alternating ~70/45, ghosts 25–45.
- **Fills:** the last bar of an 8-bar section gets a variation with p ≈ 0.3: an extra kick, a snare flam, or the drums dropping out.

### 8.5 Bass and melody
- **Bass:** root-based, with jacbz's 5 patterns as the core: whole; 2 halves; 3+1; 3.5+0.5 push; tresillo 1.5+1.5+1. Add an approach note (chromatic or 5th) into the next bar with p 0.2. LPF 200–400 Hz, mono.
- **Melody:**
  - Sparse 8th-note grid, rests on beat 1 more often than not.
  - Phrase as 2-bar call and 2-bar response.
  - Note choice weighted by chord tone: 3rd 0.30, 7th 0.20, 5th 0.15, root 0.10, 9th/extensions 0.25 combined (adapted from lofi-engine).
  - Range about C5–G6 for bell or kalimba leads.
  - Repeat motifs with varied endings. The lead is absent in about 40% of a song's bars.

### 8.6 Sound engine and FX (fits the MCU budget)
- **Synthesis:** FM voices for EP (algorithm-5 style: 3× 2-op, ratio 1:1 body and 1:14 tine with fast tine decay, velocity mapped to index), bass, kalimba or bells, and pad. Procedural noise for crackle (Poisson impulses, ~5–30 per second, band-pass ~1–6 kHz), hiss, and rain. Spend samples (≤ ~150 KB) on 4–6 dusty drum hits and 1–3 optional vocal or sax chops.
- **"Lo-fi" chain** (cheapest first):
  1. Pitch wow/flutter on all tonal voices: wow 0.3–1.5 Hz at 3–15 cents, flutter 8–14 Hz at 1–3 cents, rate slowly random-walked.
  2. Tonal-bus LPF at 1.5–5 kHz depending on "dust" (SP-404 FREQUENCY analog); drum LPF 2.5–8 kHz; intro and outro sweep from ~400 Hz up to the target.
  3. Drums and samples at 26.04 kHz, 12-bit, drop-sample pitching (SP-1200).
  4. Soft-clip saturation on drums and master.
  5. Kick-triggered duck of −2 to −4 dB on chords and bass.
  6. Small dark reverb at 15–30% wet on keys and lead.
  7. Mono-ish output.
- **Noise levels:** crackle peaks −24 to −18 dBFS; hiss −45 to −36 dBFS; rain −30 to −22 dBFS. Raise noise 3–6 dB in breakdowns and the intro.

### 8.7 The 3–5 rules that matter most
1. **Two movements, not one long loop.** Real tracks are about 2.5 minutes (median 2:26, n=117), so a 5-minute song should be two related 2–2.5 minute pieces joined by a genre-native transition.
2. **Everything changes only on 4- or 8-bar boundaries.** Use 1-chord-per-bar 4-chord loops, add or remove 1–2 layers per 8 bars, and drop the drums for 1–2 bars with p ≈ 0.2–0.3.
3. **The feel lives in microtiming:** 56% ± 4 swing, snare 10–30 ms late, hats up to 70 ms late, chords rolled 10–60 ms and sometimes an 8th late, velocities 25–110.
4. **FM fits the core timbres** (EP, bass, bells, pads). Keep samples for drums and optional chops, and make crackle and rain procedural.
5. **Per-song novelty is enforced, not hoped for.** Draw global features once per song and require at least 4 of 8 to differ from the last song, plus at least 6 BPM of tempo difference.

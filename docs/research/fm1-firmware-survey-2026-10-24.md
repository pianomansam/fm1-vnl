# FM-1 firmware survey for the instrument spike (2026-10-24)

> The research behind the instrument spike: acoustic piano, a string-ensemble lead, a cello lead, and strummed
> guitar. It builds on [`other-firmwares.md`](other-firmwares.md), [`fm1-accompaniment-code.md`](fm1-accompaniment-code.md)
> and [`logue-units.md`](logue-units.md), and doesn't repeat them. Clones are in `ref/<name>` (gitignored, read only,
> never built). License shorthand: **MIT, Apache, BSD and MPL code is reusable with credit** (MPL file by file, the same
> license as VNL-1). **GPL means ideas only.** Sample libraries carry their own licenses, listed separately.

## Summary

| Instrument | Best source | License | Verdict |
|---|---|---|---|
| Acoustic piano | **Erik37** (genkigenki): a sampled grand on fm1-base | MPL-2.0 code; Salamander samples CC BY 3.0 | Sampled, in LOF-1's "DECAY" form (an attack plus a short loop, faded by the envelope). Use 4-bit ADPCM at 16 kHz with 5-6 zones, about 30-40 KB. Base the engine on Erik37's (MPL, the same license as ours). |
| String-ensemble lead | **Shine On** (zvenson) for the recipe; **Plaits' Ensemble** (MIT, already in `ref/mutable-dreams`) for code | GPL / MIT | Synthesized. Two or three detuned saws per note, a fixed formant filter on the lead bus (not per voice), and the three-line, 120-degree ensemble. No flash cost. |
| Cello lead | **GHOULBOX** hurdy-gurdy (recipe) plus Shine On's CELLO register | GPL (ideas) | Synthesized. A band-limited saw with bow pressure setting the brightness, rosin noise, two or three fixed body resonances, delayed vibrato and a slow wander. One voice, cheap. A FluidR3 cello sample (4 KB, MIT) is the fallback to A/B against. |
| Guitar strumming | No firmware does real down/up strokes. **FM1-GM**'s FluidR3 nylon sample (MIT) for the sound | MIT samples | A sampled nylon guitar with **one voice per string** (six at most, so no voice stealing), and a strum engine of our own: a 16-step mask with a direction bit, down or up, and a slow or quick spread. About 20-30 KB of samples. |

**New firmwares since the earlier survey that matter here:** Erik37, Flyfish, FM1-GM, Polly, FMEL-1, Shine On, ESTAMPIE
and GHOULBOX (bowed sounds), Frodhes, demake-synth, and the new Plaits engines inside Mutable Dreams. Most of the
rest of the field (about 120 firmwares now) are grooveboxes, games or drone machines.

**Surprises:**

- Two firmwares already put **General MIDI sets** on the FM-1 with permissive sound licenses: Polly and CTL-1 use
  Sonivox EAS (Apache-2.0), and FM1-GM uses FluidR3 (MIT). A whole GM piano, cello, strings or nylon guitar costs
  2.5-23 KB in them.
- **Erik37 is MPL-2.0 and built on fm1-base**, like VNL-1. Its piano code can be taken file by file.
- **VNL-1 itself is listed** in the fm1-editor.com directory ("pianomansam/fm1-vnl, lo-fi hip hop firmware on fm1-base;
  source only").
- LOF-1's samples play at **23,777 Hz** in its generated header (`lof_samples.h`). They are not at 16 kHz; 16 kHz is
  only the tool's `--rate` option and the rate of its one-shot pads.

## How the field was searched

- `ref/awesome-fm-1` pulled. It was already current (last commit 2026-10-16, 179 GitHub links).
- [FM-1 Firmware Hub](https://thefirmwarehub.com/) (about 80 entries, most without descriptions) and the
  [fm1-editor.com directory](https://fm1-editor.com/firmware/) (the most complete list, with a watch list of
  unreleased work).
- GitHub search (`gh search repos`): "fm-1", "fm1 firmware", "m-vave fm-1", and instrument words (piano, guitar,
  strum, strings, cello, lofi). The instrument words found nothing new; the directories did better.
- Eight Bells' list of 40 repos, checked against the above.
- Cloned into `ref/` for reading: shineon, fm1-polly, fm1-ctl, fm1-gm, fm1-fomni, fomajam,
  ghoulbox-fm1-dungeon-synth, Felucca, Sloop_ALG02, fm1-x0x-rmx, noiz_demons, Felucca_8_patterns, demake-synth,
  espada, ChoralRootFM1, fm1-fimba, FM-1-TripDeck, lunar-modulator, tessera, frasil, flyfish, erik37, frodhes, fmel-1,
  estampie, Fantasma, fm1-chord.

## Our budget, for the numbers below

- **Flash:** an image budget of 409,600 B, with about 193 KB used, leaves **about 207 KB**. fm1-base also offers big
  data beyond the store (`plat_data*`, `PLAT_XDATA0/1`, base/README.md). That is probably how FM1-GM fits 430 KB of
  samples and Polly 200 KB. If samples outgrow the image, check how much room is there.
- **RAM:** 81,920 B, plus a 307,200 B pool. **CPU:** about 0.97 of the budget on the heaviest song, so every new
  voice has to pay for itself or replace one.
- **The ADPCM rule of thumb:** 4-bit IMA ADPCM costs half a byte per sample. That is **8,000 B a second at 16 kHz**,
  7,350 at 14.7 kHz and 11,025 at 22.05 kHz. Erik37's block ADPCM is 4.25 bits a sample (17 bytes per 32 samples).
- **What existing voices cost:** our Karplus-Strong nylon (M_PLUCK) is **0.148 for 4 chord voices** (docs/cost.md).
  Two measured comparisons favor samples:
  - Felucca's host baseline (`ref/Felucca/tests/cpu_baseline.txt`, instructions per 44.1 kHz sample): SAMPLE PIANO
    468, PHYS PLUCK 840, ANALOG STRINGS 1042, PHASE STRING 1311. Playing a sample costs about half of a plucked
    string model.
  - Erik37's prototype, in host instructions per voice per sample: sampled 105, hybrid 258, modeled 322. Its
    firmware voice is about 110 pi32v2 instructions.

---

## 1. Acoustic piano

### Sources

| Firmware | Maker, link | License | Technique | Size |
|---|---|---|---|---|
| **Erik37** | M. Geisler (genkigenki), [github.com/genkigenki/erik37](https://github.com/genkigenki/erik37) | **MPL-2.0**, on fm1-base; samples Salamander Grand V3, CC BY 3.0 | 18 zones, 3 velocity layers crossfaded during the attack, then the dominant layer; looped after the attack; block ADPCM (32 samples in 17 bytes, 16 predictor filters); cubic Hermite interpolation; per-key two-slope decay measured from the recording; damper release in two stages with release noise; brightness that follows velocity and darkens over time; half pedal; sympathetic resonance (12 string-tuned combs at quarter rate); 8-line FDN room at half rate | ~380 KB; 20 voices; about 110 instructions per voice per sample |
| **Flyfish** | delirioux, [github.com/delirioux/flyfish](https://github.com/delirioux/flyfish) | GPL-3.0 (Tuna/Felucca fork); samples bitKlavier Steinway D, CC BY 4.0 | 13 notes every 6 semitones (no note moves more than 3), one velocity layer (VEL to AMP does the dynamics), kept by register: 3.1 s in the bass down to 0.7 s at the top; IMA ADPCM at 22.05 kHz; a sympathetic-string pedal resonator tuned to the harmony (`tuna_reso`) | 243 KB |
| **Felucca** SAMPLE engine | Leo Kuroshita (Hügelton), [github.com/hugelton/Felucca](https://github.com/hugelton/Felucca) | GPL-3.0; samples VCSL Kawai grand, CC0 | 5 zones; 4-bit IMA ADPCM at 22.05 kHz; the ADPCM state stored at the loop start so loops restart exactly | Felucca's sets total 143 KB |
| **LOF-1** | RedtailWorks, `ref/fm1-HYP1/lof1` | GPL-3.0; samples VCSL/VSCO-2 CE, CC0 | "DECAY" kind: a 0.55 s attack plus a 0.12 s loop, faded by the engine's envelope. UPRIGHT is the Knight upright's felt-soft layer, 4 zones an octave apart (A1-A4); UPRIGHT2 is the VSCO upright; FMPIANO is a sampled TX81Z | 5,360 B per zone (10,720 samples at 23,777 Hz); 21 KB for the upright |
| **SLOOP's LOFI KEYS** (as documented in FMEL-1) | isod89 / AntonioAltea | GPL; samples VCSL Steinway B, CC0 | 4 keys, 1 s each at 22.05 kHz, looped, played an octave up; a few bits taken off, light drive, a gentle low-pass, a 2.5 s decay, a slow pitch wobble (±19 cents at 0.37 Hz) | ~44 KB |
| **FM1-GM** | jartificer, [github.com/jartificer/fm1-gm](https://github.com/jartificer/fm1-gm) | GPL code; **FluidR3 GM samples, MIT** | One sample for the whole keyboard (Yamaha Grand at key 62): a 0.16 s attack spliced into a loop from the steadiest part, the loop's fade removed band by band so the envelope does the fading; 14.7 kHz 4-bit ADPCM; 8-point windowed-sinc interpolation, falling back to linear under load | **3.4 KB** |
| **Polly / CTL-1** | Charles Vestal, [fm1-polly](https://github.com/charlesvestal/fm1-polly) | GPL app; **Sonivox EAS and its GM set, Apache-2.0** | 11 tiny looped zones, 8-bit, 22.05 kHz: a 1990s phone-GM piano | 8.7 KB |
| Frodhes | Leonardo Mirabal, [github.com/leonardoMirabal/frodhes](https://github.com/leonardoMirabal/frodhes) | GPL | A modeled Rhodes (hammer, tine, tonebar, pickup), vibes and marimba; 9 voices with all effects took about 4.1 of 5.8 ms per block, measured | no samples |

Erik37's [`SOURCES.md`](../../ref/erik37/SOURCES.md) is the best study of free piano samples. It recommends
**Salamander Grand V3** (Yamaha C5, CC BY 3.0, 16 layers, release noises included). Its runner-up is the **VCSL
Steinway B** (CC0, 3 normalized layers). It also names a CC0 **FreePats Kawai upright** and the CC0 Karoryfer
**Osiris** piano (soft only), both more lo-fi in character. Erik37 also tried a hybrid (the sampled attack, then up to
24 measured decaying partials, `erk_modal.c`, MPL) and found it 2.5 times the CPU and about 40 KB more RAM. Samples
won.

### What to borrow

- **From Erik37 (MPL: code we can take):**
  - the per-key two-slope decay (a fast first decay, then the aftersound, faster in the treble);
  - the damper's two-stage release with its noise (no dampers on the top keys);
  - brightness from velocity, darkening over the first second;
  - the layer handover after about 0.22 s, so only one stream runs after the attack (which halves the cost);
  - mono samples spread by key with constant-power pan.
  It is fixed-point (int32), so the arithmetic needs a float port.
- **From LOF-1 (the idea):** the DECAY kind keeps a piano to about 5 KB per zone at lo-fi rates. The engine's envelope
  does the decay, so a key can ring as long as the song wants.
- **From FM1-GM (the idea; its tool is GPL):** take the loop from where the tone is steadiest, and remove the loop's
  fade octave band by octave band. Otherwise the top brightens again at every pass and sounds like a re-strike.
- **From SLOOP's LOFI KEYS (the idea):** the lo-fi treatment is cheap and familiar: bits off, a little drive, a
  low-pass, a 2.5 s decay, a slow wobble. Our per-instrument tape warble covers the wobble.

### Verdict

**A sampled upright or grand in the DECAY form, at 16 kHz with 4-bit IMA ADPCM, 5-6 zones.** The lo-fi piano is
dark, short-sustained and wobbly, so a high rate, velocity layers and long natural tails buy little.

| Option | Flash |
|---|---|
| 6 zones (C2 to C6, every 7-8 semitones, no key more than ±4 from its root), 0.6 s attack plus 0.1 s loop at 16 kHz | 6 × 0.7 s × 8,000 B = **33.6 KB** |
| The same with a 1.2 s natural decay before the loop | 6 × 1.3 s × 8,000 B = **62 KB** |
| 4 zones an octave apart (LOF-1's layout) | **22 KB** |
| FluidR3's single sample (3.4 KB, MIT) | the floor; it chipmunks at the ends of the range, so only for a quick A/B |
| Flyfish or Erik37 sized banks (243-380 KB) | out of reach |

- **CPU:** a voice is a decode, an interpolation and a one-pole filter. That is about 110 instructions per sample at
  44.1 kHz in Erik37, roughly half a Karplus-Strong voice. At our keys' half rate it costs less still. Replacing FM
  keys voices with piano voices, rather than adding them, keeps us inside 0.97.
- **Sample choice:** Knight upright (VCSL, CC0, LOF-1's pick) or FreePats Kawai upright (CC0) for the lo-fi upright.
  Salamander (CC BY 3.0) for a grand.
- **Our own tool:** write the packer ourselves. Python, CC0 sources, resample, cut the attack, find the loop, encode
  ADPCM and store the state at the loop start. GPL tools stay out.

---

## 2. String-ensemble lead (a section playing the melody)

### Sources

| Firmware | Maker, link | License | Technique |
|---|---|---|---|
| **Shine On** | zvenson, [github.com/zvenson/shineon](https://github.com/zvenson/shineon) | GPL-3.0 | Per keyer, phase-locked 16', 8' and 4' saws plus an 8' square (PolyBLEP). The registers are **fixed formant filters on the summed footages, not per voice**: VIOLIN is the 4' saw, high-passed at 600 Hz and low-passed at 5 kHz; VIOLA is the 8' saw with a 250 Hz high-pass, a 2.5 kHz low-pass and a 1.2 kHz formant band at 3/8; CELLO is the 8' saw, a 1.8 kHz low-pass plus a 400 Hz body band at 1/2. A triangle vibrato with a delay and fade-in. The **ensemble** is one bucket-brigade line read at three taps, swayed by a slow LFO (0.6 Hz, up to ±4 ms) and a fast one (6 Hz, a tenth of the depth), 120 degrees apart, with 4-point Hermite reads. A 6-stage phaser follows. Fixed point throughout. A **CPU guard** drops the oldest keyers when a block takes over 4,000 of its 5,800 µs and adds them back one at a time when it falls under 3,000. Four sounds at once cost about twice one. |
| **Plaits StringMachineEngine and Ensemble** (in Mutable Dreams, `ref/mutable-dreams/.../plaits/dsp/engine2/string_machine_engine.*`, `fx/ensemble.h`) | Emilie Gillet; vendored by kidbee85-beep | **MIT** | A divide-down string synth (`StringSynthOscillator`, registration-blended footages), two SVFs and the **Ensemble**: one 1024-float buffer (4 KB), two LFOs (0.75 Hz and 6.57 Hz), three taps 120 degrees apart, six interpolated reads per stereo sample, in float. The engine plays chords; for a melody, take the oscillator and the Ensemble. |
| Felucca ANALOG "STRINGS" / PHASE "STRING" | Hügelton | MIT (ANALOG, in Felucca-Engines) / GPL (PHASE) | A generic VA pad: 1042 and 1311 host instructions per sample per track. More expensive than Shine On's shared-filter idea. |
| FM1-GM Strings (FluidR3) | jartificer | **MIT samples** | One 0.58 s sample with a 0.4 s loop at 14.7 kHz: **4.3 KB**. Tremolo Strings is the same sample with tremolo. |
| Polly / CTL-1 Strings (EAS) | Charles Vestal | **Apache-2.0** | 2 zones, 8-bit, 22.05 kHz, long loops: **22.7 KB**; the same sample as Slow Strings and Synth Strings. |
| FMEL-1 | AntonioAltea, [github.com/AntonioAltea/fmel-1](https://github.com/AntonioAltea/fmel-1) | GPL code; **tapes "free, but not for commercial software"** | Real Mellotron M400S tapes (FLUTE, CHOIR, CELLO, VIBES; 12 zones each), ADPCM, with tape effects. The lo-fi string sound itself, but **not usable**: the tape terms can't sit under MPL, which allows commercial redistribution. |

### What to borrow

- **The structure (Shine On, as an idea):** per note, an 8' and a 4' saw (or two or three 8' saws a few cents apart,
  for a section). Sum them on the lead's bus, then run one fixed register filter there: the VIOLIN or VIOLA EQ above,
  three SVFs. Then the ensemble. Only the oscillators are per voice, and the lead plays one or two notes at a time.
  This is cheap.
- **The ensemble (Plaits, MIT code):** port `fx/ensemble.h` to C99. It is float already and needs 4 KB of pool. It
  is the Solina sound: two LFOs and three taps 120 degrees apart. Our Rings chorus port (`app/engine/rings_fx.c`) is the
  same family; the Ensemble adds the fast LFO and the third tap, which turn "chorused synth" into "section".
- **Section feel:** a slower attack (80-200 ms), vibrato that fades in after about 300 ms, and an optional octave
  doubling (the 4' footage on and off). The per-instrument tape warble supplies the Mellotron-ish wow.
- **The CPU guard (idea):** shed voices when a block runs long. This is worth having anyway at 0.97.

### Verdict

**Synthesize it.** Use the Shine On recipe (GPL, rewritten) and Plaits' Ensemble (MIT, ported).

- **Flash:** about 0, plus a few hundred bytes of coefficients.
- **CPU:** 2-3 PolyBLEP saws per note for 1-2 notes, 3 SVFs and the ensemble on one bus. That is the same order as one
  of our pad voices plus the chorus. Running the ensemble at the chorus's reduced rate would roll off its top; try it
  at the keys' half rate first.
- **A/B:** FluidR3's Strings loop (4.3 KB, MIT) through the same Ensemble. Sampled strings move less (one loop), but
  the Ensemble supplies most of the movement anyway.

---

## 3. Cello lead (solo, bowed)

### Sources

No FM-1 firmware has a real bowed-string model (a bowed waveguide). The closest:

| Firmware | Maker, link | License | Technique |
|---|---|---|---|
| **GHOULBOX** GURDY engine (`firmware/src/eng_gurdy.c`) | Jason Persinger, [github.com/jasonpersinger/ghoulbox-fm1-dungeon-synth](https://github.com/jasonpersinger/ghoulbox-fm1-dungeon-synth) | GPL-3.0 | The melody string (chanterelle) is a band-limited saw (a bowed string's Helmholtz motion), **darker with less wheel pressure**, with **rosin friction noise**. A **body of two wooden resonances, about 270 Hz (box) and 1.1 kHz (nose)**, mixed against the dry string. WOBL is a slow random wander of pitch and loudness. Plus drones and the buzzing "dog". This is our cello recipe almost as it stands. ESTAMPIE (ocean) forks it. |
| **Shine On** CELLO register | zvenson | GPL-3.0 | The 8' saw, a 1.8 kHz low-pass, plus a 400 Hz body band at half level. |
| Felucca PHYS STRNG / FOMAJAM STRING with BOW | Hügelton (DaisySP port, MIT) / konsumer (GPL) | MIT / GPL | Karplus-Strong kept alive by noise ("bowed by random hits"). It sounds like bowed metal or a drone string, not a cello, and costs about 840-1000 host instructions per sample. Skip. |
| FM1-GM Cello (FluidR3) | jartificer | **MIT samples** | Recorded at key 58, 0.57 s with a 0.36 s loop at 14.7 kHz: **4.2 KB**. Contrabass has a longer 0.4 s loop "for its bowing". |
| EAS Cello | Sonivox | Apache-2.0 | One 561-sample loop (a single waveform). Not usable as a lead. |
| FMEL-1 CELLO | AntonioAltea | not usable (see above) | Mellotron cello tapes, 12 zones. |

### Verdict

**Synthesize it: one voice, the GHOULBOX and Shine On recipe written fresh.**

- **The bow:** a PolyBLEP saw. Bow pressure, from velocity and an envelope, sets the brightness through a one-pole
  low-pass. Add a burst of band-passed rosin noise at the attack (30-80 ms) and a low continuous rosin noise
  following pressure.
- **The body:** three fixed SVF band-passes (about 200-300 Hz, about 400-500 Hz, about 1.1 kHz) mixed against the dry
  saw. Like Shine On, these are fixed EQs, so they cost three filters for the one voice.
- **Life:** vibrato delayed 250-400 ms, 5-6 Hz, ±15-25 cents, fading in; a slow wander of pitch and loudness;
  portamento between legato notes (the cello's slide is half its character).
- **Register:** C3-A4, as the ledger says. Keep the keys out of 48-69 while it plays.
- **Cost:** about one pad voice. **Flash:** about 0.
- **Fallback to A/B:** FluidR3's cello (4.2 KB, MIT) played through the same vibrato and body. It is one sample
  across C3-A4, so ±10 semitones; listen for formant shift at the ends.
- **Outside the FM-1 field:** a real bowed waveguide (Smith's bowed string as in STK's `Bowed`) is the next step up
  if the subtractive cello sounds like a synth. STK's license is permissive, but confirm it before taking code.

---

## 4. Guitar strumming (nylon/acoustic comping)

### What exists

Nobody does **down/up strokes**. What there is:

| Firmware | Maker | License | Strum |
|---|---|---|---|
| FoMni | Charles Vestal, [fm1-fomni](https://github.com/charlesvestal/fm1-fomni) | GPL (voice after Chordian, **CC0**) | An Omnichord strumplate: the player swipes the white keys, each a string of the chord; a filtered synth voice with per-string shimmer offsets. Rhythm patterns are drum and chord gating. |
| fm1-chord | math0ne, [fm1-chord](https://github.com/math0ne/fm1-chord) | GPL | STRUM mode rolls a held chord at 200, 80 or 40 ms per note (`GAP_MS`), one direction only. Its ACOUSTIC GTR is a Felucca PHYS string preset (STRC 38, BRIT 66, DAMP 90 for a 1.5 s ring, POS 22, near the bridge). |
| demake-synth | InigoHernandez, [demake-synth](https://github.com/InigoHernandez/demake-synth) | GPL | A **signed strum in ms (+ up, − down)**, fixed per setting, not per stroke. Voicings CLOSE, OPEN (drop-2), WIDE, SHELL and QUARTAL, inverted for the least motion. Rhythms HOLD, PULSE4, PULSE8, ARPUP, ARPDN. |
| Polly | Charles Vestal | GPL styles | Strums written out by hand in the style files: country's guitar rolls 4 notes 20 ticks apart (about 22 ms at 114 BPM), all downstrokes; bossa's nylon comps 3rd-7th-9th voicings on a two-bar grid. |
| Felucca 1.6.1 STR3/STR7 | Hügelton | GPL | The black keys pick the chord and the white keys are its strings; CHORD packs have a STRUM speed (x 5 ms). |
| LOF-1 | RedtailWorks | GPL | A rolled comp at 4-49 ms per note with jitter (already in `fm1-accompaniment-code.md`). |
| Rings `strummer.h` (in Mutable Dreams) | Emilie Gillet | MIT | Detects strums from trigger and pitch changes. It is not a pattern engine, so it doesn't apply. |

**Guitar sounds available:** FluidR3 Nylon String Guitar (FM1-GM, **MIT**): one sample at key 50, 0.55 s, its loop
taken from at least 0.8 s in, past the pluck. That is **4.0 KB** at 14.7 kHz; Steel String is 4.5 KB. EAS nylon is
2 zones, 8-bit, 2.5 KB (Apache). LOF-1 used the VCSL **Strumstick** (CC0) as its "guitar". Our own Karplus-Strong
nylon exists but costs 0.148 for 4 voices. A 6-string strum needs 6, which would be about 0.22.

### Verdict

**A sampled nylon guitar with one voice per string, and a strum engine we write.**

- **Voices = strings.** Six fixed voices, one per string of the voicing. A new stroke re-strikes a string by
  retriggering its voice, which chokes the old note as a real string does. Polyphony is capped at 6 by construction,
  with no stealing logic. A string a stroke skips (an upstroke's low strings) rings on.
- **Patterns:** a 16-step mask with, per hit, **direction** (down or up), **spread**, **accent** and **muted** (a
  "chuck"). A muted hit is a short noise burst with all strings damped. That is cheap and gives the pattern its
  percussion.
  - Downstrokes play low to high on the beats.
  - Upstrokes play high to low on the "ands", hitting only the top 3-4 strings, about 3-6 dB quieter.
  - Slow strums put 30-80 ms between strings, so each one is heard; quick strums 5-15 ms.
  - Add a little jitter per string (1-3 ms) and per stroke.
  - Seed patterns from Polly's bossa and country writing and the ledger's World cycles.
- **Samples:**
  - Fastest: FluidR3 nylon, 4 KB, MIT, but one sample across the guitar's range.
  - Better: our own **4 zones × 0.8 s at 16 kHz = 25.6 KB** (CC0, if VCSL or VSCO has a classical guitar; check the
    library, as LOF-1 settled for the strumstick), plus a 0.1 s mute-noise sample (0.8 KB).
  - A pluck needs no loop: let it decay, or use a short DECAY-style loop for long rings.
- **CPU:** 6 sample voices at about half a Karplus-Strong voice each, about 0.11 by the nylon's numbers, and less at
  the keys' half rate. It replaces the keys while it plays rather than adding to them.

---

## Other things worth taking for lo-fi (brief)

- **Voicings (demake-synth, GPL ideas):** SHELL (root, 7th or 6th, 3rd, altered 5th), drop-2 OPEN, QUARTAL, and AUTO
  inversion for least motion. Quartal keys are a good lo-fi color within the no-substitutions rule.
- **Sympathetic resonance (Erik37, MPL code):** 12 string-tuned combs at quarter rate while the "pedal" is down, for
  the piano's bloom. Flyfish's harmony-tuned resonator is the same idea (GPL).
- **Drum feel data:** Polly's factory drums are excerpts of Magenta's **Groove MIDI Dataset (CC BY 4.0)**, real
  drummer performances. Their timing and velocity could seed humanization tables or ghost-note patterns, with credit.
- **Mutation radius (NOIZ DEMONS, GPL idea):** each re-roll first picks a "rank" with equal odds, which sets how far
  the new sound may move from the current one. It maps onto MAKU-style mutation for "another song like this".
- **µ-law for long one-shots (X0X-RMX, GPL idea):** G.711 µ-law for the 909 cymbals. 8 bits a sample is twice
  ADPCM's flash but cheaper to decode; not needed if ADPCM decode fits.
- **Felucca 8 Patterns** (echoe): eight sequences per track and an ARP > GEN mode, a generated loop in a scale. Nothing
  new for us.
- **Generators to read later:** Tessera (13 generators, song forms; GPL), Espada (generative tracker grooves in ten
  styles, with an arc from drums to drop; GPL), Finton (66 chord progressions; not cloned), Loom FM-1 (a chord
  progression generator; not cloned).
- **A note on floats:** Shine On says the chip has no FPU and runs fixed point. Flyfish runs floats "on the FPU
  (`-mcpu=r3 -mfprev1`)", and VNL-1 runs float. Fixed-point code we borrow (Erik37, Shine On's recipes) needs a
  float port, and the FM-1 bench, not the host, decides which is cheaper.

## License quick reference for this spike

| Source | Use |
|---|---|
| Erik37 engine (`app/dsp/erk*.c`) | **MPL-2.0: take files with their headers** |
| Plaits Ensemble, StringSynthOscillator; Rings; DaisySP | **MIT: take with credit** |
| Felucca-Engines (ANALOG, LOFI, TRIO, WHEEL, NOISE) | MIT |
| Sonivox EAS code and GM set | **Apache-2.0: take with NOTICE** |
| FluidR3 GM samples | **MIT** |
| VCSL, VSCO-2 CE, FreePats Kawai upright, Osiris | **CC0** |
| Salamander Grand V3 | CC BY 3.0 (credit in the about text) |
| bitKlavier Steinway D | CC BY 4.0 |
| Groove MIDI Dataset | CC BY 4.0 |
| Chordian (FoMni's voice source) | CC0 |
| Shine On, GHOULBOX, Flyfish, FM1-GM tools, Polly app, LOF-1, demake, all Felucca forks | GPL: ideas only |
| Mellotron M400S tapes (FMEL-1) | Not usable (non-commercial terms) |

## Credits

Erik37 by M. Geisler (genkigenki); Shine On, OberVave and Dinghy by zvenson; Polly, FoMni, CTL-1 and X0X by Charles
Vestal; FM1-GM by jartificer; Flyfish and Tuna by delirioux; Felucca by Leo Kuroshita (Hügelton Instruments); GHOULBOX
by Jason Persinger; ESTAMPIE by ocean; FMEL-1 by Antonio Altea; LOF-1 by RedtailWorks; FOMAJAM by konsumer; Frodhes by
Leonardo Mirabal; demake-synth by Iñigo Hernández; fm1-chord by math0ne; Mutable Dreams by kidbee85-beep; NOIZ DEMONS
by SPIKE the Percussionist; X0X-RMX by Mr-Roboto; Felucca 8 Patterns by echoe; Plaits, Rings and the Ensemble by Emilie
Gillet (Mutable Instruments). Directories: [FM-1 Firmware Hub](https://thefirmwarehub.com/),
[fm1-editor.com/firmware](https://fm1-editor.com/firmware/), [awesome-fm-1](https://github.com/cicloid/awesome-fm-1).

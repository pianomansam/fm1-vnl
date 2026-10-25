# Guitar sample survey for VNL-1 (FM-1 firmware)

Researched 2026-10-25 via web fetches of the actual pages. Ranking is by sound/fit first, license second (per Sam's clarification). Items marked UNVERIFIED were not confirmable from the page text I could reach (several sites rate-limited or blocked). No audio was downloaded; sizes are as listed on the source pages. Re-check license text at download time before shipping.

Device constraints in mind: mono, 16-22 kHz, 4-bit IMA ADPCM, ~200 KB flash total. At 4 bit and 16 kHz, 1 s of audio = 8 KB. So a nylon voice with ~6 zones x 0.6 s (attack + loop) is about 30 KB; a per-string strum set of ~12 notes x 0.5 s about 50 KB.

## Top 3 per category

### Nylon
1. FreePats "Spanish Classical Guitar" (CC0) - best verified license and the cleanest multisample (see A1).
2. Freesound "Nylon Guitar Single Notes" packs by j_e_f_f_g and SpeedY (CC0 per file) - per-string, already looped notes (see A2, A3).
3. University of Iowa MIS guitar (Raimundo 118 classical; "any projects, without restrictions") - three dynamics, per string, anechoic (see A4). Ranked 3rd only because there is no formal license text.
Honorable: Freesound "Nylon Plucks" by bangcorrupt (CC0, but 96 kHz stereo 4:40 file with a contradictory NC note, see trap list).

### Steel-string
Weak category: no clean CC0 steel multisample found.
1. FreePats FSS Steel-String (GPL-3.0+ with FreePats special exception; "small" bank 2.7 MiB) - best sound/fit, but license needs reading the exception (see B1). Flag: GPL.
2. Karoryfer Emilyguitar (CC0; Epiphone electric, not steel) - cannot count as steel; listed only as a stand-in via chord/strum articulations (see C3). Treat as a gap.
2b. Musical Artifacts "Acoustic Guitar" artifact 6136 (listed CC BY 3.0, GarageBand-derived SF2) - UNVERIFIED and provenance is suspicious; do not use without tracing the origin (Apple GarageBand samples are not CC BY).
3. Freesound one-shots/chords (CC0): "Acoustic Japan Guitar FG-151. G." by Black_River_Phonogram, "AcousticGuitar-C-Chord" by spitefuloctopus, "Acoustic guitar chords" by dmj. Individual chords only, quality varies. UNVERIFIED steel-ness of each.
Recommendation: for a steel-ish comp, either accept GPL-with-exception from FreePats, or record/derive from the CC0 nylon sets with brighter EQ.

### Jazz / clean electric
1. FreePats "Clean #2 (Jazz)" electric (CC0): Fender-type guitar through amp/FX rack, bridge pickup only; "small" bank 2.4 MiB FLAC (see C1). Best verified license + small size.
2. Karoryfer Black and Green Guitars (Gretsch Anniversary hollowbody + Hofner Club; CC0-1.0 repo, "royalty-free commercial and non-commercial"; ~500 MB, SFZ, hollowbody) - best tonal fit for "jazzy hollowbody" but huge; extract a few notes only (see C2). Also Karoryfer Shinyguitar archtop (CC0-1.0, 352 MB; DI pickup + mic blend) is the true archtop (see C4).
3. FreePats "Clean #1" (CC0, same Fender, amp-processed; small bank 3.0 MiB) and Karoryfer Emilyguitar (CC0, direct from both pickups, includes chord and barre articulations, muted release noises) (see C3).

## A. Nylon candidates

### A1. FreePats Spanish Classical Guitar  (top pick)
- URL: https://freepats.zenvoid.org/Guitar/acoustic-guitar.html
- License: CC0 1.0 (stated on the page; license: https://creativecommons.org/publicdomain/zero/1.0/)
- Type: nylon, Spanish classical, recorded 2008 with AKG Perception 120, noise-filtered by the author ("conditions far from ideal").
- Content: multisampled melodic notes (the page does not detail the layers). Formats: SFZ FLAC 4.5 MiB, SFZ WAV 6.9 MiB, SF2 9.5 MiB.
- Quality: unreviewed; author admits non-ideal recording, so expect some noise. Fine for 4-bit lo-fi.
- (a) Lead: good, tiny source. (b) Comping: single notes only; strum must be synthesized from notes (per-string layout not documented, check SFZ key ranges).

### A2. Freesound "Nylon Guitar Single Notes" by j_e_f_f_g (15 sounds)
- Pack: https://freesound.org/people/j_e_f_f_g/packs/20356/  Example sound: https://freesound.org/people/j_e_f_f_g/sounds/361587/
- License: CC0 on the sound page (pack page itself states no license; license is per file).
- Type: nylon. File names encode string and note (5_Gb, 6_Db, 4_A, 2_A...). Each described as "Looped, nylon string guitar note"; 44.1 kHz 16-bit stereo, ~1.5 s, ~267 KB each.
- (a) Lead: good, pre-looped, one note per zone. (b) Comping: per-string notes suit a strum engine (strum = staggered note triggers). Coverage is only ~15 notes, so pitch-shift for gaps.

### A3. Freesound "Nylon Guitar Single notes" by SpeedY (another pack)
- Example: https://freesound.org/people/SpeedY/sounds/8403/ ; CC0 on the page; 44.1 kHz 16-bit mono, ~16 s per file (1.4 MB), single plucked G-string note.
- Also "Clean_G_str_pluck.wav" and chord files ("F#min--(Low-D).wav", id 8495) by SpeedY in CC0 search results (not individually checked).
- (a) lead: decent, long decay (trim). (b) comping: per-string open/fretted notes. Older recordings (2005-era Freesound), quality unreviewed.

### A4. University of Iowa Electronic Music Studios, Guitar (Raimundo 118)
- URL: https://theremin.music.uiowa.edu/MISguitar.html ; terms: https://theremin.music.uiowa.edu/MIS.html
- License: no formal license; page says recordings "may be downloaded and used for any projects, without restrictions" and asks (not requires) to tell the director. Weak legal text for a shipped product, but permissive. Ask for written confirmation if shipping commercially.
- Type: nylon/classical (Raimundo 118), anechoic chamber, Earthworks QTC40 at 5 ft. Dynamics pp/mf/ff. Per-string folders (E, A, D, G, B, high E; ranges e.g. E2-B2, C3-B3, ...). AIFF, 16/44.1 or 24/96, mono or stereo.
- Quality: very clean, dry (anechoic) - useful, you add your own room. Full chromatic per-string coverage.
- (a) Lead: good. (b) Comping/strum: best per-string coverage of anything found; ideal source for a strum engine. Articulations (legato/gliss) promised in a future re-record, not available.

### A5. Pianobook "MG Soft Nylon Guitar (Lite)" - NOT RECOMMENDED
- https://www.pianobook.co.uk/?p=5692 . DecentSampler, D1-G3, 1 velocity, 2 round robins. Page states no license; terms are in a site Terms and Conditions page I could not load (404). Pianobook terms generally permit use in music but are not a clear grant to redistribute raw samples inside a product. Treat as no-go unless the terms are verified.

### A6. Freesound "Nylon Plucks" by bangcorrupt
- https://freesound.org/people/bangcorrupt/sounds/833157/ . Cordoba C7-CE, open D tuning (D-A-D-F#-A-D), each string open and at marked frets, piezo + mic. 4:40, 96 kHz 24-bit stereo, 153.8 MB.
- License line says CC0 but the description also says "All sounds are free for non-commercial use" and offers commercial licensing. Contradictory; see traps. Only use if the uploader confirms CC0 in writing.

### A7. MeMusic "Lo-Fi Nylon Guitars V1" (paid plugin, 3 nylon guitars, samples reportedly dedicated to public domain)
- Listing found via search: https://memusic.gumroad.com/l/nywzvs (page 404 when fetched). UNVERIFIED. Paid GBP 5; worth a look since it is lo-fi-tuned, but check the public-domain claim covers extracting WAVs.

## B. Steel-string candidates

### B1. FreePats FSS Steel-String Acoustic Guitar
- URL: https://freepats.zenvoid.org/Guitar/steel-acoustic-guitar.html
- License: GNU GPL v3 or later with a "FreePats special exception" (page says the author, Gary Campion of FlameStudios, gave permission for distribution under it). I could not retrieve the exception text; the FreePats About page (https://freepats.zenvoid.org/about.html) says FreePats licenses must "permit to distribute musical works under any license", i.e. music made with it is unencumbered. That covers songs; whether the samples inside a firmware image pull the firmware under GPL is a separate question. Read the exact exception before using.
- Type: steel (FS Seagull, from FlameStudios GIG, a reduced subset). Banks: SFZ WAV 13 MiB, SF2 12 MiB, small SFZ WAV 2.7 MiB, small SF2 2.7 MiB.
- (a) Lead: acceptable. (b) Comping: single-note multisamples; per-string layout undocumented.

### B2. Freesound CC0 acoustic chord one-shots (steel-ish, UNVERIFIED)
- AcousticGuitar-C-Chord (spitefuloctopus) https://freesound.org/people/spitefuloctopus/sounds/315706/ ; Acoustic guitar chords (dmj) https://freesound.org/people/dmj/sounds/441969/ ; Acoustic Japan Guitar FG-151. G. (Black_River_Phonogram) https://freesound.org/people/Black_River_Phonogram/sounds/420954/ ; eqavox https://freesound.org/people/eqavox/sounds/683953/
- All appeared under the CC0 filter. Good for a few whole-strum hits (one-shots are cheap at 4-bit). Quality not reviewed.

### B3. Musical Artifacts "Acoustic Guitar" 6136 (listed CC BY 3.0) - see warning above; page returned 403.

### Not found
No CC0/CC BY steel-string per-string multisample found on Freesound (CC0 filter "steel string acoustic guitar note" returned 0), OpenGameArt, or archive.org.

## C. Jazz / clean electric candidates

### C1. FreePats Clean Electric Guitar: "Clean #2 (Jazz)" and "Clean #1"  (top pick)
- URL: https://freepats.zenvoid.org/ElectricGuitar/clean-electric-guitar.html
- License: CC0 1.0 (http://creativecommons.org/publicdomain/zero/1.0/), stated on the page. Versions 2026-08-07 (Clean #1/#2), 2022-09-11 (Direct).
- Type: Fender electric (basic model), through amp + effects rack for a clean tone; bridge pickup only (no neck pickup tone, so it is brighter than a true jazz tone; can be EQ'd/low-passed, which also helps with 16 kHz). "Direct" bank is the dry DI signal.
- Sizes: Clean #2 Jazz small: SFZ FLAC 2.4 MiB (SFZ WAV 3.7, SF2 5.0); full: 63 MiB FLAC. Clean #1 small: 3.0 MiB FLAC; full 118 MiB FLAC. Direct: 74 MiB FLAC.
- Content per page: note multisamples (layer/velocity detail not given on page; check SFZ).
- (a) Lead: very good, CC0, tiny. (b) Comping: single notes; build chords/strums from the notes.

### C2. Karoryfer Black and Green Guitars (hollowbody electrics)
- Repo: https://github.com/sfzinstruments/karoryfer.black-and-green-guitars ; shop: https://shop.karoryfer.com/pages/free-black-and-green-guitars (rate-limited when fetched)
- License: repo sidebar CC0-1.0; README: "Royalty-free for all commercial and non-commercial use" (Karoryfer Lecolds 2022). Both are permissive; the SFZ listing site also says CC0-1.0.
- Type: green Gretsch Anniversary + black Hofner Club (hollowbody/semi), recorded by Brian Wood. About 500 MB, SFZ for Sforzando. Articulations not described on the README; Karoryfer libraries typically have round robins and release noises (verify).
- (a) Lead: probably the best hollowbody tone found. (b) Comping: likely contains muted/noise samples; you would hand-pick notes.
- Warning: huge download; you only need ~100 KB.

### C3. Karoryfer Emilyguitar (Epiphone Emily the Strange)
- Repo: https://github.com/sfzinstruments/karoryfer.emilyguitar ; license file: https://github.com/sfzinstruments/karoryfer.emilyguitar/blob/master/LICENSE (CC0 1.0 Universal; README also says royalty-free commercial/non-commercial).
- Type: solid-body, D'Addario flatwounds, direct from both pickups, 4 velocity layers, 3 round robins, release samples (4 RR), noises (5 RR); low E also in Db tuning. SFZ articulations include power/minor/major barre chords with keyswitches ("Chords", "Chords wide"), "Clean" (no string-mute on release). Size not stated.
- (a) Lead: flatwound clean = jazz-ish; (b) Comping: the chord samples are the closest thing to ready-made strums found with a clean license.

### C4. Karoryfer Shinyguitar (archtop)
- Repo: https://github.com/sfzinstruments/karoryfer.shinyguitar (CC0-1.0 sidebar; README "royalty-free for all commercial and non-commercial use"); shop: https://shop.karoryfer.com/pages/free-shinyguitar ; listed 352 MB.
- Type: shiny black archtop, DI pickup + mic blend (CC100), 1/3/5 voices, release noise, vibrato controls. Sforzando-oriented. Use the pickup channel for clean tone.
- (a) Lead: the real archtop sound; (b) comping: single-note multisamples with release noises; no chord set stated.
- Note the README warns that editing the bank XML breaks Sforzando loading (signature); irrelevant for sample extraction.

### C5. Freesound jazz/clean phrases (CC0, loops not multisamples)
- Archtop Jazz Guitar Solo #001 (bassimat; CC0; 2:52, AI-generated with Suno, "Joe Pass style") https://freesound.org/people/bassimat/sounds/832732/ - AI-generated audio; copyright status is unclear in some jurisdictions, but CC0 as posted. Could be sliced for single notes/chords but note provenance.
- Pretty F# Dorian guitar chord w/ chorus (skylarjulian) https://freesound.org/people/skylarjulian/sounds/557714/ - fits "Rhodes-like chorus guitar" brief (not listened).
- LoFi Jazz Chords (bjporu) https://freesound.org/people/bjporu/sounds/798825/ ; Fmaj7 Jazzy (Pax11) https://freesound.org/people/Pax11/sounds/444850/ ; guitar mute pluck riff (CVLTIV8R) https://freesound.org/people/CVLTIV8R/sounds/811473/ (muted pluck - possible "chuck" source).
- All CC0 filter hits; I did not open each license or listen. Verify per file.

### C6. Blue Jeans and Moonbeams (BJAM) by Malaclypse the Younger (MIT per sfzinstruments.github.io)
- Listed at https://sfzinstruments.github.io/guitars as MIT, "electric and acoustic guitars", v4.23.04; docs: https://docs.google.com/document/d/15XBK9LqTsNRG14JD23bidiAvaEaj8UXcT8O8PDTnS3o (requires sign-in; not readable). Described elsewhere as a Strat-style with three mixable pickups plus an acoustic guitar. I could not read the license text or see the repo; UNVERIFIED but potentially the only free source with neck pickup selection and an acoustic. Worth a manual check.

### C7. SampleScience "Clean Electric Guitar" (public domain samples, but plugin)
- https://samplescience.gumroad.com/l/cleanelectricguitar (page text not retrievable). Search coverage says two electric guitars sampled note by note and dedicated to the public domain; delivered as VST/AU plugin (v2 listed at $1+, "pay what you want"). To get WAVs you would render notes from the plugin in a DAW, which is allowed if the samples are truly PD, but confirm on the author's page. UNVERIFIED.

## D. Sources checked that have no usable guitar or unsuitable terms

- Versilian VCSL (CC0 1.0): https://github.com/sgossner/VCSL . Chordophones folder contains only "Composite Chordophones" and "Zithers". No guitar. (VSCO-2 CE is orchestral; no guitar.)
- Philharmonia: https://philharmonia.co.uk/resources/sound-samples/ . Includes guitar, mandolin, banjo. Terms: free to use "as you wish" including in commercial works, but they must not be sold or made available "as is" as samples or as a sampler instrument. TRAP: a firmware that ships them as a playable multisampled instrument arguably falls into the "sampler instrument" prohibition. Avoid.
- FluidR3 / MuseScore MS General: MIT (https://github.com/musescore/MuseScore/blob/master/share/sound/FluidR3Mono_License.md); attribution required (Frank Wen etc.). Guitar presets exist but are generic GM quality (low). Note MS General's own license "still under discussion" at the time of that file (alpha). FluidR3 itself is MIT. Usable as a fallback only.
- GeneralUser GS 2.0: license summary at https://scancode-licensedb.aboutcode.org/generaluser-gs-2.0.LICENSE : free for music creation and commercial use and encourages software use, allows modification, but does not explicitly grant standalone redistribution of the samples, and author states samples came from various sources with uncertain provenance and tells commercial software developers to be aware. TRAP for firmware; gray area.
- Timbres of Heaven: Polyphone lists it as "personal use, don't share with more restrictions" (https://www.polyphone.io/en/soundfonts/instrument-sets/261-timbres-of-heaven-v3-4-final). Not usable.
- Zanderjaz soundfont archive (https://www.zanderjaz.com/downloads/soundfonts/guitars/): collection of unknown-origin SF2s; site says it created none and ownership/licensing is unknown per file. Do not use.
- Sonatina Symphonic Orchestra (OpenGameArt node 15174): orchestral; no guitar; Sampling Plus 1.0 license (not an open license).
- Unreal Instruments (Standard Guitar, Metal GTX, The Slapper): listed as "Custom" license, 0.7-1.3 GB, distorted/multi-articulation. Custom terms unchecked and a prior commenter could not find terms; skip.
- Karoryfer commercial guitars (Glockenskull, Secret Agent, Snowkiss, Surfkiss): commercial licenses, not free.
- OpenGameArt: only loops/one-shots (guitar samples 2: CC0 distorted progressions https://opengameart.org/content/guitar-samples-2 ; a CC0 single-chord strum). No multisample.
- archive.org: no relevant free nylon/steel multisample surfaced in search.
- Noisecrux Peavey Predator (23 notes E2-C6, two velocities, SFZ, direct bridge humbucker) - license not found (rekkerd.org coverage). UNVERIFIED; likely a "free" with unknown redistribution terms.
- Freesound packs generally: license is per file, not per pack. The CC0 filter on search results helps, but each file page must be checked (see Nylon Plucks).

## Licensing traps summary
1. Philharmonia: free for commercial music, but forbids offering samples "as is" or as a sampler instrument. Firmware playing them as an instrument is likely out.
2. FreePats steel guitar: GPL-3.0+ with a special exception; read the exception text; do not assume permissive.
3. Freesound "Nylon Plucks" (bangcorrupt): says CC0 and "free for non-commercial use, contact for commercial" in the same description.
4. GeneralUser GS: commercial use of music fine; standalone sample redistribution and provenance ambiguous.
5. Timbres of Heaven: personal use only. Zanderjaz archive: unknown ownership.
6. Pianobook: per-instrument page shows no license; need site T&C. MG Soft Nylon Lite unverified.
7. Karoryfer Emilyguitar/Shinyguitar/Black and Green: repo sidebar CC0-1.0 and README "royalty-free commercial and non-commercial". Treat as fine; to be safe keep a note of both statements.
8. Iowa MIS: permissive wording but no formal license text.
9. bassimat archtop solo: AI-generated (Suno), CC0 by uploader; authorship/copyright of AI output is murky.
10. Share-alike: no CC BY-SA guitar sources found; none of the recommended picks require share-alike. The one GPL candidate (FreePats steel) is the only copyleft concern.

## Suggested path for VNL-1
- Nylon comp + lead: Iowa per-string (strum engine) or FreePats Spanish classical (CC0) for lead; Freesound j_e_f_f_g notes as a CC0 backup for pre-looped zones.
- Jazz/clean: FreePats Clean #2 small bank (CC0, 2.4 MiB source) for notes; Karoryfer Emilyguitar chord articulations or Shinyguitar for color; extract only a few zones.
- Steel: either FreePats FSS after reading the exception, or fake it from nylon with EQ.

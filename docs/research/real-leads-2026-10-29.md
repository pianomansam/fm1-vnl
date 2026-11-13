# Real (sampled) versions of six synthesized leads

Question: can muted trumpet, saxophone, vibraphone, guitar lick, string lead and cello be real multisampled instruments under a license that is safe inside a GPL-3.0-or-later firmware and a local tool? Research only: file listings, READMEs and license texts were read through the GitHub API and the web; no large files were downloaded and no code was touched.

## Summary

| Lead | Verdict | Source | License |
|---|---|---|---|
| Muted trumpet | Replace | VSCO-2-CE Trumpet, Harmon-mute sustain | CC0 |
| Saxophone | Replace | Karoryfer Weresax (alto), runner-up FreePats Tenor Sax (pre-looped) | CC0 |
| Vibraphone | Replace | VCSL Vibraphone (soft and hard mallets) | CC0 |
| Guitar lick | Replace | Karoryfer Shinyguitar archtop, pickup channel | CC0 |
| String lead | Replace only if an ear A/B passes, else remove | VSCO-2-CE Violin Section "susVib" | CC0 |
| Cello | Replace | Karoryfer x bigcat Cello (solo) | CC0 |

None of the recommended sources needs attribution (CC0). The repo's CREDITS.md convention (credit anyway) can be kept. The only attribution-required option found is MTG Solo Sax (CC-BY 4.0), listed as a runner-up.

## Conventions and caveats for every table

- **Note names are not MIDI.** VCSL and VSCO file names sit one octave under standard MIDI (the Knight piano's `C#1` is MIDI 37, as the manifest already notes). Karoryfer SFZs carry the real keycenter (bigcat cello `C1` is key 24, Weresax `db2` is key 49), so read the `pitch_keycenter` in the SFZ rather than the file name. The trumpet, sax and vibes ranges below are given as file names; check key centers before mapping.
- **Loops.** The current lead format is an attack plus a crossfaded loop. None of the sources except FreePats Tenor Sax and MTG Solo Sax ships loop points in the SFZ, but all the sustained instruments here hold a steady tone for 4 to 12 s, so a loop can be cut offline. Plucked and struck sources (guitar, vibraphone) decay and need no loop.
- **Size.** Source sizes are in the repo, not what ships. After the pack step (16-bit, about 22 kHz, short attack plus loop, a handful of zones) the flash cost is a small fraction of the source size; the cost doc is the place to measure it.
- **Octave range.** Leads span about C4-C6 (MIDI 60-84), cello about B3-A4.

## 1. Muted trumpet

| | |
|---|---|
| **Best** | **VSCO-2-CE (Versilian Studios Chamber Orchestra, Community Edition), `Brass/Trumpet`**, https://github.com/sgossner/VSCO-2-CE |
| License | CC0 1.0 (repo `LICENSE`, GitHub reports CC0-1.0). No attribution needed. |
| Articulations | `harmonM-sus` (Harmon mute, sustain), `straightM-sus` (straight mute, sustain), `sus` (open), `susvib` (open with vibrato), `stac` (open staccato, 2 round robins) |
| Harmon-mute notes | 8 notes, 2 velocity layers (`v1` soft, `v3` loud), 1 round robin: A#2 C3 D#3 G#3 A#3 D4 F4 A4 (file names, so roughly 3 to 4 semitones apart, about A#3 to A5 sounding if the -1 octave rule holds). Pitch-shifting up a few semitones would reach C6. |
| Size | Harmon sustain 16 files, about 1 to 1.6 MB each (about 18 MB); whole Trumpet folder 140 files, 120 MB. |
| Loop-friendliness | Good: single long sustains per note (about 1 to 1.6 MB). No loop points shipped, cut offline. Muted tones are steady and dark, so a loop should be easy. |
| Mute / vibrato | Harmon, straight, or open; vibrato only on the open `susvib` set. |
| Runner-up | FluidR3_GM or MuseScore_General preset 59 "Harmon Mute Trumpet" (MIT; the MuseScore sample-source list shows the trumpet family as FluidR3 samples, uncredited). One looped sample set, tiny, but a single-layer GM sound. Also Iowa MIS Bb trumpet: no mutes, so it does not answer the need. |
| **Recommendation** | **Replace** with the VSCO Harmon-mute sustain set. |

Excluded: Philharmonia trumpet (see the license table), Sonatina trumpets (no mute listed, and a restricted license).

## 2. Saxophone

| | |
|---|---|
| **Best** | **Karoryfer Weresax (alto saxophone)**, https://github.com/sfzinstruments/karoryfer.weresax |
| License | CC0 1.0 (repo `LICENSE`; README: "royalty-free for all commercial and non-commercial use"). No attribution needed. |
| Notes and layers | Chromatic, one sample per semitone, roughly Db2 to G4 in file names, key centers from 49 (Db3) up; sounding about C#3 to G5. 2 velocity layers (`p`, `f`), 2 round robins, 2 mic captures (`cnd` clean condenser, `dnm` overdriven dynamic). Separate breath-noise samples. |
| Size | 256 sample files, about 200 MB for the lot (about 0.75 MB per note sample); a lead needs only a handful. |
| Loop-friendliness | Sustained samples of about 5 to 6 s each, no loop points shipped; cut a crossfaded loop. The SFZ expects vibrato to be added as an effect (sax-style vibrato only dips below pitch), so the samples themselves are non-vibrato, which loops cleanly. |
| Range note | Alto covers C4 to G5 naturally; C6 would need a shift of about 5 semitones or a clamp. |
| Runner-up | **FreePats Tenor Saxophone** (CC0, Versilian's tenor, https://freepats.zenvoid.org/Reed/saxophone.html). Already edited with infinite sustain loops. Small bank 6.5 MiB (SF2 or SFZ), 4.7 MiB FLAC; full bank 25 to 40 MiB. VCSL source is non-vibrato, 2 velocities, notes about every 3 semitones, plus staccato and a vibrato set. Tenor suits C3-C5 better than C5-C6. Also Karoryfer Bear Sax (baritone, CC0, 145 MB), VCSL Saxello and Tenor directly (CC0), and MTG Solo Sax (soprano/alto/tenor/baritone, 24-bit, pre-looped SFZ, **CC-BY 4.0, attribution required**; https://github.com/sfzinstruments/MTG.SoloSax). |
| **Recommendation** | **Replace** with Weresax alto (best range fit), or FreePats Tenor Sax if loops-already-done and size matter more than range. Both CC0. |

## 3. Vibraphone

| | |
|---|---|
| **Best** | **VCSL Vibraphone**, https://github.com/sgossner/VCSL (`Idiophones/Struck Idiophones/Vibraphone`) |
| License | CC0 1.0 (repo README: "no royalties, no credit, no special terms"). Already used in this repo, so the provenance and manifest path are known. |
| Articulations | Soft mallets (`v1`, `v2`), hard mallets (`v2`, `v3`), bowed (6 sustained notes: A2 D4 E3 E5 G3 A4). 1 round robin (soft C3 and E3 `v1` have 2). |
| Notes | 11 notes per mallet set, file names F2 A2 C3 E3 G3 B3 D4 F4 A4 C5 E5, so about F3 to E6 sounding if the -1 octave rule holds: covers C4-C6. |
| Size | 22 files per mallet set, about 42 MB per set, about 1.9 MB each (stereo, 44.1 kHz). |
| Loop-friendliness | Struck and decaying: no loop needed. The bowed set is sustained and loopable if a held pad-like vibes tone is ever wanted. |
| Motor | No motor-on versions: the library gives straight (motor off) notes only. Motor vibrato is easy to add with an amplitude LFO (about 5 to 6 Hz), as the current synth presumably does. |
| Runner-up | Iowa MIS vibraphone (sustain, damp, short sustain, bow; pp/mf/ff; 24/44.1 stereo AIFF; octave-length files; license text is only "free to use", see the license table); FluidR3 / MuseScore General preset 11 (MIT). Neither adds a motor version. |
| **Recommendation** | **Replace** with VCSL soft mallets (`v2` for the main layer, `v1` for soft); hard mallets for a brighter option. |

## 4. Guitar lick (jazz single-note lead)

| | |
|---|---|
| **Best** | **Karoryfer Shinyguitar (archtop, played by D. Smolken), electric (pickup) channel**, https://github.com/sfzinstruments/karoryfer.shinyguitar |
| License | CC0 1.0 (repo `LICENSE`, GitHub reports CC0-1.0; README: "royalty-free for all commercial and non-commercial use"). No attribution needed. |
| Notes and layers | Pickup samples (`Samples/electric`): 338 non-release files, notes db2 e2 gb2 a2 c3 eb3 gb3 a3 c4 eb4 gb4 a4 c5 eb5 gb5 a5 c6 (every minor third, to C6), 3 velocity layers, 4 round robins, plus mute (palm-muted), percussive and release-noise samples. A mic channel (`Samples/acoustic`) holds the same set. |
| Size | About 227 MB per channel in the repo (about 0.7 MB per note); a lead needs a few zones. |
| Loop-friendliness | Plucked, decaying: no loop. Sustained lead notes can just decay; the SFZ also has a vibrato/mute CC set for reference. |
| Notes | Archtop through a magnetic pickup is the actual jazz-guitar sound, closer than Emilyguitar's Epiphone. The survey at `docs/research/guitar-samples-survey-2026-10-25.md` (C4) already listed it; Emilyguitar was picked for the "electric" slot instead. |
| Runner-up | **FreePats Clean Electric "Clean #2 (Jazz)"** (CC0; small bank 2.4 MiB FLAC; bridge pickup only so brighter). Next: Karoryfer Black and Green Guitars (Gretsch hollowbody and Hofner Club, CC0, about 500 MB). |
| **Recommendation** | **Replace** with Shinyguitar's pickup samples. |

## 5. String lead (string-ensemble lead)

| | |
|---|---|
| **Best** | **VSCO-2-CE Violin Section, `susVib`**, https://github.com/sgossner/VSCO-2-CE (`Strings/Violin Section/susVib`). Also a solo violin: `Strings/Solo Violin/Arco Vib`. |
| License | CC0 1.0. No attribution needed. |
| Articulations | Sustain with vibrato (`susVib`), tremolo, spiccato, pizzicato. No non-vibrato sustain for the section. Viola and cello sections add a `susvib` set each. |
| Notes and layers | Violin Section susVib: 22 files, 11 notes (A2 A3 B2 B4 C4 D3 D5 E4 F#3 G2 G4 in file names), 2 velocity layers. Cello Section susvib: 27 files, 13 notes. Viola Section susvib: 26 files. |
| Size | Violin Section susVib 46 MB (about 2 MB per file); Cello Section susvib 73 MB; Viola Section 72 MB. |
| Loop-friendliness | Long sustains of 8 to 14 s with the vibrato baked in. Loops of baked vibrato tend to pulse; this is the main risk. A long loop (2 to 4 s) with a long crossfade reduces it. |
| Owner risk | Sam heard sampled string ensembles as "scratchy/grainy" and FluidR3 strings cut off awkwardly on quick notes with gaps. A true section recording with vibrato may still sound grainy at low sample rates (22 kHz, 12 to 16 bit), and a baked vibrato makes it harder to loop. |
| Runner-up | Karoryfer x bigcat Cello with its 4-voice and 8-voice unison mappings as a pseudo-ensemble (CC0; see item 6). Karoryfer String Cyborgs (CC0) are synthesizers built on bowed-string samples and are not a natural ensemble. FluidR3 / MuseScore "Strings Slow/Fast" (MIT) are the ones Sam already disliked. |
| **Recommendation** | **Replace only if it survives an ear test in a real song** (per "judge sounds in songs"): VSCO Violin Section susVib, soloed against the synth on a dense example. If it sounds grainy or pulses, **remove the string lead** and let the (real) cello and pads carry the role; there is no better freely licensed ensemble source. |

## 6. Cello

| | |
|---|---|
| **Best** | **Karoryfer x bigcat Cello** (solo cello, performed by Kamila Borowiak), https://github.com/sfzinstruments/karoryfer-bigcat.cello |
| License | CC0 1.0 (`LICENSE`; README: "royalty-free for all commercial and non-commercial use"; copyright 2016 Karoryfer Lecolds and bigcat instruments). No attribution needed. |
| Articulations | Sustain (`Samples/sus`), staccato (270 files), pizzicato (52), noises (21). Bowed with velocity layers or a mod-wheel dynamic; 4- and 8-voice unison; vibrato, glide and legato are SFZ controls (the sample vibrato is controlled, so the sustains are loop-friendly). |
| Notes and layers | Sustain pitches C, Eb, Gb, A in each octave, C1 to C5 in SFZ key naming (C1 is key 24), 4 velocity layers (`p`, `mp`, `mf`, `f`), 2 takes per note (`d`, `g`). Covers B3 to A4 (Gb3 at key 54 up through A4 at key 69, then C5 at 72). |
| Size | Sustain set 136 files, 86 MB (about 0.57 MB per file, 24-bit mono 44.1 kHz, about 4 to 6 s each). A cello lead needs roughly 6 zones at one velocity layer. |
| Loop-friendliness | Good: 4 to 6 s of steady bowing per note. The SFZ ships no loop points, cut offline. |
| Runner-up | **FluidR3_GM Cello (MIT)**, the one Sam already called good (4.2 KB at key 58, 0.36 s loop, 14.7 kHz; also MuseScore_General preset 42, same samples, MIT). Tiny but a single sample set; it is what the earlier spike heard. Also VSCO-2-CE Cello Section `susvib` (CC0, section not solo, 73 MB). |
| **Recommendation** | **Replace** with the bigcat cello; keep the FluidR3 cello as the known-good A/B reference (the spec noted the cello "was the one sample Sam called good rather than clearly better", so the bigcat set is worth an ear test against it). |

## Licenses checked

| Source | License as found | Usable in GPL-3.0 firmware and a local tool? | Notes |
|---|---|---|---|
| VCSL (sgossner/VCSL) | CC0 1.0 | Yes | Aerophones: Tenor Saxophone, Saxello, harmonicas, recorders, ocarinas, organ, didgeridoo. Chordophones: pianos, harpsichords, harps, zithers, bowed psaltery. Idiophones: Vibraphone, Marimba, Glockenspiel, Xylophone, bells, kalimba. No trumpet or brass besides didgeridoo, no violin, cello or bowed strings. |
| VSCO-2-CE (sgossner/VSCO-2-CE) | CC0 1.0 | Yes | Brass (trumpet with Harmon and straight mutes, horn, trombones, tuba), woodwinds (flute, oboe, clarinet, bassoon), strings (violin, viola and cello sections, solo violin, solo contrabass, harp), keys, percussion (glock, marimba, xylo, timpani). No saxophone, no vibraphone. |
| Karoryfer (sfzinstruments org) | CC0 1.0 on the repos used here (cello, Weresax, Bear Sax, Shinyguitar, String Cyborgs, double bass). A few other repos in the org carry CC-BY or other licenses; check each. | Yes for the ones named | Alto sax, baritone sax, solo cello, archtop guitar, double bass, many electric basses and drums. No trumpet or vibraphone. |
| FreePats (freepats.zenvoid.org) | CC0 1.0 per instrument page | Yes | Tenor Saxophone (from VCSL, pre-looped), clean and distorted electric guitar, nylon and steel guitars, harp, glass, tubular bells, xylophone, synth strings and brass. No acoustic trumpet, cello, vibraphone or string ensemble. The site front page does not state a license; each instrument page does. |
| University of Iowa MIS | No formal license; page says the recordings "may be downloaded and used for any projects, without restrictions" and asks for feedback | Probably, but weakly documented; not a license text | Bb trumpet (no mutes), soprano and alto sax, vibraphone, cello, violin, viola. 24-bit AIFF stereo, per-octave files; pre-2012 sets are mono 16/44.1 and chromatic. The vibraphone page has no license text at all. Treat as a fallback only; other FOSS projects ship Iowa derivatives (Karoryfer Scarypiano, under CC0), but that is their claim. |
| Sonatina Symphonic Orchestra | Creative Commons Sampling Plus 1.0 (reported) | **No (avoid)** | Sampling Plus is not a free-culture license (copying the whole work is restricted to non-commercial use, advertising restrictions), and part of the content comes from Iowa and Philharmonia. No muted trumpet found. |
| Philharmonia Orchestra samples | Free to use, but "must not be sold or made available 'as is' (as samples or as a sampler instrument)" | **No** | Shipping samples inside a sampler firmware is exactly the prohibited use. |
| FluidR3_GM | MIT | Yes (attribution kept with the notice) | Already used in FM1-GM. Presets: Vibraphone, Jazz Guitar, Cello, Harmon Mute Trumpet, soprano/alto/tenor/baritone sax, strings. Quality is a single looped sample per preset; strings are what Sam disliked. |
| MuseScore_General (SF2 208 MB, SF3 36 MB) | MIT (acknowledgements and copyright notices must be kept) | Yes | The sample-sources list shows the relevant instruments (vibraphone, jazz guitar, cello, trumpet, Harmon trumpet, saxes) are unchanged FluidR3 (mono) samples, so nothing better than FluidR3 for these six. Newer improved samples are in other instruments. |
| GeneralUser GS | Custom permissive license ("use in your software projects", commercial use allowed); author "cannot be 100% sure where all of the samples originated" | Doubtful | Provenance is uncertain, and the license is not a standard one; prefer FluidR3 or CC0 sources. |
| MTG Solo Sax (kinwie; samples from MTG on Freesound) | CC-BY 4.0 | Yes, with attribution | Soprano, alto, tenor, baritone, 24-bit, pre-looped SFZ, FLAC, about 111 MB. Only attribution-required option here. |
| Freesound | Per-file; one-shots only, no complete multisample pack with a single license found | n/a | A CC0 single Harmon-mute note (Samulis, from VSCO-2) exists; it is a one-shot from the same VSCO library, so use VSCO directly. |

## Decision summary

| Instrument | Action | Take from | Attribution |
|---|---|---|---|
| Muted trumpet | Replace | VSCO-2-CE Trumpet `harmonM-sus` | none (CC0) |
| Saxophone | Replace | Karoryfer Weresax alto, or FreePats Tenor Sax | none (CC0) |
| Vibraphone | Replace | VCSL Vibraphone soft mallets | none (CC0) |
| Guitar lick | Replace | Karoryfer Shinyguitar pickup samples | none (CC0) |
| String lead | Replace if ear test passes, else remove | VSCO-2-CE Violin Section `susVib` | none (CC0) |
| Cello | Replace | Karoryfer x bigcat Cello `sus` | none (CC0) |

Open items for the owner or the implementation spike:

1. Confirm key centers (the VCSL and VSCO file-name octave) with a quick pitch measurement before mapping.
2. Cut loops for the sustained sets (trumpet, sax, cello, violin) and run them through the repo's loop and crossfade tooling; judge in songs, not soloed.
3. Decide alto vs tenor for the sax (range C4-C6 favors alto).
4. Ear-test the string lead; if it fails, remove it.
5. Record the sources, commit ids and sha of each LICENSE in `tools/samples/manifest.json` and CREDITS.md, as for the existing banks.

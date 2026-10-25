# Drum sample survey for VNL-1 (FM-1 firmware)

Date: 2026-10-25. Method: web search plus direct reads of GitHub, archive.org, Freesound search pages and license pages. Nothing was downloaded. I did not listen to any sample, so "lo-fi fit" is judged from descriptions and instrument type. Freesound rate-limited me (HTTP 429) partway through, so the Freesound picks are leads to audition, not verified finds. Every Freesound item needs its license badge re-checked on its page (the search filter was "Creative Commons 0", but a page can be edited).

Reminder on terms: the firmware embeds derived data (4-bit ADPCM, re-tuned, filtered). Treat that as redistribution of the samples inside a product. CC0 needs nothing. CC BY needs attribution (a credits file or on-device/README text). CC BY-SA 3.0/4.0 is the problem license: whether an ADPCM-converted sample embedded in a firmware image makes the whole firmware an "adaptation" is unsettled. Safest is to avoid it, or keep it out of the main image.

## 1. Sources, ranked (sound quality / lo-fi fit first, then license)

### A. Karoryfer lo-fi/odd kits on sfzinstruments (CC0)
Listing with licenses: https://sfzinstruments.github.io/drums/

- **Gogodze Phu Vol II** (Karoryfer, 2018). Acoustic kit "from lo-fi to hi-fi", inspired by the distorted drums on Kim Jung Mi's "Now" (1973). Kick, snare, 3 toms, hi-hat; about 1,701 16-bit WAVs, about 400 MB. Best lo-fi fit of anything found: it was designed with degraded variants. The sfzinstruments list says CC0-1.0 ([list](https://sfzinstruments.github.io/drums/)). KVR lists it as GPL 3 ([KVR](https://www.kvraudio.com/product/gogodze-phu-vol-ii-by-karoryfer/details)). CONFLICT: do not ship until the LICENSE file in the actual download (check the sfzinstruments GitHub repo) is read. The repo URL guess I tried returned 404; find it from the sfzinstruments page.
- **Big Rusty Drums** (Karoryfer). Oversized 1980s Polish kit, 2.3 GB. License: CC0-1.0, full legal text in repo: https://github.com/sfzinstruments/karoryfer.big-rusty-drums (LICENSE file). Big, boxy, characterful; kick/snare/toms good for dusty boom bap. Format: WAV (Aria/SFZ). Source is large; take a handful of hits only.
- **Swirly Drums** (Karoryfer). Punk/indie kit plus unusual percussion played with brushes. CC0-1.0: https://github.com/sfzinstruments/karoryfer.swirly-drums (CC0 text on repo page). Brush material is the best jazz-hop candidate found. Note: it was sold commercially ($29, later $9) before being opened; the repo carries CC0, which is what governs.
- **Unruly Drums** (Karoryfer). Every drum including kick is a snare; 2 GB. CC0-1.0 per sfzinstruments list. Odd, probably low use, but snare-like "kicks" can be dusty.
- **Virtuosity Drums** (Versilian Studios + Karoryfer, KVR 2021 contest): CC0-1.0 per https://github.com/sfzinstruments (repo `virtuosity_drums`). Not inspected for contents.
- **Frankensnare**: snares from small to huge, 900 MB, CC0-1.0 per the list. Good source for several snare tunings.

### B. SM Drums (Public Domain per sfzinstruments list)
Deeply sampled free drums, 2.2+ GB, listed as "Public Domain" on https://sfzinstruments.github.io/drums/. Clean/acoustic. Good raw material for hats, kicks, snares with velocity layers. Public domain dedication wording is not as strong as CC0 (jurisdiction issues); acceptable, but note it.

### C. Versilian Community Sample Library (VCSL), sgossner
https://github.com/sgossner/VCSL. License: "This collection is under a Creative Commons 0 license" (README; LICENSE file in repo). WAV, round robins and velocity layers throughout.
Percussion folders I enumerated through the GitHub API:
- Struck idiophones: Hi-Hat Cymbal (closed, loose, 4 velocities x 2 RR, about 210-490 KB each), Claps (6 group claps plus 4 solo claps), Cajon (2 hits, velocities and RR), Cowbells, Claves, Triangles, Woodblock, Finger Cymbals, Suspended Cymbals, Sleigh Bells, Tambourine 1-2 (+3 legacy), Cabasa, Guiro, Agogo Bells, Brake Drum.
- Shaken: Shaker Large and Small (a Shaker folder exists; I could not list the files), Tambourine 1 shakes and rolls.
- Membranophones: Bass Drum 1-2, Snare Drum Modern 1-3, Rope Tension Snare, Tom 1-2, Bongos, Conga, Quinto, Tumba, Darbuka, Frame Drum, Timpani.
Character: clean, concert/orchestral-leaning (bass drums are big concert drums, snares are orchestral-style, no kit sound). Great for percussion (conga, bongo, tambourine, shaker, claps, cajon as a soft kick/rim stand-in). Weak for kicks and snares unless heavily filtered and saturated.
- **VSCO 2 CE**: https://github.com/sgossner/VSCO-2-CE, CC0-1.0 (license label on repo page). Has `Percussion` and `VSCO 1 Percussion` folders; I did not enumerate them. Orchestral percussion, same caveat.

### D. Freesound CC0 single hits (leads to audition; license to verify per sound)
Freesound license definitions: https://freesound.org/help/faq/ (CC0 = no conditions).
- Brush snares: [SNARE_brushes1.wav (jannevse)](https://freesound.org/people/jannevse/sounds/614387/), [brush_hit_small.aif (Heigh-hoo)](https://freesound.org/people/Heigh-hoo/sounds/20296/), [snarebrushrimshotloud.wav (stomachache)](https://freesound.org/people/stomachache/sounds/44868/). Search result: https://freesound.org/search/?q=snare+brush&f=license%3A%22Creative+Commons+0%22 (159 hits).
- 808 / boomy kicks: [808 Deep kick (EKVelika)](https://freesound.org/people/EKVelika/sounds/208447/), [Rusty Box 808 style kick 1-3 (thaighaudio)](https://freesound.org/people/thaighaudio/sounds/351063/) (lo-fi-sounding by name), [Cannon 808'd kick (Remedial)](https://freesound.org/people/Remedial/sounds/135549/), [KickAC (bdu)](https://freesound.org/people/bdu/sounds/808/). Warning: "MPC_808_*.aif" by kbeezy88 (https://freesound.org/people/kbeezy88/sounds/501759/) are likely sampled from commercial gear/libraries; CC0 stamp does not remove provenance risk.
- Hats: [Hip Hop Closed Hihat (ToxicFilmzSRsounds)](https://freesound.org/people/ToxicFilmzSRsounds/sounds/448223/), [hat cl3 (shpira)](https://freesound.org/people/shpira/sounds/317093/), [closedhatROOMPhrase (stomachache)](https://freesound.org/people/stomachache/sounds/140535/), [Hihat_dry (heideldrum)](https://freesound.org/people/heideldrum/sounds/846110/), [OpenAndCloseHatAC (bdu)](https://freesound.org/people/bdu/sounds/812/), [rd-6 c hihat (laffik)](https://freesound.org/people/laffik/sounds/638084/) (RD-6 is a Behringer clone of the 808, lower IP risk than Roland samples).
- Toms: [lo fi tom 00.wav (johnnypanic)](https://freesound.org/people/johnnypanic/sounds/647625/).
- Loops (not usable as one-shots, but could be sliced): holizna 85 BPM Lofi HipHop drum loop https://freesound.org/people/holizna/sounds/621177/; deadrobotmusic drum loops (also CC0 per filter).
- Not yet searched (rate-limited): shakers, rimshots/sidesticks, snaps, dusty snares. Do those next with the CC0 + duration filter.

### E. Hydrogen drumkit library (mixed licenses)
Official feed: hydrogen-music.org/feeds/drumkit_list.php (my fetch failed on a certificate error). The Debian packaging copyright file lists the kits Debian accepted: https://metadata.ftp-master.debian.org/changelogs/main/h/hydrogen-drumkits/stable_copyright
- GPL-2/GPL-2+: ElectricEmpireKit, HardElectro1, VariBreaks (Artemio), Millo drums, ColomboAcousticDrumkit (GPL-2+ with a composition exception), Gimme A Hand, The Black Pearl, ForzeeStereo.
- CC BY-SA 3.0: circAfrique, BJA_Pacific.
- CC BY 3.0: rumpf_kit_z01_h2 (README says GPL, a conflict), belofilms_GuitarStrums.
- CC0: Audiophob (dubstep kit, probably wrong character).
- Flags: GPL is a poor fit for firmware (it requires GPL terms on the combined work for software; for data it is ambiguous). Debian warns that many Hydrogen kits sample hardware drum machines and that the hardware makers likely hold copyright; some kits in the full feed carry non-commercial clauses (those are excluded Debian's list). Do not use anything from the feed beyond what is on the Debian list without reading its drumkit.xml license field.

### F. DrumGizmo / SFZ ports of acoustic kits (CC BY, multi-mic, huge)
- **MuldjordKit** CC-BY-4.0 (347 MB SFZ port), **DRS Kit** CC-BY-4.0 (754 MB), **Naked Drums** CC-BY-4.0 (1.3 GB, 10 RR, up to 5 velocity layers). All via https://sfzinstruments.github.io/drums/. Clean, modern multi-mic acoustic: the most realistic hats/snares/kicks, but not lo-fi until processed. CC BY requires credit. DrumGizmo wiki page https://drumgizmo.org/wiki/doku.php?id=kits lists CrocellKit, DRSKit, MuldjordKit, The Aasimonster but gives no per-kit license on that page; check each kit page.
- **Salamander Drumkit** (Alexander Holm): CC BY-SA 3.0, 369.7 MB, https://archive.org/details/SalamanderDrumkit. Share-alike: avoid for the main image.
- **Sam's Sonor**: CC-BY-SA-4.0. **G1 / L1** (Gretsch/Ludwig): "Freemium" free Basic versions, terms not checked; skip.

### G. oramics/sampled drum machines (provenance risk)
https://github.com/oramics/sampled (DM folder: CR-78, LM-2 LinnDrum, MRK-2, TR-505, TR-909 Detroit/SP, TR-808 with 16 numbered variants of each voice: BD, SD5, CH, OH, CP, CL, RS, CB, MA, CY, toms, congas). The READMEs label them "Public Domain" (e.g. [LM-2 README](https://raw.githubusercontent.com/oramics/sampled/master/DM/LM-2/README.md), source machines.hyperreal.org). That label is the collector's assertion, not a grant from Roland or Linn. These are copyrighted recordings of commercial hardware or of sample libraries. Sound is perfect for lo-fi hip hop; legal risk is real. Recommend treat as "audition only, replace later" unless the user accepts the risk. Better: use an original CC0 808-style synthesized kick (Freesound CC0 above, or synthesize in-firmware: kick and snare synthesis costs almost no flash).

### H. Commercial free packs: licensing traps (checked what I could)
- **99Sounds**: license page https://99sounds.org/license/ says "You do not have the right to sell or redistribute (even for free) the audio files" on their own or as a separate attachment. Usable in your music, not in a device. Redistribution inside firmware: NOT allowed (a firmware is arguably "as part of a product", but the text does not grant it; treat as excluded). The license text does not mention samplers or instruments; ask them in writing if you want one of their kits.
- **Cymatics, BVKER, Samples From Mars, Splice**: I could not retrieve the license texts (429 and 403 errors, searches returned only secondary summaries that say "royalty-free"). Secondary sources say only royalty-free for music use; none confirm redistribution inside instruments. Treat as excluded unless you read their license text. Sample Focus's license prohibits redistribution in sample packs (https://samplefocus.com/license).
- **MusicRadar SampleRadar**: I could not retrieve the terms page (404). Do not use without reading the current terms.
- Hardware-sourced "free 808/909/LinnDrum packs" on the web: copyright of the hardware maker or the sample's creator likely applies (Debian's note above).
- "Royalty-free" almost always means "use in your tracks", not "redistribute in another product". Only CC0/CC BY/PD/explicit grants count.

## 2. Top list (about 10)

1. Karoryfer Gogodze Phu Vol II - lo-fi acoustic kit, built degraded. CC0 per sfzinstruments, GPL 3 per KVR: resolve first.
2. Karoryfer Big Rusty Drums - CC0, big 1980s Polish kit.
3. Karoryfer Swirly Drums - CC0, brushes and odd percussion.
4. SM Drums - Public Domain, clean deep samples for hats/kick/snare.
5. Frankensnare - CC0, many snare tunings.
6. VCSL - CC0, percussion (tambourine, shaker, conga, bongo, cajon, claps, cowbell, hi-hat with RR).
7. Freesound CC0 singles (brushes, 808-style kicks, hats, lo-fi tom).
8. Karoryfer Unruly Drums / Virtuosity Drums - CC0, texture material.
9. MuldjordKit / DRS / Naked Drums - CC BY 4.0, realistic acoustic (needs credit).
10. oramics/sampled drum machines - great sound, flagged copyright risk, not recommended for shipping.

## 3. Suggested shared pool (all CC0 unless noted; audition required)

Targets: 16-22 kHz mono, trim tails hard, 4-bit ADPCM. Rough flash: a 200 ms hit at 16 kHz in 4-bit = 1.6 KB; 400 ms = 3.2 KB. Whole pool of 21 hits at about 250 ms average is about 35-45 KB, leaving room.

| Slot | Pick | Source |
|---|---|---|
| Kick 1 (soft/dusty) | Big Rusty Drums kick, soft velocity | Karoryfer, CC0 |
| Kick 2 (punchy) | SM Drums kick, medium | PD |
| Kick 3 (lo-fi) | Gogodze Phu Vol II kick, degraded variant | Karoryfer, license TBD |
| Kick 4 (808 sub) | EKVelika "808 Deep kick" or thaighaudio "Rusty Box 808 style kick 2" | Freesound CC0 |
| Snare 1 (crunchy) | Gogodze Phu Vol II snare (degraded) | license TBD; fallback Big Rusty snare |
| Snare 2 (dusty body) | Big Rusty Drums snare | CC0 |
| Snare 3 (tight/clean) | Frankensnare small snare | CC0 |
| Snare 4 (brushed) | Swirly Drums brush snare, or jannevse SNARE_brushes1 | CC0 |
| Rim 1 | VCSL Snare Drum Modern, rim/side hit (pick from NS-hit variants) or Swirly Drums rim | CC0 |
| Rim 2 (stick) | VCSL Claves 1 hit (re-tuned) as dry click/sidestick | CC0 |
| Closed hat 1 | VCSL Hi-Hat Cymbal HitC v2 | CC0 |
| Closed hat 2 | SM Drums closed hat | PD |
| Closed hat 3 (lo-fi) | ToxicFilmzSRsounds "Hip Hop Closed Hihat" or shpira "hat cl3" | Freesound CC0 |
| Open hat 1 | VCSL HiHat_HitLoose | CC0 |
| Open hat 2 | SM Drums open hat | PD |
| Shaker 1 | VCSL Shaker, Small | CC0 |
| Shaker 2 | VCSL Cabasa (or Tambourine 1 Shake) | CC0 |
| Ride | Swirly Drums ride, else SM Drums ride | CC0 / PD |
| Clap/snap | VCSL Claps (SoloClap_vl2) | CC0 |

Extras if wanted: VCSL Conga/Quinto/Tumba hits, VCSL Tambourine 1 hit, Gogodze/Big Rusty toms.

## 4. Open items

- Resolve the Gogodze Phu Vol II license conflict (CC0 vs GPL 3) from the repo LICENSE.
- Audition Freesound picks; finish shaker, rimshot, snap, dusty-snare searches (rate limit).
- Enumerate SM Drums, Swirly, Frankensnare and VSCO 2 CE file lists before choosing exact hits (I only confirmed VCSL file lists).
- Keep a credits/provenance file per hit (source URL, license, author) in the repo, even for CC0.
- Defer to the "To revisit" ledger: whether to synthesize kick/snare/hat in-firmware rather than sample them.

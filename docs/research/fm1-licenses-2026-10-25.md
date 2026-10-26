# FM-1 firmware licenses: GPL vs MPL (2026-10-25)

Every repo in cicloid/awesome-fm-1 (CC0) plus the earlier firmware survey, about 180 repos, each license read from
GitHub's license detection (`gh api repos/<owner>/<repo>`). Detection reads the LICENSE file; a repo's own notes (or
bundled third-party code) can differ, so check the file before reusing anything.

## The count

| License | Repos | What it means for VNL-1 (MPL-2.0) |
|---|---|---|
| GPL-3.0 | 104 | Ideas only. Copying code would make VNL-1 GPL-3.0. |
| MIT | 28 | Reusable with the notice. |
| none | 20 | No license = all rights reserved: ideas only, no code. |
| MPL-2.0 | 8 | Reusable file by file (keep MPL on the files). |
| NOASSERTION | 8 | A license GitHub can't identify: read the file first. |
| Apache-2.0 | 4 | Reusable with the notice. |
| GPL-2.0 | 3 | Ideas only (and GPL-2.0-only is incompatible with GPL-3.0 too). |
| Unlicense, WTFPL, CC0 | 4 | Public-domain-like: reusable. |
| not found | 5 | Deleted or private. |

## The MPL family (the fm1-base / felucca-bsp line)

charlesvestal/fm1-base, hugelton/felucca-bsp, charlesvestal/fm1-amb (AMB-1), charlesvestal/fm1-ctl (CTL-1),
charlesvestal/fm1-radio, genkigenki/erik37 (sampled grand), johnskiller/fm1-ctrl, and pianomansam/fm1-vnl (VNL-1).
Note the split inside Charles Vestal's work: fm1-base, AMB-1, CTL-1 and Radio are MPL; X0X, Fomni, Polly, Monk,
Velocity and Mocky are GPL-3.0.

## The GPL family (the Felucca line and most firmwares)

Felucca itself (hugelton/Felucca and its forks), SLOOP, Optimist, Skiff, Shine On, Melodee, Flyfish, FM1-GM, FMEL-1,
Ghoulbox, fm1-chord, Frodhes, Demake, Dinghy, Hortator, Felucca-WebApp, and Charles Vestal's X0X, Fomni, Polly,
Monk, Velocity and Mocky. Most of the scene is GPL-3.0 because Felucca is.

## Reusable non-MPL code worth knowing

- **hugelton/Felucca-Engines (MIT):** five of Felucca's synthesis engines (ANALOG, LOFI, TRIO, WHEEL, NOISE), split
  out under MIT. The one way to reuse Felucca engine code in VNL-1.
- **Discodave-Glitch/eight-bells (MIT):** the Euclidean beats and melodies we studied.
- **beashoko/april-os (MIT):** a sampling-focused Felucca fork, licensed MIT. Check its files: a Felucca fork
  relicensed MIT may still contain GPL code it can't relicense.
- **Keitark/fm1-nes (Apache-2.0), czietz/fm1-firmware-patcher (Unlicense), AL-255/FM-1-RE (WTFPL):** tools and
  reverse-engineering notes.
- **Bundled third-party code inside GPL repos** keeps its own license: FluidR3 samples (MIT) in FM1-GM, the Sonivox
  EAS synth (Apache-2.0) in Polly, Mutable Instruments Plaits (MIT) in Optimist. Those parts can be taken from their
  original upstreams, not from the GPL repo.

## For VNL-1

- VNL-1 stays MPL-2.0 (fm1-base's). The standing rule holds: no GPL code; ideas only.
- If Sam ever wanted GPL code (e.g. from Optimist), VNL-1 would have to become GPL-3.0 as a whole (MPL-2.0 allows being
  combined into a GPL-3.0 work); that would bind everyone who builds on it.

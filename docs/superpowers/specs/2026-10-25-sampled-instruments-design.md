# Sampled instruments, strums, string leads and sampled kits: design

Status: approved; implemented on branch `sampled`. Builds on the instrument spike (branch `spike/instruments`, through 8b5dc13; its doc
`docs/research/instrument-spike-2026-10-24.md`), the budget notes (`docs/research/cpu-vs-flash-2026-10-25.md`) and
Sam's picks in the song-engine ledger ("Approved by ear", 2026-10-25).

## Why

Sam (2026-10-25): "try to use samples as well as synthesis... as long as the license allows them." The spike let Sam
pick by ear. This design brings those picks into the real engine, as new options beside the synths.

## Rules from Sam that bind everything below

- **Add, don't replace.** Every new sound is an added option; the synths stay. The only replacement Sam named: the
  FreePats nylon replaces the synth (Karplus-Strong) nylon.
- **Reuse, don't reinvent.** One sampler voice for every sampled instrument; one strum engine for both guitars; one
  string synth for the high lead and the cello; one sample pool for every sampled kit; one shared ride.
- **Only approved parts.** Nothing Sam removed comes back (the upright bass, offbeat-only comping, Swirly, cross-sticks
  as raw samples, the Kawai, the FluidR3 cello, the synth nylon, every shaker but the VCSL small one).
- **Licenses:** CC0 or MIT only, each verified at its source; attribution in `CREDITS.md`. No GPL code.

## Goals

1. **Knight upright piano** (VCSL, CC0): the one acoustic piano. It plays any piano part: held, half-note, arpeggio
   and stab comping, the upcoming broken-chord climb, and the lead. Never the lead and the keys in the same song.
2. **Strummed guitar:** a strum engine (down and up strokes, a slow or quick spread, accents, muted chucks, one voice
   per string) as a comping style for the guitars. Patterns: down/down-up/up-down-up, chucks, bossa, ballad. Quick
   spread for every pattern but the ballad, which rolls quick or slow (Sam).
3. **FreePats Spanish classical nylon** (CC0) replaces the synth nylon in every set that has it, held, strummed and
   struck.
4. **Karoryfer Emilyguitar** (CC0): a jazz/clean electric for comping (held, stabs, strums) and the lead.
5. **String ensemble lead:** the spike's string-section synth (2a, with the Plaits Ensemble, MIT) as a lead, in two
   registers: the high lead (2a as heard) and the cello lead (the "low and dark" voicing, demo 2c, which Sam passed).
6. **Sampled drum kits** from one pool, processed per kit (pitch, low-pass, drive at playback), with real hi-hat choke:

   | Kit | Kick | Snare | Hats (closed / open) | Status |
   |---|---|---|---|---|
   | Warm (KIT_S_WARM) | Gogodze | Gogodze sidestick | Gogodze; rides throughout | approved as heard (down 1 st, LP 3.2 kHz, drive 1.2) |
   | Dark (KIT_S_DARK) | Gogodze | Gogodze center, down 3 st, LP 3.5 kHz | Gogodze closed down 3 st, LP 5 kHz / its open, same | my grouping |
   | Rusty (KIT_S_RUSTY) | Big Rusty | Frankensnare 20x6 poplar | Big Rusty foot chick / Big Rusty open, soft | my grouping |
   | Gentle (KIT_S_GENTLE) | Big Rusty | VCSL rope snare, snares off | VCSL pedal close / VCSL open, plus the VCSL small shaker | my grouping |
   | Clean (KIT_S_CLEAN) | Gogodze | Frankensnare 20x6 poplar | VCSL closed, softest / VCSL open | my grouping |

   Sam approved every listed hit and pair; the groupings and names are my call (named apart from the synth kits'
   Dusty and Soft), regrouped by ear once rendered.
   **Other lanes (my call):** the rim lane plays the kit's snare at a ghost's level (the warm kit: its sidestick); the
   shaker lane plays the VCSL small shaker in every sampled kit (the synth shaker is not used on sampled kits).
7. **The shared ride** (Big Rusty 22", following each kit's processing; Sam: "that roll sounds good"). A per-song
   roll over songs with a sampled kit (my call on the mechanics): the warm kit always rides throughout; another
   sampled kit rides throughout in about 15% of its songs, switches from hats to ride for one section (a B section,
   the last groove, or a jazzier roll) in about 50%, and stays on hats in the rest. That lands near Sam's 1 in 4
   throughout and half of the rest. The test counts songs with a sampled kit.
8. **Stab comping returns** (Sam: "with better sounding instruments, they may work"), not the offbeat style. Stabs on
   the struck sampled instruments (the Knight, the nylon, the Emilyguitar) only; the synth keys keep their styles.
   **Style weights (my call, by ear in the renders):** the style roll becomes instrument-aware. Synth keys keep today's
   weights (held 44, arpeggio 20, half 36), so their songs don't change. The Knight: held 35, half 25, arpeggio 15,
   stabs 25. The guitars (nylon, Emilyguitar): held 20, half 15, arpeggio (fingerpicked) 15, stabs 20, strum 30.
   Nylon songs shift, as the nylon itself is replaced.
   **Guitars' held comping only at faster tempos (Sam, 2026-10-25):** a held guitar chord is too sparse when the bar
   outlasts the string's ring, so below 84 BPM the guitars' held weight goes to half notes (held 0, half 35).
   **Strumming only under sustained leads (Sam, 2026-10-25):** a strum competes with a struck lead (the vibes, the
   kalimba, the synth pluck, the guitar lick, a Knight or Emilyguitar lead), so under those the guitars don't strum
   (strum weight 0; the other styles keep their weights).
9. **The synth kits stay:** the 808 and Brushes kits have no sampled equivalent (keep); Dusty and Soft remain as
   options until Sam retires them by ear.

## Non-goals

- The broken-chord climb itself (its own design; the Knight is ready for it).
- A-B-A two-lead arrangements, birds, tape warble (ledger).
- Moving the pad layer (M_ENS) onto the new string synth: a later A/B by ear (reuse candidate).
- The Emilyguitar replacing the synth guitar lick (an option Sam hasn't taken).
- Hardware (Plan 2), including confirming the FM-1's real flash.

## Anti-goals

- A sample Sam hasn't heard shipping as if approved (anything new gets a render link before it counts).
- Grain or clicks: loops that pulse, zones that jump in tone, cut-off releases (Sam heard FluidR3 strings cut off
  awkwardly on quick notes).
- Breaking the music rules (voicings, registers, no semitone rubs) or the mix (lead 8-10 dB over the keys).
- Over budget: CPU ≤ 1.0 (≤ 0.97 kept as margin) on the re-picked heaviest roll; flash within the image budget.

## Constraints

- Engine rules: C99, float only, no libm, no malloc, ISR-safe, deterministic per seed, denormal-safe.
- **Flash:** about 215 KB free in the 400 KB image budget (`tools/mem-check.sh`). Estimate for this design: about
  150-190 KB (Knight 38, nylon 31-49, Emilyguitar 51, drum pool about 42-64, code a few KB).
- **CPU:** a sampler voice about 0.03 at half rate (0.066 full); a sampled kit 0.13-0.23; the string synth about 0.10
  at half rate. The heaviest song today (about 0.97) is a synth Rhodes song and stays.
- **RAM:** about 100 B per sampler voice; 4 KB for the ensemble line.
- Samples: 16 kHz mono 4-bit IMA ADPCM, attack plus a crossfaded loop (the spike's packer).

## Design

### Sample data

- **Packed at build time, checked in:** a fetch script downloads each source into its own scratch directory; the
  packer (the spike's `pack.py` / `pack2.py`, tidied) writes one generated C file of `const` tables (zones, root
  notes, loop points, per-hit trims). The generated file is committed, so a build needs no network; the fetch and
  pack scripts regenerate it.
- **Trims from the budget notes:** nylon 0.5 s a note; open hats about 0.4 s; the ride about 0.5 s. Each trim is
  checked by ear in the renders.
- **Hats, shaker and ride sample rate:** 16 kHz cuts everything above 8 kHz, where cymbal sparkle lives. A 22 kHz or
  32 kHz option for these few hits costs only a few KB; the builder renders one A/B and Sam picks.

### The sampler voice (M_SAMP)

One model for every sampled pitched instrument: ADPCM decode, Hermite interpolation, a one-pole tone, an amp envelope,
zone by note (velocity into level and tone, one layer). It runs at half rate where its tone allows (as the keys do).
Pitched roles can play it like any patch (`vx_assign`), so the Knight, the nylon and the Emilyguitar are new patches,
not new code paths.

### The strum engine

A guitar comping style (a new CS_STRUM, used only by the guitars): the voicing spread over up to 6 string voices,
down or up strokes 5-15 ms (quick) or 30-80 ms (slow) apart, accents, muted chucks, per-pattern rhythms from the
spike. Guitar voicings, not the rootless piano ones: the builder adapts the voicer within the chord rules (no
semitone rubs, no substitutions). Strums are rolled per song like the other comp styles.

### The string synth (M_STR)

The spike's 2a model: three detuned saws per note, a high-pass, a low-pass and two formant bands, then the Ensemble.
Two voicings as patches: the high lead (2a) and the cello lead (2c low-dark: HP 65 Hz, LP 1.4 kHz, formants 450 and
1300 Hz). As a lead it plays mono and legato like the sustained leads (the sustained-lead rules apply).
**The cello's register (my call):** as Sam heard and passed it (2c, keys at their usual octave), the cello lead may
sit inside the keys' range, B3-A4: the lead's floor rule (a 4th above the keys' top) is lifted for it. The mix rule
(lead 8-10 dB over the keys) and the semitone rule still apply.

### Sampled kits

A new drum path beside the synth kits: per lane, a sampled hit with the kit's pitch, low-pass and drive at playback;
hi-hat choke (any hat hit cuts the sounding hat in about 6 ms). The kits table above is data. The ride lane uses the
shared ride, with the per-song roll in goal 7; a ride section replaces the hats (closed and open), the snare and kick
unchanged.

### Sets

New sets add the new sounds without removing any (the library's set table):
- Knight piano keys (e.g. with the flute, the sax or a string lead), and a Knight lead over electric keys.
- Emilyguitar comping (strums and stabs) and an Emilyguitar lead.
- High string lead and cello lead sets.
- **Kits per song (Sam, 2026-10-25: "yes"):** every set keeps its synth kit and also names a sampled kit; each song
  rolls one of the two, about half each. Sets on the 808 or Brushes synth kits keep them always (no sampled
  equivalent). New sets may name a sampled kit only.
The exact set list is the builder's, under the moods and flavors; every new set gets a render link.

## Tests (measurable)

- **Sampler:** pitch within 3 cents of the note at every zone and across zone boundaries; no click at a loop point
  (no sample-to-sample jump over a threshold beyond the attack); releases fade, never cut.
- **Strum:** onsets spread within the pattern's window (5-15 ms quick, 30-80 ms slow); up strokes on the "and"s;
  chucks short; never more than 6 voices; no steals inside one strum.
- **Strings:** the two voicings' centroids within 15% of the spike's measured 2a and 2c-low-dark demos.
- **Kits:** each kit's hits present at their lanes; choke cuts an open hat within 10 ms of the next hat hit; ride
  roll shares over 500 songs within ±10% of goal 7's; a ride section has no hats.
- **Stabs:** never the offbeat style; only on the Knight, nylon and Emilyguitar.
- **Pairing:** the Knight never both lead and keys in a song.
- **Mix:** the existing level tests (lead over keys 8-10 dB, pads, duck) pass with every new set.
- **Budget:** flash in budget (`tools/mem-check.sh`); CPU ≤ 0.97 on the standard list plus the re-picked heaviest
  roll (the picker run over sets with samples); recorded in `docs/cost.md`.
- **Licenses:** every source in `CREDITS.md` with its license and URL.

## Left to the builder

The sampler's exact filter and envelope values, the guitar voicer, the set list, the per-kit trims and levels, the
codec details, and the order of work, within the rules above. Calls the builder makes go in the plan's ledger as
rulings; anything Sam must hear gets a render link.

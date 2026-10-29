# VNL-1 Workbench, round 2: sections, parts, piano roll

Status: draft for Sam's review (2026-10-29). Builds on docs/superpowers/specs/2026-10-28-workbench-design.md (the
workbench), whose constraints all still hold.

## Intent

After the first listen, Sam wants to move around a song and judge it part by part: jump to any section, turn parts on
and off, see each part's notes, re-roll one part at a time, and say which rolls work. Same reason as the workbench:
unblock decisions without hand-made renders.

## Goals

1. **Sections.** The song's sections (the plan's blocks: intro, groove, lead, break, outro…) drawn across the top of
   the page, scaled by length, with the playhead moving over them. Click a section: playback continues from that
   section's first bar (catch-up to that bar, the same mechanism as every other change). Feedback entries record the
   section and bar they were given in.
2. **Parts on and off.** Mute and solo per part: keys, bass, lead, pad, ear candy, drums. Listening only: the song is
   unchanged (the keys' duck still follows the lead, voicings still lead), only what reaches the speakers changes.
   Feedback records what was muted or soloed.
3. **Piano roll, view only.** Each part's notes over the whole song: pitch against bars, note lengths, velocity as
   shade, chord changes and section boundaries marked; it scrolls with playback. Drums as a lane grid (kick, snare,
   hats…). It always shows what's playing now (after any change lands, it redraws).
4. **Re-roll per part.** A 🎲 per part makes new notes for that part only, same instrument and same rules:
   - keys: a new voicing path (exists: the voicing re-roll);
   - lead: a new phrase (exists: the phrase re-roll);
   - bass: a new bass line (its style draw and its walking window);
   - pad: new pad notes (its group choices);
   - ear candy: new candy figures;
   - drums: a new drum part from the approved vocabulary (another kick phrase k2-k6 and its ghost/variation draw; the
     kit stays).
   Other parts don't change (what a part reacts to — e.g. the keys making room for a busy lead — may move, and the
   page says so). Each part keeps a history of its rolls; ◀ ▶ steps back and forth through them.
5. **Feedback per part.** 👍 / 👎 and a note on a part's current roll, saved with the full snapshot (including that
   part's roll) so Claude can replay exactly what was heard.

## Non-goals

- Editing notes in the piano roll (parked: what happens to hand-edited notes when a part regenerates is undecided).
- Per-section decisions (a different kit or lead per section).
- Anything on the FM-1.

## Anti-goals

- A mute or solo that changes the song itself (it must be listening only).
- A part re-roll that silently changes other parts without saying so.
- A piano roll that shows something other than what's playing.

## Constraints

As the workbench design: local only, plain HTML/JS, default-off engine hooks under app/ (firmware behavior unchanged
with no hook set), changes land at the next bar (a section jump lands at the next bar too, at the chosen section's
first bar), approved options only, US English, commits never pushed or merged without Sam asking.

## Decided by Claude (Sam can overrule)

- **Section jump timing:** at the next bar line, like every change (no mid-bar cut).
- **Mute/solo** lives in the workbench glue (events for a muted part don't reach the voices, or the part's bus is
  silenced); no engine change.
- **Piano roll data** comes from running the committed song silently on the main-thread probe instance (as the
  lead-register probe does) and collecting every event; redrawn on each landing.
- **Part re-rolls** extend the per-song overrides (song_ov_t) with salts for the bass, pad and candy seed streams and
  a drums salt (kick phrase pick and variation), each default 0 (as rolled). Roll history is page-side (a list of
  salts per part).
- **Drums re-roll** picks within the approved kick phrases and the song's pattern family, never a removed sound.

- **Lead and ear candy:** the existing phrase re-roll moves the lead and the engine's melody/candy streams together;
  it stays that way (old feedback keeps replaying), and the page reports the candy change as a reaction. The new
  candy 🎲 moves only the candy.
- **Drums 🎲 vs the drum screen:** the drums 🎲 draws a kick phrase (k2-k6) plus its variation; it is disabled, with a
  message, while the pattern grid holds hand edits ("reset the drawn pattern to re-roll"). The kick-phrase row keeps
  its own 🎲 (phrase only). A drums history entry is its salt and phrase.
- **Lifetimes:** a seed or arrangement-map change resets part salts and histories (a salt means something else on a
  new song) unless the part is locked; parts get a 🔒 like rows; "re-roll all" re-rolls unlocked parts too; a family
  change keeps the salts; mute and solo are listening state and persist across regenerates.
- **Mute details:** muting cuts that part's sounding notes at once and drops its new ones; muted parts stay in the
  piano roll, greyed. Clicking a section while stopped moves the start point; Play starts there.
- **Reactions:** after a part re-roll lands, the page diffs every other part's events (from the probe) and lists
  any part that changed ("keys moved: they make room for the busier lead"), with no fixed list.

## Left to the builder

- The exact streams each salt reaches (bass: groove.c streams 112/113 and the bass rhythm; pad: pads.c 191; candy:
  melody/candy streams), proven by tests that a salt changes only its part's events (plus documented reactions).
- Drawing (canvas or SVG), zoom and scroll behavior.

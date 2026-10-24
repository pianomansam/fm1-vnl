# SDD ledger — plan: docs/superpowers/plans/2026-10-24-sustained-lead.md
Spec: docs/superpowers/specs/2026-10-24-sustained-lead-design.md. Branch mix-synths (after the synth trim 9642e7e). Executor: subagent-driven.
Synth trim review: approved. minor (deferred): test_glide_songs' bass half vacuous (vx_glides(V_BASS) always 0; drop the role param and bass bookkeeping); test_chip message prints 6 beside a +7 check; docs/cost.md Task 1–4 sections need "superseded by Sam's cut" notes; an old-comment mention of the chillwave/Juno pair.

Pre-flight scan:
| Rows | Produces vs consumes | Found |
| 1→2 | phrase_motif / phrase_bar / phrase_note_t (degrees, roles, 16th timing) consumed by lead.c's adapter | consistent |
| T1 self | tests on the pure plan | consistent |
| T2 self | tests on rendered songs + existing rule tests | consistent; sustained-lead sets after the trim: winds (Felt and trumpet, Nylon and sax, Neo-soul Wurli, Felt and flute, Rhodes neo-soul) + saw-lead synth sets (13, 18, 19) — the plan's render list names sets by name, fine |
Ruling: none needed.
Task 1: Ruling: question and arrival held ≥ a half note (8 16ths), per the spec's test list (the plan said ≥ 4 16ths) — the spec is binding; the breath (≥ half a bar of rest) follows within the bar — cost if wrong: one constant.
Task 1: Ruling: the question/arrival sound from beat 1 of bars 4/8 for ≥ 8 16ths then rest ≥ 8; when bar 3/7 has a beat-1 onset, the target ties in from the end of bar 3/7 (a push) — "beat 1 rests" means no onset on beat 1 (a tie counts as a rest) — cost if wrong: retune one rule.
Task 1: Ruling: phrase-wide leap-then-step-back ≥ 70% (leaps right after a breath are exempt): the approach into a target comes from the side that answers the leap; a plan-wide test is added — cost if wrong: none.
Task 1: Ruling: orn flags recomputed after nudges; a note on 16th 0 or 8 is never an ornament — Task 2's hold-longer rule depends on correct tags — cost if wrong: none.
Task 1: minor (deferred): arc check uses >= (ties pass); phrase.c:211 arrival rule needs a comment; p[48] add() without an assert (worst ~32).
Task 1: fix round 1/5 (3 addressed, 0 open — holds from beat 1 or tied in, phrase-wide leaps 82%, orn recompute; commits 2ccfaba..ba2f151)
Task 1: minor (deferred): onsets per bar reach exactly 5 (no headroom); build()'s redraw loops rely on n1 ≥ 1 (true by construction); ~18% of leaps unanswered (clears 70%).
Task 1: complete (commits 9642e7e..ba2f151, review clean). Carry to Task 2: phrase_bar returns zero notes for bars 4/8 when the target is tied in (its len runs past the bar) — treat that empty bar as a held note.
Task 2: Ruling: Rhodes neo-soul's lead-over-keys median 6.3 (window 4–6) fixed by lowering the muted trumpet's patch level ~0.5 dB (it also sits Felt and trumpet near the top) — Sam judges by ear — cost if wrong: one level.
Task 2: Ruling: "a step back" = 1–2 semitones (the spec's word and the code's own step_back); the phrasing must reach ≥ 70% at that definition — no widened band — cost if wrong: none.
Task 2: Ruling: sustained leads run zero arpeggios (the 1-in-200 tolerance is removed; the cause is pick()'s cadence approach, not a rule conflict) — fix in pick() — cost if wrong: none.
Task 2: Ruling: the plan's block array gets a guaranteed zero terminator (b[25]) for the lead's forward neighbor reads — cost if wrong: none.
Task 2: Ruling: test_line_wanders' guards ported to the phrase line (distinct notes per song; leaps over a 5th within a gesture capped at a small rate) — cost if wrong: none.
Task 2: minor (deferred): +28 velocity for ≤ 4-16th notes swamps the 16-point height arc (dynamics shaped by length — ear check); duplicate #include "groove.h" in lead.c; long lines; motion check allows equal per-phrase counts (no ornaments to drop); cost bench run was noisy (rerun on a quiet machine).
Task 2: fix round 1/5 (6 addressed, 0 open — strict step back 83.8%, zero arpeggios, b[25] terminator + 24-block test, line guards ported, trumpet −0.5 dB, beat-1 bound per arrivals; commits 48fa6d5..f78e58b)
Task 2: minor (deferred): the no-arpeggio rule is a soft cost (measured zero, not structural); leap cap 0.20% vs 0.3% (soft cost); the low-note velocity boost (+28 at note 60, pads excluded) outweighs the 16-point height arc — over percussive keys low notes play louder than high (ear check); the 110 clamp compresses loud short low notes; thin margins (Nylon and sax and Rhodes neo-soul medians at 6.0; contour 90.6% vs 90%).
Task 2: complete (commits ba2f151..f78e58b, review clean). Cost 0.896 (quiet machine).
Final review: with fixes — Important: a key request mid-phrase moves the answer up to an octave (anchor counted from the key; B→C −6.2 st mean). One fix dispatch: key-wrapped re-anchor + test; home flag for tied-in arrivals (pass the next bar's home); re-anchor when the anchor was never set; long lines; redundant include.
Final: Ruling: phrase position counts from the start of the contiguous lead run, not b->start (spec call 1) — a better rule: back-to-back lead blocks continue one phrase — cost if wrong: none.
Final: Ruling: lead_state_t gained vel, leap and ctrun (spec call 7 said nothing new stored) — all reset per song by song_start's memset; vel is per-bar output, leap/ctrun span bar lines — cost if wrong: none.
Final: minor (deferred): thin margins are deterministic at fixed seeds (won't flake; will break on future tuning); the synth-trim minors stay open (test_glide_songs half vacuous, test_chip message, cost.md notes, old comment).
Final: fix wave 170ef6e (key-wrapped re-anchor, tied-in arrival home, unset-anchor re-anchor, cosmetics). Incident: the implementer's script deleted gitignored app/build/new/ (renders only; regenerable); controller re-renders. minor (deferred): at a key change about half the songs move the keys an octave, shifting the lead's floor and answer 4–8 st (engine behavior; the new test skips them); ~half the arrivals on prog 23's home chord reached by more than a step.
Final: fix wave clean (re-review: all addressed; minor: no test of two key requests in a row). Known engine behavior (deferred): on a key change about half the songs revoice the percussive keys an octave away, and the sustained lead's range follows, so its answer jumps 4–8 semitones.

## Beat 1 loosened (Sam, 2026-10-24: "your rule to not play on beat one is too literal and strict")

Measured before: the phrase plan struck beat 1 in 17-32% of bars, and 46% of songs first struck it in bar 4. Sam's
only beat-1 direction had been "vary that a bit"; the stacked rules (motif 65% off 1, a 1-bar motif never on 1, beat 1
resting every second bar, tie-ins, the hybrid on 1 at 30% of chord changes) made it rare. His "bar" directions were
reviewed: all were bars, none meant beats.

- The motif starts on beat 1 60% of the time; the "rest every second bar" rules (planner and adapter) are gone;
  questions and arrivals tie in 1 time in 4, else struck on 1. Beat 1 is struck in 68% of sustained lead bars.
- The hybrid lands on 1 at 55% of chord changes (25% late, 20% pushed).
- Ruling: restatement contour kept >= 85% (was 90%) plus zero reversals: a restated note struck on beat 1 snaps to a
  chord tone more often, and where the next step would rub it repeats instead (46 cases, all step <-> repeat, none
  turned the other way). The snap now weights the planned direction 16 (was 8), a repeat 8. Cost if wrong: restatements
  that sound less literal.
- Levels: the lead plays more, so trims: lead bus -0.4 dB, sax -1.3 dB, flute -0.5, muted trumpet -0.3, saw lead
  -0.2, synth pluck -0.3. The break's low-pass on the lead is 0.85 of the others' (it stays over the darkened keys:
  Rhodes neo-soul seed 4 dipped to +1.0 dB in its break). Two level tests pull against each other here (a break no
  louder than the full band, every song's lead 2 dB over its keys); both pass with thin margins (+2.1 dB).

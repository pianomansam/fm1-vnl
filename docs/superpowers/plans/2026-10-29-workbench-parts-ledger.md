# SDD ledger — plan: docs/superpowers/plans/2026-10-29-workbench-parts.md

Spec: docs/superpowers/specs/2026-10-29-workbench-parts-design.md. Branch: workbench (from 05ee9d3).

## Pre-flight scan

| Tasks | Produces → consumes | Found |
|---|---|---|
| T1 → T5 | WB_OV_*_SALT → per-part 🎲 | consistent |
| T2 → T3/T4/T5 | wb_sections, wb_jump, wb_mute/solo, wb_events* → strip, roll, reactions diff | consistent |
| T3 → T5 | desk.listen, snapshot listen → per-part feedback | consistent |
| T4 → T5 | roll redraw on landing → re-roll display | consistent |
| T2 self | spec put mute in the glue; plan adds a default-off vx_mute engine hook | ruled in the plan (the glue can't cut sounding notes) |
| T1, T3, T4, T5 self | consistent | — |

## Progress
Task 1: dispatched (BASE 05ee9d3, implementer af45ac64, opus)
Scope change (Sam, 2026-10-29): remove the ear candy part entirely; the pad must be able to be absent, and seeds shouldn't always have it; the leads muted trumpet, sax, vibes, guitar lick, string lead and cello get replaced with real (sampled) instruments or removed.
Ruling: candy_salt dropped from Task 1 (message sent to af45ac64); the candy part comes out of Task 5's parts; candy removal and the optional pad go into a new engine task after Task 1 (needs Sam's pad rate); the real-leads question goes to a research survey first (a93c3d99), then in-song demos — cost if wrong: re-plan of Task 5's part list
Sam (2026-10-29): pad in about two of three songs — approved; and the pad can drop out of certain sections (new). Folded into the new engine task (candy removal + optional pad).
Sam approved the pad-section rule: in a pad song each section draws pad in/out, present ~2 of 3 sections, likelier out in intros/breaks, likelier in for lead sections and main grooves, never out more than 2 sections in a row, fades at the section line.
New Task 1b (after Task 1): Engine — remove ear candy entirely (no V_CANDY events; set/mood candy fields unused; firmware too); pad per song ~2/3 (seed stream), pad per section per the rule above; tests; workbench options/rows drop candy. Then Task 5 drops candy from parts and handles an absent pad (part shown "no pad in this song", 🎲 can't add it unless the pad-on row is set).
Task 1: implementer DONE_WITH_CONCERNS b44a62f (candy salt dropped). Ruling: the ride section stays chosen from the as-rolled bass walk under a bass salt — keeps the drums unchanged, as the spec's 'only its part' wants — cost if wrong: the ride section may sit over a non-walking stretch after a bass re-roll
Task 1: review Approved. Ruling: bass_salt's lead (3/40) and keys (1/40) reactions accepted — the spec allows reactions the page reports — cost if wrong: none
Task 1: minor (deferred): REACTS mask unbounded (cap lead/keys moves, e.g. ≤5/40); duplicated pinned-hash check without printed hashes; stale clash_test comment (544-546); ⚠️ old 8-entry ov snapshots must restore (check in T2/T5)
Task 1: complete (commits 05ee9d3..b44a62f, review clean)
Task 1b: dispatched (BASE f67842d, implementer a8e55b1b, opus)
Task 1b: implementer DONE_WITH_CONCERNS b4e00ff (DUCK_PAD mix fix; fade in engine; pad tests force pad on; hashes 2341dc17/dd40694f). Review dispatched (ab8267e8).
Task 1b: review Approved (spec ✅, quality approved). vnl_host runs the app via the fm1-base host sim, so new-song.sh renders get the fade.
Task 1b: minor (deferred): Motion rng shift moves 1-bar drum drops (old drop feedback points elsewhere); fade direction may flip on a re-merge at a boundary (rare); song_tick comment should say renderers call vx_set_pad_level; t1b_silent oracle mirrors pad_part; cost entry needs a quiet-host re-measure
Task 1b: for Sam's ear: pad fades at keys-out lines; lead plays every phrase where candy traded fours; DUCK_PAD ~1 dB
Task 1b: complete (commits f67842d..b4e00ff, review clean). constraints.md hashes updated.
Task 2: dispatched (BASE b4e00ff, implementer a4a098e5, opus)
Task 2: implementer DONE_WITH_CONCERNS 363b28c (drum tails fade on unmute; solo wins; jump bars 0-based; wb-worklet.js touched; test-only exports)
Task 2: review Approved (spec ✅, quality approved).
Task 2: Ruling: a staged jump plus a new seed/map — Task 3's page drops any staged jump when a seed or map change is staged (the new song starts at bar 1) — keeps a jump from landing at a bar picked from the old song's sections — cost if wrong: Sam re-clicks the section
Task 2: minor (deferred): unmuting the lead cuts its voices (ends the duck early, may drop a glide); drum tail ≤40 ms at unmute; mute forces the per-bus path (cost unmeasured, docs/cost.md line); wb_catchups comment stale; len_steps off by a 16th under swing; no pad mid-note unmute test
Task 2: complete (commits b4e00ff..363b28c, review clean)
Task 3: dispatched (BASE 363b28c, implementer a2bed145, sonnet)
Task 3: implementer DONE_WITH_CONCERNS 3285da6 (jump drop via wb_jump(0); position.section untested; playhead per bar)
Task 3: review Needs fixes (1 Important: wb_apply/wb_jump(0) race). Fix round 1 → implementer resumed (engine-side jump clear on seed/map; positionOf; strip await). Minor (deferred): no blocked-change-keeps-jump test; playhead per bar; narrow-screen part grid
Task 3: fix round 1 → 805ef3e; re-review: all addressed. Minor (deferred): heard sections refresh a few ms after a landing; highlight matched by start/bars; same-seed restore keeps a jump (untested)
Task 3: complete (commits 363b28c..805ef3e, review clean; engine suite rerun pending at dispatch of Task 4)
Task 4: dispatched (BASE 805ef3e, implementer abe6edfb, sonnet)
Task 4: implementer DONE_WITH_CONCERNS 979d613 (no chord-change lines; 68-bar test song). Review dispatched (af1950f5).
Task 4: review Needs fixes (chord lines missing; roll() drops redraw while busy). Fix round 1 → implementer resumed (also 76-bar seed, mute/solo grey asserts, bars from sections). Minor (deferred): redraw key cost (bytes.join); devicePixelRatio
Task 3: engine suite rerun at 805ef3e → all tests passed
Task 4: fix round 1 → 2a870cf; re-review: all addressed. Minor (deferred): rollAgain untested; draw timed on a stub canvas only; position check partly tautological; color bumps on later passes draw a chord line
Task 4: complete (commits 805ef3e..2a870cf, review clean)
Task 5: Ruling: no candy part; pad 🎲 disabled when the song has no pad (salt kept); pad on/off doesn't reset salts — candy removed by Sam; keeps a pad roll across on/off — cost if wrong: a disabled button Sam expected to work
Task 5: dispatched (BASE 2a870cf, implementer a8b4f726, opus)
Task 5: implementer DONE_WITH_CONCERNS 3b2f3e8 (hand-set salts reset on new song; keys 🎲 silent on seeds 21/5; reactions w/o reason; reactions at commit)
Task 5: review Needs fixes (dead keys 🎲 in ~17% of songs, 100% strum — engine salt only moves the first chord's spread and loop_cycle reconverges; keys-room reason never fires). Fix round 1 → implementer resumed (retry ≤8 salts, refuse if none changes the part, no no-op history; WHY.keys; drums history under locked seed; kick overwrite message; smoke assert)
Task 5: Ruling: no engine change to widen the voicing salt's reach in this plan — the workbench refuses dead rolls instead; tell Sam the keys voicing re-roll is narrow (harmony.c loop_cycle) — cost if wrong: Sam wants more voicing variety and it's a separate engine task
Task 5: minor (deferred): hand-set phrase/voicing rows reset on new seed unless locked (tell Sam); reactions show at commit; set() runs a full silent render (cost unmeasured, docs/cost.md); dropdown hand edits skip reactions
Task 5: fix round 1 → 577ab43; re-review: all addressed. Minor (deferred): restore() doesn't clear dead[] (refused 🎲 stays off until next change); seed/family re-enable untested
Task 5: complete (commits 2a870cf..577ab43, review clean)
Final review: dispatched (range 0bc185e..577ab43, opus, a52468e9)
Final review: ready after fixes (2 Important: velocity-only rolls count as new; locked drums lose the kick). Re-grade: finding 3 (drums 🎲 near-silent with kick locked) folds into fix 1; 5 and 6 cheap, included. Fix wave → Task 5 implementer resumed.
Final: minor (deferred): ~7% of pad songs lose the pad >2 sections running (keys-out/no-solo-voice silences not counted by the rule) — tell Sam
Final: Ruling: the pad 🎲 may change the pad's solo instrument (stream shared in pads.c) — left as is and put to Sam — cost if wrong: a pad roll swaps the instrument; one-line engine split later
Final: fix wave 3674803; re-review all addressed. Smoke, server tests, engine suite green.

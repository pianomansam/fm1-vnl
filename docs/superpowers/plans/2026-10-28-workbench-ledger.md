# SDD ledger — plan: docs/superpowers/plans/2026-10-28-workbench.md

Spec: docs/superpowers/specs/2026-10-28-workbench-design.md. Branch: workbench (from gpl at d7d9a4f).

## Pre-flight scan

| Tasks | Produces → consumes | Found |
|---|---|---|
| T2 ↔ T3 | both edit harmony.c, lead.c, groove.c | sequential; T3 builds on T2's lib_lane_hits reads; consistent |
| T2 → T5 | LIB_SET_WB, lib_set_wb → wb_role_set | consistent |
| T2 → T10 | LIB_PATTERN_WB, two_bar, hit_b/ghost_b → wb_pat_step/wb_pat_load | consistent |
| T3 → T5/T6 | song_ov_t, song_override → wb_ov_set keys WB_OV_* | consistent (T5 names the keys) |
| T4 → T8 | drums_hook_t, drums_hook → wb_drums | consistent |
| T5 → T6-T10 | wb.c exports, smoke.mjs, server.py, page | sequential edits of shared files; consistent |
| T6 → T10 | options.json kickPhrases (k2-k6) → kick-phrase presets | consistent |
| T6 ↔ T8/T9 | options.json kits ← new kit presets | T6 runs before the kits exist: see ruling 1 |
| T7 → T10 | POST /kit, /pattern → save buttons | consistent |
| T8 → T9 | WB_SRC_SAMPLE declared in T8, implemented in T9 | consistent |
| T1 self | re-pin only Sam-approved changes | consistent |
| T2 self | tests use existing pinned-hash helper (sf_songs) | consistent |
| T3 self | "pads.c or wherever harmony_pad_place lives" | harmony_pad_place is in harmony.c/h; fine |
| T4 self | ok | consistent |
| T5 self | smoke needs RF_* indices in JS | ruling 2 |
| T6 self | kit row before kits exist | ruling 1 |
| T7 self | ok | consistent |
| T8 self | spike sources live in scratchpad (may be cleaned) | brief says re-clone if gone; fine |
| T9 self | fixture SFZ + tiny wavs under workbench/tools/fixtures | consistent |
| T10 self | ok | consistent |

Ruling 1: T6's kit row lists the kits that exist at that point (Brushes approved; the retired kits behind "show retired"); T8 and T9 add their presets to options.json — the plan orders tasks that way — cost if wrong: none (T8/T9 touch options.json anyway).
Ruling 2: wb.c also exports `int wb_field(const char *name)`? No: simpler — wb.h's enums are mirrored once in workbench/wb-consts.js (generated or hand-kept, checked by smoke against wb_roll_get bounds) — implementer's choice between a tiny exported table (`wb_consts()` JSON string) and a JS mirror; must be single-source or tested — cost if wrong: a wrong field index, caught by smoke.

## Progress
Task 1: dispatched (BASE d7d9a4f, implementer aaca240c)
Task 1: implementer DONE_WITH_CONCERNS e5f4524 (vibes v+=60; lead-over-keys floors loosened); review dispatched (a67fd914)
Task 1: review Needs fixes (3 Important: lead-over-keys floors loosened, pad band widened, vibes +60 saturates; minors: strum +2 evidence, /85, long lines). FIX_BASE e5f4524. Round 1 sent to aaca240c
Task 1: fix round 1/5 (6 addressed, 0 open; commits e5f4524..37722ee)
Task 1: minor (deferred): new long lines with trailing comments in voices.c keys_gain (~:62, :65) and song_test.c:185
Task 1: minor (deferred): thin margins (Knight+sax lead median 8.1; Knight+flute pad 7.0/7.1); strum-lead tolerance +2 rests on medians recorded in a comment
Task 1: note for Sam: mix retuned by measurement (keys_gain −0.75 dB Rhodes/Wurli/guitars held+half, Knight +0.75, strum −1.5, vibes lead +1.5 and velocity 94-110 arc) — he should hear vibes (2, 8, 26), Rhodes and Knight sets
Task 1: complete (commits d7d9a4f..37722ee, review clean)
Task 2: dispatched (BASE 37722ee, implementer a6beac09)
Task 2: implementer DONE_WITH_CONCERNS d0bc7bf. Ruling: LIB_PATTERNS initializers got ', 0, {0}, {0}' (-Wmissing-field-initializers under -Werror) — the plan's 'stay valid' meant values unchanged — cost if wrong: none
Task 2: review Needs fixes (Important: committed library.c-E backup). Ruling: the id-keyed set rules (lib_set_walks, lib_set_keys_shift) must honor a custom set's base id — the workbench copies sets and must sound like them — cost if wrong: a small API addition. FIX_BASE d0bc7bf. Round 1 sent to a6beac09
Task 2: note for later tasks: brace initializers of lib_pattern_t must spell out two_bar/hit_b/ghost_b under -Werror
Task 2: fix round 1/5 (3 addressed, 0 open; commits d0bc7bf..52b12aa)
Task 2: minor (deferred): no RED evidence for the wb_ tests (first failure was the initializer error)
Task 2: complete (commits 37722ee..52b12aa, review clean). Interface: lib_set_wb(const lib_set_t *s, uint8_t base_id)
Task 3: dispatched (BASE 52b12aa, implementer a65ca0fb, opus)
Task 3: implementer DONE_WITH_CONCERNS 16e5c2e. Ruling: test_ov_phrase_salt forces CS_HELD — the brief's "keys identical" holds only where the comping doesn't make room for a busy lead (Sam's rule); drums and bass identical across all 40 with rolled comping — cost if wrong: none (the engine behavior is intended)
Task 3: note for T6: the engine doesn't refuse nonsensical overrides (e.g. strum on keys); the workbench's dropdowns must offer only valid values per instrument; instrument ranges for keys_oct/lead_oct are approximate (song_ov_range in override.c) — Sam may tune
Task 3: review Needs fixes (Important: keys_oct +1 starves the lead over percussive keys; minors: negative registers + clash coverage for oct, lead_style test indirect). ⚠️ firmware build not run — controller runs it at the final review. FIX_BASE 16e5c2e. Round 1 sent to a65ca0fb
Task 3: fix round 1/5 (3 addressed, 0 open; commits 16e5c2e..0fc1a36)
Task 3: minor (deferred): dense ternary lead.c:62; clash's widened lead range under a register override allows 59/88 bounds; keys +1 refused over leads topping under 94 (judgment number); test_ov_phrase_salt forces CS_HELD (ruled)
Task 3: complete (commits 52b12aa..0fc1a36, review clean)
Task 4: dispatched (BASE 0fc1a36, implementer aaf2edd0)
Task 4: review Approved. Ruling: drums_hook_t.render WRITES into x (the existing drums_render contract), not "adds" as the plan said — matches every call site — cost if wrong: none; T8 must follow "writes"
Task 4: minor (deferred): test style/comment punctuation in test_drums_hook (song_test.c:154-166)
Task 4: complete (commits 0fc1a36..2ef7d9b, review clean)
Task 5: dispatched (BASE 2ef7d9b, implementer a3a090bb, opus)
Task 5: implementer DONE 2e5b7ec. Ruling: song end = release, 2 s gap, loop the same song from bar 1 (unspecified by the plan; a workbench keeps playing what's being judged) — cost if wrong: one branch
Task 5: review Approved. ⚠️ resolved: wb_seed replaces the whole staged roll and custom set (overrides carry) — Ruling: Task 6's page owns "locks and hand edits survive a regenerate": after wb_seed it re-applies every locked or hand-set row (roll fields, roles, overrides) before landing — cost if wrong: hand edits lost on reseed, caught by T6's test
Task 5: minor (deferred): smoke lacks a catch-up vs from-scratch envelope comparison (reviewer probe: envelope corr 0.986-0.997 no-op, 0.87-0.95 after key/tempo change); catch-up cost ≤1.6 ms of a 2.9 ms quantum, not in docs/cost.md; wb_consts snprintf chain lacks an n<size clamp (wb.c:330-336); worklet call trap leaves the page's promise unsettled (wb-worklet.js:28-30); failed boot never shown (index.html:46-49); misleading "shortest song" comment (smoke.mjs:146); wb_role_get copies lib_set_t; WASI loader duplicated in smoke and worklet
Task 5: complete (commits 2ef7d9b..2e5b7ec, review clean)
Task 6: dispatched (BASE 2e5b7ec, implementer ab759709, opus)
Task 6: implementer DONE_WITH_CONCERNS ca5a850 (extra desk.js + wb exports incl. wb_kick_phrase; lead_oct read-back runs 2 scratch songs per landing ~2 ms; chillwave/vox pads marked synth candidate)
Task 6: review Needs fixes (Critical: seed restart lands before reapply/undo; Important: mid-change bar line lands half-staged state; lead_oct read-back on audio thread every landing; minors 4-7). Ruling: Chillwave pad and Vox pad are approved (Sam tuned them), family synth — cost if wrong: two options shown. FIX_BASE ca5a850. Round 1 sent to ab759709
Task 6: implementer interrupted by a session restart mid round 1 (work uncommitted in tree); resumed ab759709 to finish
Task 6: fix round 1/5 (7 addressed, 0 open; commits ca5a850..27c875f)
Task 6: minor (deferred): interleaving smoke checks only the tempo lock, not the locked lead role; with the seed locked, rerollAll releases hand edits up front
Task 6: complete (commits 2e5b7ec..27c875f, review clean). Interfaces: stage_t (st/com/live/mark), wb_apply commits st→com, wb_stage_bytes, wb_lead_oct_probe on a main-thread probe instance (wb.js probe), desk.js holds page logic, options.json kits have "ride": true for sampled kits
Task 7: dispatched (BASE 27c875f, implementer a76e6204)
Task 7: implementer DONE 0eccf4f
Task 7: security review finding (Medium, valid): server.py POST accepts cross-origin writes (a web page Sam visits could POST to localhost:8732 and write feedback/kits/patterns). Ruling: fix in Task 7's fix round — reject POST unless Origin is absent or http://localhost:<port>/http://127.0.0.1:<port>, Host is localhost/127.0.0.1:<port>, and Content-Type is application/json — cost if wrong: none
Task 7: review Needs fixes (Important: negative Content-Length bypasses cap; cross-origin writes (= security finding); snapshot records unheard staged state; minors: dead-letter, dedupe id, changes growth, atomic kit write). FIX_BASE 0eccf4f. Round 1 sent to a76e6204
Task 7: fix round 1/5 (3 addressed + minors, 1 open — no negative/missing Content-Length test; commits 0eccf4f..6473d0f)
Task 7: minor (deferred): snapshot's two calls (wb_pending, wb_live_state) can straddle a landing; locks/hand taken from the current desk, not the heard state; dedupe needs an id
Task 7: round 2 sent to a76e6204 (length tests; sentChanges ordering). FIX_BASE 6473d0f
Task 7: fix round 2/5 (2 addressed, 0 open; commits 6473d0f..1e7e805)
Task 7: minor (deferred): index.html:194-195 — the round-2 cursor change lets overlapping feedback clicks log the same changes twice; revert to slicing+advancing synchronously before the await (6473d0f ordering)
Task 7: complete (commits 27c875f..1e7e805, review clean). Interfaces: desk.snapshot() async {version:1, rows (heard), pending, locks, hand, seed, drums:{}}; desk.restore(); wb_live_state(), wb_pending(); POST /feedback,/kit,/pattern (JSON only, same-origin, Host checked, name [a-z0-9-]{1,40}, body {name,...})
Task 8: dispatched (BASE 1e7e805, implementer a3e6b724, opus)
Task 8: implementer DONE_WITH_CONCERNS 7343408 (808 level ±1.5 dB around target over 6 songs; first lane edit on a song with a retired kit silences other lanes until T9; extra wb_opt.c/.h and poketrack samples stub)
Task 8: review Approved. Important (latent, load-bearing for T10's lane picker): the first lane edit copies only KIT_BRUSHES; on any other kit the other lanes go silent. Ruling: Task 9 fixes it — the first edit copies the song's current kit into lanes for every kit (BUILTIN for the non-sampled built-in kits 0-3 and Brushes; SAMPLE for sampled kits once T9's sample sets exist; the retired mixed sampled kits may map to their nearest shipped source or be refused with a message, never a silent lane) — cost if wrong: a silent lane in T10
Task 8: minor (deferred → carried into T9 dispatch): wb_lane_db accepts NaN (wb.c ~:241); no-lanes audition plays drums.c's live kit, and BUILTIN hits leave drums.c's kit at Brushes; cost figure is an average (log a worst quantum); weak smoke asserts (audition while playing; override hash on one seed); open-hat choke ignores mixed sources; LO-FI lane 12 is labeled "BOOMBAP COWBELL" upstream — label it LO-FI cowbell in pickers
Task 8: complete (commits 1e7e805..7343408, review clean)
Task 9: dispatched (BASE 7343408, implementer a931756b, opus; carries T8's first-edit copy fix + minors)
Task 9: implementer interrupted by a session restart (work uncommitted in tree, no report); resumed a931756b to finish
Task 9: implementer DONE_WITH_CONCERNS c59ca51. Ruling (implementer's call, accepted): retired mixed sampled kits refuse the first lane edit with a message (their hits are processed copies partly from unshipped sources) — never a silent lane — cost if wrong: Sam picks a shipped kit first
Task 9: review Needs fixes (Important: sample alloc/copy on the audio thread, unmeasured; minors: prep path containment, SHA check, alloc 0, random seed, load-failure test, stageDrums cause; ⚠️ brush CC mapping → implementer verifies vs spike). Ruling: workbench costs go in workbench/README.md, not docs/cost.md — cost if wrong: one doc move. FIX_BASE c59ca51. Round 1 sent to a931756b
Task 9: minor (deferred, To revisit): BUILTIN lanes from two synth kits share drums.c's table (retired kits only)
Task 9: fix round 1/5 (all addressed, 0 open; commits c59ca51..50d71cc)
Task 9: minor (deferred): an alloc failure leaves a rejected cached promise that stops preload at that set; structured-clone and browser memory.grow cost unmeasured; pinned-SHA check untested
Task 9: complete (commits 7343408..50d71cc, review clean). Interfaces: wb_sample_alloc/wb_sample_set/wb_sample_piece; Samples.preload at boot; kit_lanes copies the song kit on first lane edit (retired mixed kits refuse)
Task 10: dispatched (BASE 50d71cc, implementer a132dd60, opus)
Ruling (Sam, 2026-10-28): skip the firmware build check for this branch — run ./build.sh before the next device install instead — cost if wrong: a firmware compile error found later
Task 10: implementer DONE_WITH_CONCERNS 8e90776 (GET /list/kits|patterns; toms/crash note only; poly3 shaker edit becomes a 2-bar loop; kit lock doesn't block lane edits)
Task 10: review Needs fixes (Important: lane sound/level edits lost when the kit is kept/locked on reseed; minors: lane() via drumEdit, wb_pat_load legality, poly3 test, long comment). Ruling: toms/crash row is a note without controls — the engine has no tom/crash lane, controls would be silent ("audible or cut") — tell Sam — cost if wrong: add controls when the drummer layer adds the lanes. Ruling: a shaker edit clearing poly3 accepted (documented). FIX_BASE 8e90776. Round 1 sent to a132dd60
Task 10: fix round 1/5 (all addressed, 0 open; commits 8e90776..379b86f)
Task 10: minor (deferred): set()'s grid restage ignores stageGrid's result — a kept grid whose base turns illegal after a reseed is silently dropped
Task 10: complete (commits 50d71cc..379b86f, review clean)
Final: dispatched whole-branch review (d7d9a4f..379b86f)
Final: review "Ready with fixes" — Important: (1) Load kit on a mixed-sampled-kit song silently fails and reports success, pins the retired kit; (2) 84% of seeds play a retired kit by default (Dusty/Soft/808/mixed sampled); (3) feedback snapshot not queued behind in-flight changes. Fix before merge: worklet call trap; set() grid restage result. Minors: lock toggle not queued; ride lane editable on non-sampled kits; kit row name after Load kit; save overwrites silently; GET lacks Host check. For Sam: T1's mix retune changes firmware sound (voices.c gains, vibes arc).
Final: Ruling on (2): a rolled retired kit is replaced on landing by an approved kit of the song's family (rows.js pickFor, deterministic per seed) unless the kit row is kept (locked/hand) — Sam reset the drums ("None of our 3 kits should remain"; original kits only) and demos use approved parts only — cost if wrong: one rule; "show retired" still lets Sam pick them
Final: one fix wave dispatched
Final: fix wave re-review clean (commits 379b86f..3f87e16). Minor (deferred): a loaded kit file's kit swap isn't highlighted or logged as a kit-row change and skips conflicts(); a note is lost if the in-queue snapshot throws

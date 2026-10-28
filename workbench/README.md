# VNL-1 Workbench

A local page for auditioning the song engine: the engine (app/song, app/engine) built to WebAssembly and played in an
AudioWorklet. Local only; not the FM-1, so it may exceed the device's budgets.

    sh workbench/build.sh            # -> app/build/workbench/ (wb.wasm and the page)
    python3 workbench/server.py      # http://localhost:8732/ (127.0.0.1 only; a port as the argument)
    node workbench/tools/smoke.mjs   # the engine glue's tests, no browser

## How it fits

- `engine/wb.c` (`wb.h`): the glue the page calls. One song plays; the page stages changes (roll fields, the custom
  set's instruments, the per-song overrides, the kit, a kick phrase; a seed too) and `wb_apply` commits them: only a
  commit is heard, at the next bar line by catch-up: the song
  restarts silently with the staged state, ticks to that bar, and sound resumes, so every state is as if the song had
  always had those decisions. A seed or a new arrangement map restarts from bar 1; a change in the last bar does too.
  The song loops after a 2 s gap at its end.
- `wb_consts()` exports the engine's indices (RF_*, WB_OV_*, the roles, LIB_SET_WB) as JSON, so the page never keeps
  its own copy.
- `wb-worklet.js` runs wb.wasm (base/web/emu/worklet.js's pattern, without the device); `wb.js` is the page's side;
  `index.html` the page: the transport and the decision rows (layout A).
- `rows.js`: the rows, what each dropdown offers (from `options.json`; retired options only behind "show retired"),
  the dice (an approved option of the song's family) and what a value forces (a row it moves, or the lock that blocks
  it). `desk.js` drives the engine with them: every change is staged, checked against the locks (undone whole if a
  locked row would move) and landed; a seed or map change stages every locked or hand-set row again. The lead register's read-back runs scratch songs in a second wb.wasm on the main
  thread (`wb.js`'s probe), never on the audio thread.
- `options.json`: the hand-maintained option lists (instruments, kits, comping, patterns, kick phrases, families),
  each approved, candidate or retired, seeded from the ledgers.
- Feedback: 👍/👎 and a note, on the song or a row, save a line to `workbench/feedback.jsonl`: a versioned snapshot
  (every row, the locks, the seed; `desk.snapshot()` / `desk.restore()`), where the song was, and the log of every applied
  change. Entries queue in localStorage while the server is down (the page says "not saved yet") and are resent.
  `node workbench/tools/replay.mjs <line number | snapshot.json> out.wav [seconds]` renders one through wb.wasm.
  The snapshot is the song being heard (a change still waiting for its bar line is under `pending`); `changes` are those
  since the previous entry, stamped with the position at the click; entries carry an id the server won't write twice;
  ones the server refuses (4xx) are kept in the browser's `wb-feedback-dead`. The server takes POSTs only from its own
  page (Host, Origin and a JSON content type are checked).
- `server.py` also takes `POST /feedback`, `/kit` and `/pattern` (names `[a-z0-9-]{1,40}`, bodies up to 1 MB), writing
  only under `workbench/`; its tests: `python3 workbench/tools/test_server.py`.
- `server.py` serves the build first, then `workbench/` (kits, patterns, samples, options.json).

The smoke test is the evidence that the engine renders and lands changes; whether it sounds right needs a listen in
Chrome or Safari.

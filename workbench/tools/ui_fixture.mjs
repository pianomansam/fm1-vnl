// SPDX-License-Identifier: GPL-3.0-or-later
// The mock's values for a few fixed states, as a C header for tests/host/ui_test.c: the device's value bag
// (app/ui/values.c) must give the same text, flags and fractions. The states: seed 72 at bar 14 (13 from 0), playing,
// everything as rolled, on the main screen (the bass tile highlighted), with a tempo change waiting, on part screens;
// and stopped with the song ready. The page's rows and labels are built as index.html builds them (rows.js's
// optionsFor, ui/rolled.js's names from wb_rolled), every kit available, as on the device with its sample bank.
//   sh workbench/build.sh && node workbench/tools/ui_fixture.mjs [--check]
import { readFileSync, writeFileSync } from "node:fs";
import * as D from "../ui/device.js";
import data from "../ui/screens.js";
import { ROWS, row, optionsFor, fromEngine, familyOf, rhythmRow } from "../rows.js";
import { rolledNames } from "../ui/rolled.js";

const OUT = new URL("../../tests/host/ui_values_fixture.h", import.meta.url);
const opts = JSON.parse(readFileSync(new URL("../options.json", import.meta.url), "utf8"));
const WASM = readFileSync(new URL("../../app/build/workbench/wb.wasm", import.meta.url));
let mem;
const stub = new Proxy({ fd_write: (fd, iov, n, out) => { new DataView(mem.buffer).setUint32(out, 0, true); return 0; } },
                       { get: (t, k) => (k in t ? t[k] : () => 0) });
const { instance } = await WebAssembly.instantiate(WASM, { wasi_snapshot_preview1: stub, env: new Proxy({}, { get: () => () => 0 }) });
const ex = instance.exports;
mem = ex.memory;
ex._initialize?.();
ex.wb_init();
const json = (p) => { const b = new Uint8Array(mem.buffer, p); let s = ""; for (let i = 0; b[i]; i++) s += String.fromCharCode(b[i]); return JSON.parse(s); };
const C = json(ex.wb_consts()), lib = json(ex.wb_library());

const SEED = 72, BAR = 13;
ex.wb_seed(SEED);
ex.wb_apply();
const st = json(ex.wb_state()), values = fromEngine(st, C, opts);
values.family = familyOf(values, opts);
const legal = { prog: lib.progs.map((p) => p.id).filter((id) => ex.wb_roll_legal(C.RF.PROG, id)) };
const state = { values, legal, unavailable: new Set() };
const rn = rolledNames(json(ex.wb_rolled()), state, opts);
// index.html's labels() and rows()
const name = (id, as = id) => {
  const v = values[id], o = optionsFor(row(id), state, opts, { showRetired: true, lib }).find((y) => String(y.value) === String(v));
  const n = String(o?.short ?? o?.name ?? v ?? "").toUpperCase();
  return o?.rolled && rn[as] ? rn[as] : n;
};
const labels = { ...Object.fromEntries(ROWS.map((r) => [r.id, name(r.id)])), rhythm: name(rhythmRow(state, opts), "rhythm") };
const choices = (id, as = id) => optionsFor(row(id), state, opts, { lib })
  .filter((o) => !(row(id).source === "kits" && state.unavailable.has(o.value)))
  .map((o) => { const n = String(o.short ?? o.name).toUpperCase(); return { value: o.value, name: o.rolled && rn[as] ? `${n} (${rn[as]})` : n }; });
const rh = rhythmRow(state, opts), ids = ROWS.filter((r) => r.kind !== "seed" && r.kind !== "reroll").map((r) => r.id);
const rows = { values: { ...values, rhythm: values[rh], loop: 0, endless: 0, move: 0 }, rolled: rn,
               options: { ...Object.fromEntries(ids.map((id) => [id, choices(id)])), rhythm: choices(rh, "rhythm"),
                          loop: [{ value: 0, name: "OFF" }, { value: 1, name: "ON" }],
                          endless: [{ value: 0, name: "OFF" }, { value: 1, name: "ON" }],
                          move: [{ value: 1, name: "DRUM FILL" }, { value: 2, name: "COMP FILL" }, { value: 0, name: "OFF" },
                                 { value: 3, name: "STUTTER SIMPLE" }, { value: 4, name: "STUTTER COMPLEX" }] } };
const roll = Object.fromEntries(["KEY", "TEMPO", "MOTION", "MOOD", "MODE", "BARS"].map((k) => [k.toLowerCase(), st.roll[C.RF[k]]]));
const sections = json(ex.wb_sections(0));

// the names the phase-1 screens (the main and part screens) read
const PHASE1 = Object.keys(data.screens).filter((id) => id === "main" || id.startsWith("p"));
const names = new Set();
const walk = (x, k) => {
  if (Array.isArray(x)) x.forEach((v) => walk(v, k));
  else if (x && typeof x === "object") for (const [kk, v] of Object.entries(x)) walk(v, kk);
  else if (typeof x === "string") {
    for (const m of x.matchAll(/\{(\w+)\}/g)) names.add(m[1]);
    if (k === "when" || k === "value") names.add(x.replace(/^!/, ""));
  }
};
for (const id of PHASE1) walk(data.screens[id]);
for (const k of Object.keys(data.lines)) names.add(k);
for (const f of Object.keys(data.fields)) for (const s of ["", "Frac", "Pend", "First", "Last"]) names.add(f + s);

const CASES = [
  { name: "main", screen: "main", cursor: 2, playing: 1 },
  { name: "pending", screen: "main", cursor: 0, playing: 1, reqs: { tempo: [86, 16] } },
  { name: "comp", screen: "pComp", cursor: 0, playing: 1, reqs: { compReg: [1, 16] } },
  { name: "comp_more", screen: "pCompMore", cursor: 0, playing: 1 },
  { name: "drums", screen: "pDrums", cursor: 0, playing: 1 },
  { name: "song_more", screen: "pSongMore", cursor: 0, playing: 1 },
  { name: "song3", screen: "pSong3", cursor: 0, playing: 1 },
  { name: "lead", screen: "pLead", cursor: 0, playing: 1 },
  { name: "stopped", screen: "main", cursor: 0, playing: 0 },
];
const facts = json(ex.wb_screen(BAR));
const ident = (s) => s.replace(/-/g, "_MINUS").replace(/\+/g, "_PLUS").replace(/(?<=[a-z0-9])(?=[A-Z])/g, "_").toUpperCase();   // gen.py's
const esc = (s) => JSON.stringify(String(s)).replace(/[^\x20-\x7e]/g, (c) => `\\${c.charCodeAt(0).toString(8).padStart(3, "0")}`);
let out = `/* SPDX-License-Identifier: GPL-3.0-or-later */\n/* generated by workbench/tools/ui_fixture.mjs: the mock's values (valuesOf) in fixed states, seed ${SEED}. ` +
          `Never edit this file. */\n#define UI_FX_SEED ${SEED}\n#define UI_FX_BAR ${BAR}\n`;
const list = [];
for (const c of CASES) {
  const sc = data.screens[c.screen], reqs = Object.fromEntries(Object.entries(c.reqs ?? {}).map(([f, [v]]) => [f, v]));
  let pend = null;
  for (const [f, [v, at]] of Object.entries(c.reqs ?? {})) if (!pend || at < pend.at) pend = { field: f, val: v, at, bars: at - BAR };
  const shown = c.playing
    ? { hasSong: true, stopped: false, roll, bar: BAR, section: facts.section, map: facts.map, chord: facts.chord, labels, rows,
        mark: true, reqs, partRoll: null, jump: null, cursor: sc.menu ? c.cursor : -1, secOff: 0, sections, pend, msg: null,
        page: sc.pages?.length > 1 ? `${sc.pages.indexOf(c.screen) + 1}/${sc.pages.length}` : "" }
    : { hasSong: false, stopped: true, roll, bar: 0, section: -1, map: json(ex.wb_screen(0)).map, chord: json(ex.wb_screen(0)).chord,
        labels, rows, mark: true, reqs: {}, cursor: sc.menu ? c.cursor : -1, secOff: 0, sections, pend: null, msg: null, page: "" };
  const bag = D.valuesOf(shown, data);
  const vals = [...names].filter((n) => n in bag).sort().map((n) => {
    const v = bag[n];
    return typeof v === "boolean" ? `{${esc(n)}, 1, "", ${v ? 1 : 0}}` : typeof v === "number" ? `{${esc(n)}, 2, "", ${v}}`
      : `{${esc(n)}, 0, ${esc(v)}, 0}`;
  });
  out += `static const ui_fx_val_t UI_FX_${c.name.toUpperCase()}[] = {\n    ${vals.join(",\n    ")},\n};\n`;
  const rq = Object.entries(c.reqs ?? {}).map(([f, [v, at]]) => `{UFLD_${ident(f)}, ${v}, ${at}}`);
  list.push(`    {${esc(c.name)}, US_${ident(c.screen)}, ${c.playing}, ${c.cursor}, ${rq.length}, {${rq.join(", ") || "{0, 0, 0}"}}, ` +
            `UI_FX_${c.name.toUpperCase()}, ${vals.length}}`);
}
out += `static const ui_fx_case_t UI_FX_CASES[] = {\n${list.join(",\n")},\n};\n`;
if (process.argv.includes("--check")) {
  let was = "";
  try { was = readFileSync(OUT, "utf8"); } catch { /* missing */ }
  if (was !== out) { console.error("tests/host/ui_values_fixture.h is stale: run node workbench/tools/ui_fixture.mjs"); process.exit(1); }
  console.log("ui_values_fixture.h is up to date");
} else {
  writeFileSync(OUT, out);
  console.log(`wrote tests/host/ui_values_fixture.h (${CASES.length} states)`);
}

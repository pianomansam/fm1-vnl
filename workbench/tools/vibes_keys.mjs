// SPDX-License-Identifier: GPL-3.0-or-later
// The vibes on the keys (2026-11-05), rendered through wb.wasm as the page plays them, with the lead-over-keys measure
// (song_test's test_lead_over_keys: per bar, the lead bus soloed minus the keys bus soloed, in dB, over the bars a lead
// note starts in where the lead reads over -45 dB and the keys over -60 with a chord struck in the bar; the glue off,
// the bed off) and where the keys sound before the lead and where the lead plays (m:ss). Each render is written with
// the bed (the song's noise and ambience, vinyl where it rolled none) and without it (name-nobed.wav).
//   node workbench/tools/vibes_keys.mjs <out dir> [name ...]   (no names: every render below)
import { existsSync, readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { Desk } from "../desk.js";
import { Samples } from "../wb.js";
import { wav } from "./replay.mjs";

const SR = 44100, Q = 128, VX_OFF_GLUE = 64;
const here = (p) => new URL(p, import.meta.url);
const JSON_EXPORTS = new Set(["wb_consts", "wb_state", "wb_live_state", "wb_library"]);
const VIBES = 11;

// name: { set, seed, kit (null: rolled), keys (null: the set's), comp (-1 rolled: else CS_*), measure (measured, not
// rendered)
export const RENDERS = {
  "song1-vibes-flute-city-pop": { set: 34, seed: 6, kit: 20, keys: null, comp: -1 },
  "song2-vibes-felt": { set: 36, seed: 14, kit: 19, keys: null, comp: -1 },
};

async function load() {
  let mem;
  const wasi = { fd_write: (fd, iov, n, out) => { new DataView(mem.buffer).setUint32(out, 0, true); return 0; }, proc_exit: () => {} };
  const stub = new Proxy(wasi, { get: (t, k) => (k in t ? t[k] : () => 0) });
  const env = new Proxy({}, { get: () => () => 0 });
  const { instance } = await WebAssembly.instantiate(readFileSync(here("../../app/build/workbench/wb.wasm")),
                                                     { wasi_snapshot_preview1: stub, env });
  const ex = instance.exports;
  mem = ex.memory;
  if (ex._initialize) ex._initialize();
  ex.wb_init();
  return ex;
}

const cstr = (ex, p) => {
  const b = new Uint8Array(ex.memory.buffer, p);
  let s = "";
  for (let i = 0; b[i]; i++) s += String.fromCharCode(b[i]);
  return s;
};

// the song set up through the desk (as the page's rows would): { ex, C, desk }
async function setup(spec, bed = false) {
  const ex = await load();
  const call = async (fn, ...a) => {
    if (fn === "wb_stage_bytes") return new Uint8Array(ex.memory.buffer, ex.wb_stage_ptr(), ex.wb_stage_size()).slice();
    if (fn === "wb_write") return void new Float32Array(ex.memory.buffer, a[0], a[1].length).set(a[1]);
    const v = ex[fn](...a);
    return JSON_EXPORTS.has(fn) ? JSON.parse(cstr(ex, v)) : v;
  };
  const C = await call("wb_consts"), lib = await call("wb_library");
  const opts = JSON.parse(readFileSync(here("../options.json"), "utf8"));
  const files = {
    exists: async (p) => existsSync(here(`../${p}`)),
    get: async (p) => {
      if (!existsSync(here(`../${p}`))) return null;
      const b = readFileSync(here(`../${p}`));
      return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
    },
  };
  const samples = new Samples(call, C, files);
  await samples.check(opts);
  ex.wb_seed(spec.seed);
  if (ex.wb_roll_set(C.RF.SET, spec.set) !== spec.set) throw new Error(`seed ${spec.seed}: set ${spec.set} not taken`);
  ex.wb_apply();
  const desk = new Desk(call, C, opts, lib, { samples, rnd: () => 0.5 });
  await desk.read(true);
  const said = [];
  const set = async (id, v) => { const r = await desk.set(id, v); if (!r.ok) said.push(`${id}: ${r.message}`); };
  if (spec.kit !== null) await set("kit", spec.kit);
  if (bed) {
    if (!desk.values.noise && !desk.values.ambience) await set("noise", 1);
  } else {
    await set("noise", 0);
    await set("ambience", 0);
  }
  if (spec.keys !== null) await set("keys", spec.keys);
  if (spec.comp >= 0) await set("comp", spec.comp);
  if (said.length) throw new Error(said.join("; "));
  return { ex, C, desk };
}

function render(ex, n) {
  const l = new Float32Array(n), r = new Float32Array(n);
  ex.wb_play(1);
  for (let at = 0; at < n; at += Q) {
    const k = Math.min(Q, n - at);
    ex.wb_render(k);
    l.set(new Float32Array(ex.memory.buffer, ex.wb_out_l(), k), at);
    r.set(new Float32Array(ex.memory.buffer, ex.wb_out_r(), k), at);
  }
  return { l, r };
}

// per bar energy (dB) of a render
const barDb = ({ l, r }, barLen, bars) => Array.from({ length: bars }, (_, b) => {
  let e = 0, c = 0;
  for (let i = Math.round(b * barLen); i < Math.min(l.length, Math.round((b + 1) * barLen)); i++, c += 2)
    e += l[i] * l[i] + r[i] * r[i];
  return c && e > 0 ? 10 * Math.log10(e / c + 1e-20) : -200;
});

const mmss = (s) => `${Math.floor(s / 60)}:${String(Math.floor(s % 60)).padStart(2, "0")}`;

// spans of bars (sorted) as m:ss-m:ss
function spans(bars, barSec) {
  const out = [];
  for (const b of bars) {
    const last = out[out.length - 1];
    if (last && b <= last[1] + 2) last[1] = b;   // a rest of a bar or two stays in the span
    else out.push([b, b]);
  }
  return out.map(([a, b]) => `${mmss(a * barSec)}-${mmss((b + 1) * barSec)}`);
}

export async function run(name, outDir) {
  const spec = RENDERS[name];
  let { ex, C, desk } = await setup(spec);
  const st = desk.values, bpm = ex.wb_roll_get(C.RF.TEMPO), bars = ex.wb_bars();
  const barSec = 4 * 60 / bpm, barLen = barSec * SR, n = Math.round(bars * barLen);
  // the events: where the lead and keys play
  const ne = ex.wb_events(0), evp = ex.wb_events_ptr(), dv = new DataView(ex.memory.buffer);
  const leadBars = new Set(), keysBars = new Set(), keysHeld = new Set(), keysNotes = [];
  for (let i = 0; i < ne; i++) {
    const o = evp + i * 8, bar = dv.getUint16(o, true), role = dv.getUint8(o + 3), note = dv.getUint8(o + 4);
    if (role === C.V.LEAD) leadBars.add(bar);
    if (role === C.V.CHORD) {
      keysBars.add(bar);
      keysNotes.push(note);
      for (let b = bar; b < bar + Math.max(1, Math.ceil(dv.getUint16(o + 6, true) / 16)); b++) keysHeld.add(b);
    }
  }
  const firstLead = Math.min(...leadBars);
  const exposed = [...keysBars].filter((b) => b < firstLead).sort((a, b) => a - b);
  const mix = render(ex, n);
  if (outDir && !spec.measure) {
    writeFileSync(`${outDir}/${name}-nobed.wav`, wav(mix));
    const b = await setup(spec, true);
    writeFileSync(`${outDir}/${name}.wav`, wav(render(b.ex, n)));
  }
  // the lead over the keys: each bus soloed, the glue off (a fresh instance each, the same song)
  const solo = async (role) => {
    const s = await setup(spec);
    s.ex.wb_fx_off(VX_OFF_GLUE);
    s.ex.wb_solo(role);
    const db = barDb(render(s.ex, n), barLen, bars);
    s.ex.wb_fx_off(0);
    return db;
  };
  const kdb = await solo(C.V.CHORD), ldb = await solo(C.V.LEAD);
  // the test's measure; and over the bars a keys chord is struck in only (the second bar of a two-bar chord left out:
  // the Rhodes there has decayed under the test's -60 dB gate, so it isn't counted, while the vibes still ring at
  // about -50 and are, which lifts the vibes' mean: the struck bars compare like with like)
  let sum = 0, cnt = 0, hs = 0, hc = 0;
  for (let b = 0; b < bars; b++)
    if (leadBars.has(b) && ldb[b] > -45 && kdb[b] > -60) {
      if (keysBars.has(b) || st.keys !== VIBES) { sum += ldb[b] - kdb[b]; cnt++; }   // (the test's lk_keys_sound)
      if (keysBars.has(b)) { hs += ldb[b] - kdb[b]; hc++; }
    }
  const lo = Math.min(...keysNotes), hi = Math.max(...keysNotes);
  const ins = (id) => desk.opts.instruments.find((i) => i.id === id)?.name;
  const comp = [...desk.opts.compRhythms.map((c) => ({ id: c.value, name: c.name })), ...desk.opts.comping]
    .find((c) => c.id === ex.wb_comp_applied())?.name;
  return {
    name, set: spec.set, seed: spec.seed, bpm, bars, seconds: n / SR,
    keys: ins(st.keys), lead: ins(st.lead), bass: ins(st.bass), pad: ins(st.pad), comp,
    kit: desk.opts.kits.find((k) => k.id === st.kit)?.name, pattern: st.pattern, mood: st.mood,
    keysRange: [lo, hi], leadOverKeys: cnt ? sum / cnt : null, bars_measured: cnt,
    leadOverStruckKeys: hc ? hs / hc : null, bars_struck: hc,
    keysExposed: spans(exposed, barSec), leadPlays: spans([...leadBars].sort((a, b) => a - b), barSec),
    leadIns: ins(st.lead),
    ...(process.env.VK_BARS ? { kdb: kdb.map((x) => Math.round(x)), ldb: ldb.map((x) => Math.round(x)),
                                leadBars: [...leadBars].sort((a, b) => a - b),
                                held: [...keysHeld].sort((a, b) => a - b) } : {}),
  };
}

if (process.argv[1] && process.argv[1].endsWith("vibes_keys.mjs")) {
  const [, , outDir, ...names] = process.argv;
  if (outDir) mkdirSync(outDir, { recursive: true });
  for (const name of names.length ? names : Object.keys(RENDERS)) console.log(JSON.stringify(await run(name, outDir)));
}

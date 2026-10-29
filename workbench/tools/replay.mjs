// SPDX-License-Identifier: GPL-3.0-or-later
// Render a feedback snapshot to a WAV through wb.wasm, as the page plays it.
//   node workbench/tools/replay.mjs <feedback.jsonl line number | snapshot.json> out.wav [seconds]
// A line number is 1-based; the snapshot is the entry's, or a file holding one (an entry's "snapshot" is used if present).
// Without seconds: the whole song (up to 10 minutes).
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { Desk } from "../desk.js";
import { Samples } from "../wb.js";

const SR = 44100, Q = 128;
const here = (p) => new URL(p, import.meta.url);
const JSON_EXPORTS = new Set(["wb_consts", "wb_state", "wb_live_state", "wb_library"]);

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

// The snapshot's song as { l, r } Float32 frames (seconds: the song's length if omitted); { l, r, mismatch, listen }:
// listen, the parts muted or soloed as heard ("muted: bass, lead; solo: keys"; "" for every part)
export async function replay(snap, seconds) {
  const ex = await load();
  const call = async (fn, ...a) => {
    if (fn === "wb_stage_bytes") return new Uint8Array(ex.memory.buffer, ex.wb_stage_ptr(), ex.wb_stage_size()).slice();
    if (fn === "wb_write") return void new Float32Array(ex.memory.buffer, a[0], a[1].length).set(a[1]);
    const v = ex[fn](...a);
    return JSON_EXPORTS.has(fn) ? JSON.parse(cstr(ex, v)) : v;
  };
  const C = await call("wb_consts"), lib = await call("wb_library");
  const opts = JSON.parse(readFileSync(here("../options.json"), "utf8"));
  ex.wb_seed(snap.seed);
  ex.wb_apply();
  const files = {   // the sampled kits' files, as the page fetches them from workbench/
    exists: async (p) => existsSync(here(`../${p}`)),
    get: async (p) => {
      if (!existsSync(here(`../${p}`))) return null;
      const b = readFileSync(here(`../${p}`));
      return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
    },
  };
  const samples = new Samples(call, C, files);
  await samples.check(opts);
  const mismatch = await new Desk(call, C, opts, lib, { samples }).restore(snap);
  const bars = ex.wb_bars(), bpm = ex.wb_roll_get(C.RF.TEMPO);
  const n = Math.round((seconds ?? Math.min(bars * 4 * 60 / bpm, 600)) * SR);
  const l = new Float32Array(n), r = new Float32Array(n);
  ex.wb_play(1);
  for (let at = 0; at < n; at += Q) {
    const k = Math.min(Q, n - at);
    ex.wb_render(k);
    l.set(new Float32Array(ex.memory.buffer, ex.wb_out_l(), k), at);
    r.set(new Float32Array(ex.memory.buffer, ex.wb_out_r(), k), at);
  }
  const { mute = [], solo = null } = snap.listen ?? {};
  const listen = [mute.length ? `muted: ${mute.join(", ")}` : "", solo ? `solo: ${solo}` : ""].filter(Boolean).join("; ");
  return { l, r, mismatch, listen };
}

// 16-bit stereo WAV bytes
export function wav({ l, r }) {
  const n = l.length, b = Buffer.alloc(44 + n * 4);
  b.write("RIFF", 0); b.writeUInt32LE(36 + n * 4, 4); b.write("WAVEfmt ", 8);
  b.writeUInt32LE(16, 16); b.writeUInt16LE(1, 20); b.writeUInt16LE(2, 22); b.writeUInt32LE(SR, 24);
  b.writeUInt32LE(SR * 4, 28); b.writeUInt16LE(4, 32); b.writeUInt16LE(16, 34); b.write("data", 36); b.writeUInt32LE(n * 4, 40);
  const s16 = (x) => Math.max(-32768, Math.min(32767, Math.round(x * 32767)));
  for (let i = 0; i < n; i++) { b.writeInt16LE(s16(l[i]), 44 + i * 4); b.writeInt16LE(s16(r[i]), 46 + i * 4); }
  return b;
}

// a feedback line number (1-based) or a JSON file: the snapshot
export function snapshotFrom(arg) {
  if (/^\d+$/.test(arg)) {
    const lines = readFileSync(here("../feedback.jsonl"), "utf8").split("\n").filter(Boolean);
    if (+arg < 1 || +arg > lines.length) throw new Error(`feedback.jsonl has ${lines.length} lines`);
    return JSON.parse(lines[+arg - 1]).snapshot;
  }
  const j = JSON.parse(readFileSync(arg, "utf8"));
  return j.snapshot ?? j;
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const [, , arg, out, secs] = process.argv;
  if (!arg || !out) { console.error("usage: replay.mjs <feedback line number | snapshot.json> out.wav [seconds]"); process.exit(2); }
  const res = await replay(snapshotFrom(arg), secs ? +secs : undefined);
  if (res.mismatch.length) console.error(`replay: the engine kept other values for: ${res.mismatch.join(", ")}`);
  if (res.listen) console.error(`replay: rendered as heard, ${res.listen} (the snapshot's mute/solo)`);
  writeFileSync(out, wav(res));
  console.log(`replay: ${out} (${(res.l.length / SR).toFixed(1)} s)`);
}

// SPDX-License-Identifier: GPL-3.0-or-later
// The workbench's wasm against the same engine built natively (tests/host/wb_native.c): for a seed that lands on each
// approved kit (the device's pick, wb_kit_pick), 20 s of the song from bar 1, both hashed sample for sample. They must
// match: the drum lanes' float code (poketrack's port, Optimist's, the sample bank's kits) renders the same on both.
//   node workbench/tools/kits_native.mjs app/build/tests/wb_native
import { readFileSync } from "node:fs";
import { execFileSync } from "node:child_process";

const SECS = 20, Q = 128;
const native = process.argv[2];
const wasm = readFileSync(new URL("../../app/build/workbench/wb.wasm", import.meta.url));
const opts = JSON.parse(readFileSync(new URL("../options.json", import.meta.url), "utf8"));

async function load() {
  let mem;
  const wasi = { fd_write: (fd, iov, n, out) => { new DataView(mem.buffer).setUint32(out, 0, true); return 0; } };
  const stub = new Proxy(wasi, { get: (t, k) => (k in t ? t[k] : () => 0) });
  const env = new Proxy({}, { get: () => () => 0 });
  const { instance } = await WebAssembly.instantiate(wasm, { wasi_snapshot_preview1: stub, env });
  const ex = instance.exports;
  mem = ex.memory;
  if (ex._initialize) ex._initialize();
  return ex;
}

const ex = await load();
const want = opts.kits.filter((k) => k.status === "approved").map((k) => k.id), seeds = new Map();
for (let seed = 1; seed < 2000 && seeds.size < want.length; seed++) {
  ex.wb_init();
  ex.wb_seed(seed);
  const k = ex.wb_kit_pick();
  if (want.includes(k) && !seeds.has(k)) seeds.set(k, seed);
}
let failed = seeds.size !== want.length;
if (failed) console.log(`  FAIL no seed under 2000 lands on kits ${want.filter((k) => !seeds.has(k)).join(", ")}`);
const got = new Map(execFileSync(native, [String(SECS), ...seeds.values()], { encoding: "utf8" }).trim().split("\n")
  .map((l) => l.split(" ").map(Number)).map(([s, k, h]) => [s, { k, h }]));
for (const [kit, seed] of seeds) {
  ex.wb_init();
  ex.wb_seed(seed);
  const k = ex.wb_kit_pick();
  if (k >= 0) ex.wb_kit_set(k);
  ex.wb_apply();
  ex.wb_play(1);
  let h = 2166136261;
  const u = new Uint32Array(1), f = new Float32Array(u.buffer);
  for (let n = 0; n < SECS * 44100; n += Q) {
    ex.wb_render(Q);
    const l = new Float32Array(ex.memory.buffer, ex.wb_out_l(), Q), r = new Float32Array(ex.memory.buffer, ex.wb_out_r(), Q);
    for (let i = 0; i < Q; i++) {
      f[0] = l[i];
      h = Math.imul(h ^ u[0], 16777619) >>> 0;
      f[0] = r[i];
      h = Math.imul(h ^ u[0], 16777619) >>> 0;
    }
  }
  const nat = got.get(seed), ok = nat && nat.k === k && nat.h === h;
  const name = opts.kits.find((x) => x.id === kit).name;
  console.log(`  ${ok ? "ok  " : "FAIL"} seed ${seed} (${name}): wasm ${h.toString(16)}, native ${nat ? nat.h.toString(16) : "none"}`);
  failed ||= !ok;
}
console.log(failed ? "kits-native: FAILED" : "kits-native: every approved kit renders the same in wasm and natively");
process.exit(failed ? 1 : 0);

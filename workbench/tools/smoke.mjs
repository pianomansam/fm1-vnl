// SPDX-License-Identifier: GPL-3.0-or-later
// The workbench's smoke test: wb.wasm loaded directly (as the worklet does), no browser.
//   sh workbench/build.sh && node workbench/tools/smoke.mjs
import { readFileSync } from "node:fs";

const WASM = readFileSync(new URL("../../app/build/workbench/wb.wasm", import.meta.url));
const SR = 44100, Q = 128;
let failed = 0;
const check = (ok, what) => {
  console.log(`  ${ok ? "ok  " : "FAIL"} ${what}`);
  if (!ok) failed++;
};

async function load() {
  let mem;
  const wasi = {
    fd_write: (fd, iov, n, out) => { new DataView(mem.buffer).setUint32(out, 0, true); return 0; },
    proc_exit: () => {},
  };
  const stub = new Proxy(wasi, { get: (t, k) => (k in t ? t[k] : () => 0) });
  const env = new Proxy({}, { get: () => () => 0 });
  const { instance } = await WebAssembly.instantiate(WASM, { wasi_snapshot_preview1: stub, env });
  const ex = instance.exports;
  mem = ex.memory;
  if (ex._initialize) ex._initialize();
  ex.wb_init();
  return ex;
}

function cstr(ex, p) {
  const b = new Uint8Array(ex.memory.buffer, p);
  let s = "";
  for (let i = 0; b[i]; i++) s += String.fromCharCode(b[i]);
  return s;
}

// n frames as one interleaved-free pair { l, r }, rendered a quantum at a time; onQ(ex, at) after each
function render(ex, n, onQ) {
  const l = new Float32Array(n), r = new Float32Array(n);
  for (let at = 0; at < n; at += Q) {
    const k = Math.min(Q, n - at);
    ex.wb_render(k);
    l.set(new Float32Array(ex.memory.buffer, ex.wb_out_l(), k), at);
    r.set(new Float32Array(ex.memory.buffer, ex.wb_out_r(), k), at);
    if (onQ) onQ(ex, at + k);
  }
  return { l, r };
}

// quanta until pred() holds (at most max frames); the frames rendered
function until(ex, pred, max) {
  let n = 0;
  while (!pred() && n < max) { render(ex, Q); n += Q; }
  return n;
}

const db = ({ l, r }, from = 0, to = l.length) => {
  let s = 0;
  for (let i = from; i < to; i++) s += l[i] * l[i] + r[i] * r[i];
  return 10 * Math.log10(s / (2 * (to - from)) + 1e-30);
};

function longestZeros({ l, r }) {
  let run = 0, best = 0;
  for (let i = 0; i < l.length; i++) {
    run = l[i] === 0 && r[i] === 0 ? run + 1 : 0;
    if (run > best) best = run;
  }
  return best;
}

const barLen = (bpm) => 4 * 60 / bpm * SR;

// the frames bar b (1-based) lasts: rendered up to its start, then to the next bar's
function measureBar(ex, b) {
  until(ex, () => ex.wb_bar() === b, 60 * SR);
  return until(ex, () => ex.wb_bar() !== b, 60 * SR);
}

const C = await (async () => { const ex = await load(); return JSON.parse(cstr(ex, ex.wb_consts())); })();
const { RF, OV, V } = C;

console.log("consts");
{
  const ex = await load();
  ex.wb_seed(21);
  const t = ex.wb_roll_get(RF.TEMPO), bars = ex.wb_roll_get(RF.BARS);
  check(RF.N === Object.keys(RF).length - 1 && OV.N === Object.keys(OV).length - 1, "the RF and OV tables are whole");
  check(t >= 70 && t <= 90 && bars >= 36 && bars <= 76 && bars % 4 === 0, `RF.TEMPO ${t} and RF.BARS ${bars} in range`);
  check(ex.wb_ov_get(OV.COMP) === -1 && ex.wb_ov_get(OV.KEYS_OCT) === 0, "overrides start as rolled");
}

console.log("seed 21, 20 s");
const ref = await (async () => {
  const ex = await load();
  ex.wb_seed(21);
  ex.wb_play(1);
  return render(ex, 20 * SR);
})();
{
  const level = db(ref);
  check(level > -45, `RMS ${level.toFixed(1)} dBFS > -45`);
  const ex = await load();
  ex.wb_seed(21);
  ex.wb_play(1);
  const again = render(ex, 20 * SR);
  let same = true;
  for (let i = 0; i < ref.l.length && same; i++) same = ref.l[i] === again.l[i] && ref.r[i] === again.r[i];
  check(same, "a fresh instance renders identical samples");
}

console.log("stopped");
{
  const ex = await load();
  ex.wb_seed(21);
  check(longestZeros(render(ex, SR)) === SR, "silence before play");
  ex.wb_play(1);
  render(ex, 3 * SR);
  ex.wb_play(0);
  const out = render(ex, SR);
  check(longestZeros({ l: out.l.subarray(SR / 10), r: out.r.subarray(SR / 10) }) === SR - SR / 10, "silence 100 ms after stop");
}

console.log("tempo 85 staged in bar 3");
{
  const ex = await load();
  ex.wb_seed(21);
  ex.wb_play(1);
  const bpm = ex.wb_roll_get(RF.TEMPO) === 85 ? 75 : 85;
  until(ex, () => ex.wb_bar() === 3, 60 * SR);
  render(ex, SR / 2);
  check(ex.wb_roll_set(RF.TEMPO, bpm) === bpm, `wb_roll_set(RF.TEMPO, ${bpm}) applied`);
  ex.wb_apply();
  const len = measureBar(ex, 4);
  check(Math.abs(len - barLen(bpm)) <= Q, `bar 4 lasts ${len} frames (${bpm} BPM: ${barLen(bpm).toFixed(0)})`);
  const out = render(ex, 8 * SR);
  check(ex.wb_bar() > 5 && ex.wb_catchups() === 1, `the song keeps going (bar ${ex.wb_bar()}, one catch-up)`);
  check(db(out) > -45, "and sounds");
}

console.log("Review Focus 1: a change staged in the last bar");
{
  const ex = await load();
  ex.wb_seed(21);
  ex.wb_play(1);
  ex.wb_roll_set(RF.BARS, 36);    // the shortest song, to reach its end sooner (play lands it)
  const bars = ex.wb_bars();
  until(ex, () => ex.wb_bar() === bars, 300 * SR);
  check(ex.wb_bar() === bars, `reached the last bar (${bars})`);
  render(ex, SR / 2);
  const bpm = ex.wb_roll_get(RF.TEMPO) === 72 ? 74 : 72;
  ex.wb_roll_set(RF.TEMPO, bpm);
  ex.wb_apply();
  const waited = until(ex, () => ex.wb_bar() === 1, 2 * barLen(70));
  check(ex.wb_bar() === 1 && waited <= barLen(70), `restarted at bar 1 (after ${waited} frames)`);
  const out = render(ex, 2 * SR);
  check(db(out) > -60 && longestZeros(out) <= Q, `not silent in the first 2 s (${db(out).toFixed(1)} dBFS)`);
  const len = measureBar(ex, 2);
  check(Math.abs(len - barLen(bpm)) <= Q, `with the change: bar 2 lasts ${len} frames (${bpm} BPM)`);
  check(ex.wb_catchups() === 1, `one landing for the change (${ex.wb_catchups()})`);
}

console.log("Review Focus 2: 50 stagings within one bar");
{
  const ex = await load();
  ex.wb_seed(21);
  ex.wb_play(1);
  until(ex, () => ex.wb_bar() === 3, 60 * SR);
  let last = -1;
  for (let i = 0; i < 50; i++) {
    const f = [RF.KEY, RF.MOTION, RF.SWING][i % 3];
    const v = ex.wb_roll_dice(f);
    if (f === RF.KEY && v >= 0) last = v;
    ex.wb_ov_set(OV.VOICING_SALT, i + 1);
    ex.wb_apply();
    render(ex, Q);
  }
  check(ex.wb_bar() === 3 && ex.wb_catchups() === 0, "all 50 inside bar 3, none landed yet");
  until(ex, () => ex.wb_bar() === 4, 60 * SR);
  const out = render(ex, 2 * SR);
  check(ex.wb_catchups() === 1, `one catch-up (${ex.wb_catchups()})`);
  check(ex.wb_roll_get(RF.KEY) === last && ex.wb_ov_get(OV.VOICING_SALT) === 50, "the latest state is the one staged");
  check(longestZeros(out) <= Q, `no gap after the bar line (longest zero run ${longestZeros(out)} frames)`);
  check(db(out) > -45, "and it sounds");
}

console.log("a new map, and the song's end");
{
  const ex = await load();
  ex.wb_seed(21);
  ex.wb_play(1);
  until(ex, () => ex.wb_bar() === 5, 60 * SR);
  check(ex.wb_roll_dice(RF.MAP) >= 0, "the map re-rolled");
  ex.wb_apply();
  until(ex, () => ex.wb_bar() !== 5, 60 * SR);
  check(ex.wb_bar() === 1, `a new map restarts at bar 1 (bar ${ex.wb_bar()})`);
  const bars = ex.wb_bars();
  until(ex, () => ex.wb_bar() === bars, 400 * SR);
  const tail = until(ex, () => ex.wb_bar() === 1, 30 * SR);
  check(ex.wb_bar() === 1 && tail > 2 * SR, `at its end the song loops after its last bar and a 2 s gap`);
}

console.log("custom set");
{
  const ex = await load();
  ex.wb_seed(21);
  const before = [V.CHORD, V.BASS, V.LEAD, V.CANDY, V.PAD].map((r) => ex.wb_role_get(r));
  const other = before[1] === before[0] ? before[2] : before[0];
  ex.wb_role_set(V.BASS, other);
  const after = [V.CHORD, V.BASS, V.LEAD, V.CANDY, V.PAD].map((r) => ex.wb_role_get(r));
  check(ex.wb_roll_get(RF.SET) === C.LIB_SET_WB, "the song switches to the custom set");
  check(after[1] === other && [0, 2, 3, 4].every((i) => after[i] === before[i]), "only the bass changed");
  ex.wb_play(1);
  ex.wb_apply();
  check(db(render(ex, 10 * SR)) > -45, "and it plays");
}

console.log(failed ? `smoke: ${failed} FAILED` : "smoke: all passed");
process.exit(failed ? 1 : 0);

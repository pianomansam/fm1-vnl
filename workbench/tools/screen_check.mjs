// SPDX-License-Identifier: GPL-3.0-or-later
// The device mock against the device: today's main screen drawn by the host simulator (app.c's draw() on fm1-base's
// gfx.c, the ground truth) and by the workbench's mock (ui/device.js, ui/screens.js) in the same states, compared
// pixel by pixel (RGB565). Both need their builds:
//   ./build.sh --host && sh workbench/build.sh && node workbench/tools/screen_check.mjs [OUTDIR]
// OUTDIR: each state as a PNG, the device's beside the mock's (and their differences in red).
// The states: before the first song (NEXT SONG), a song playing, a knob just turned (the message and the change
// waiting), the change waiting, a stop's message, stopped. The simulator's state (the bar, the key ...) is read off its
// own `expect` lines (a deliberately impossible value prints what it got); the song's facts the mock needs (the
// section, the map, the chord) come from the workbench's wb.wasm for the same seed and bar.
import { execFileSync } from "node:child_process";
import { mkdtempSync, readFileSync, writeFileSync, existsSync, mkdirSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { deflateSync } from "node:zlib";
import { Lcd, Device, W, H } from "../ui/device.js";
import data from "../ui/screens.js";

const root = new URL("../../", import.meta.url).pathname;
const HOST = join(root, "app/build/host/vnl_host"), WASM = join(root, "app/build/workbench/wb.wasm");
for (const [f, how] of [[HOST, "./build.sh --host"], [WASM, "sh workbench/build.sh"]])
  if (!existsSync(f)) { console.log(`screen_check: no ${f} (${how} first)`); process.exit(1); }
const outDir = process.argv[2];
if (outDir) mkdirSync(outDir, { recursive: true });

// wb.wasm, as smoke.mjs loads it
async function load() {
  let mem;
  const wasi = { fd_write: (fd, iov, n, out) => { new DataView(mem.buffer).setUint32(out, 0, true); return 0; } };
  const stub = new Proxy(wasi, { get: (t, k) => (k in t ? t[k] : () => 0) });
  const env = new Proxy({}, { get: () => () => 0 });
  const { instance } = await WebAssembly.instantiate(readFileSync(WASM), { wasi_snapshot_preview1: stub, env });
  const ex = instance.exports;
  mem = ex.memory;
  ex._initialize?.();
  ex.wb_init();
  return ex;
}
const ex = await load();
const json = (p) => { const b = new Uint8Array(ex.memory.buffer, p); let s = ""; for (let i = 0; b[i]; i++) s += String.fromCharCode(b[i]); return JSON.parse(s); };
const C = json(ex.wb_consts());

// the simulator: a script, its shots (fbdump: RGB565, high byte first) and the values its expect lines printed
function host(seed, lines) {
  const dir = mkdtempSync(join(tmpdir(), "vnl-screen-"));
  writeFileSync(join(dir, "s.vnl"), lines.join("\n") + "\n");
  let out = "";
  try { out = execFileSync(HOST, [join(dir, "s.vnl"), dir], { env: { ...process.env, VNL_SEED: String(seed) }, encoding: "utf8" }); }
  catch (e) { out = e.stdout; }   // (the expect lines fail on purpose)
  const got = [...out.matchAll(/expect (\w+) == -99, got (-?\d+)/g)].map((m) => [m[1], +m[2]]);
  const shot = (name) => {
    const b = readFileSync(join(dir, name)), px = new Uint16Array(W * H);
    for (let i = 0; i < W * H; i++) px[i] = (b[2 * i] << 8) | b[2 * i + 1];
    return px;
  };
  return { got, shot };
}

// the mock in a state: the song (seed) at bar (from 0), as the workbench's engine has it
function live(seed, bar, over = {}) {
  ex.wb_seed(seed);
  ex.wb_apply();
  const roll = json(ex.wb_state()).roll, r = (k) => roll[C.RF[k]];
  return { playing: true, gap: 0, bar: bar + 1, bars: r("BARS"), seed,
           roll: { key: r("KEY"), tempo: r("TEMPO"), motion: r("MOTION"), mood: r("MOOD"), mode: r("MODE") },
           facts: json(ex.wb_screen(bar)), ...over };
}

let t = 0;
const engine = { set: () => {}, play: () => {} };
const mock = (dev) => { const l = new Lcd(); dev.draw(l); return l.fb; };

function compare(name, dev, mk) {
  let same = 0;
  for (let i = 0; i < W * H; i++) same += dev[i] === mk[i];
  const pct = (100 * same) / (W * H);
  console.log(`  ${same === W * H ? "ok  " : "FAIL"} ${name}: ${pct.toFixed(3)}% of pixels match${same === W * H ? "" : ` (${W * H - same} differ)`}`);
  if (outDir) png(join(outDir, `${name}.png`), [dev, mk, diff(dev, mk)]);
  return same === W * H;
}
const diff = (a, b) => a.map((v, i) => (v === b[i] ? (v >> 2) & 0x39e7 : 0xf800));   // the same dimmed; red where not

// PNGs (several screens side by side, 8 px apart), no dependency: zlib and a CRC
function png(file, screens) {
  const gap = 8, w = screens.length * (W + gap) - gap, raw = Buffer.alloc((w * 3 + 1) * H);
  for (let y = 0; y < H; y++)
    screens.forEach((s, k) => {
      for (let x = 0; x < W; x++) {
        const v = s[y * W + x], o = y * (w * 3 + 1) + 1 + (k * (W + gap) + x) * 3, r = v >> 11, g = (v >> 5) & 63, b = v & 31;
        raw[o] = (r << 3) | (r >> 2); raw[o + 1] = (g << 2) | (g >> 4); raw[o + 2] = (b << 3) | (b >> 2);
      }
    });
  const crc = (buf) => { let c = ~0; for (const x of buf) { c ^= x; for (let k = 0; k < 8; k++) c = (c >>> 1) ^ (0xedb88320 & -(c & 1)); } return ~c >>> 0; };
  const chunk = (type, body) => {
    const b = Buffer.concat([Buffer.from(type), body]), len = Buffer.alloc(4), c = Buffer.alloc(4);
    len.writeUInt32BE(body.length); c.writeUInt32BE(crc(b));
    return Buffer.concat([len, b, c]);
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(H, 4); ihdr[8] = 8; ihdr[9] = 2;
  writeFileSync(file, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", ihdr),
                                     chunk("IDAT", deflateSync(raw)), chunk("IEND", Buffer.alloc(0))]));
}

console.log("screen_check: the mock against the host simulator (app.c's draw())");
let ok = true;
const SEEDS = [12345, 4242];
// a knob each seed turns, for the change waiting (K2 tempo up; K1 key down: the key's spelling in the song's mode)
const TURNS = { 12345: ["K2", 1], 4242: ["K1", -2] };
for (const seed of SEEDS) {
  const [knob, n] = TURNS[seed];
  const h = host(seed, [
    "wait 5", "fbdump next.raw",
    "wait 6000", "expect bar -99", "fbdump play.raw",
    `turn ${knob} ${n}`, "expect bar -99", "expect pending -99", "fbdump msg.raw",
    "wait 1400", "expect bar -99", "expect pending -99", "fbdump pending.raw",
    "tap PLAY", "wait 50", "fbdump stopmsg.raw", "wait 1400", "fbdump stopped.raw",
  ]);
  const seq = h.got.map(([k, v]) => `${k}=${v}`);
  if (h.got.length !== 5) { console.log(`  FAIL ${seed}: the simulator's state unread (${seq})`); ok = false; continue; }
  const [b0, b1, p1, b2, p2] = h.got.map(([, v]) => v);
  t = 0;
  const now = () => t, dev = new Device(data, engine, now);
  dev.update(live(seed, 0, { bar: 0 }));   // the first frame: no song yet
  ok = compare(`${seed}-next-song`, h.shot("next.raw"), mock(dev)) && ok;
  dev.update(live(seed, b0));
  ok = compare(`${seed}-playing`, h.shot("play.raw"), mock(dev)) && ok;
  dev.update(live(seed, b1));
  dev.press(knob, n);
  const pend = dev.shown().pend;
  if ((pend?.bars ?? -1) !== p1) { console.log(`  FAIL ${seed}: the change waits ${pend?.bars} bars on the mock, ${p1} on the device (${seq})`); ok = false; }
  ok = compare(`${seed}-knob-message`, h.shot("msg.raw"), mock(dev)) && ok;
  t += 1400;
  dev.update(live(seed, b2));
  if ((dev.shown().pend?.bars ?? -1) !== p2) { console.log(`  FAIL ${seed}: pending ${dev.shown().pend?.bars} vs ${p2}`); ok = false; }
  ok = compare(`${seed}-pending`, h.shot("pending.raw"), mock(dev)) && ok;
  dev.press("PLAY");
  dev.update({ ...live(seed, b2), playing: false });
  ok = compare(`${seed}-stop-message`, h.shot("stopmsg.raw"), mock(dev)) && ok;
  t += 1400;
  ok = compare(`${seed}-stopped`, h.shot("stopped.raw"), mock(dev)) && ok;
}
console.log(ok ? "screen_check: all match" : "screen_check: FAILED");
process.exit(ok ? 0 : 1);

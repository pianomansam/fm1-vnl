// SPDX-License-Identifier: GPL-3.0-or-later
// The device's screens against the mock's, pixel for pixel: the host simulator draws a screen with the UI runtime
// (app/ui: the generated tables, the value bag, render.c) and prints the bag it drew from (uistate); the mock
// (ui/device.js's drawScreen) draws the same screen from that bag. Every pixel must match. Steps of a script:
//   { press: CONTROL } | { turn: [CONTROL, DETENTS] } | { key: [N, MS] } | { wait: MS } | { screen: ID } |
//   { row: [ROW, VALUE] } (a row's value forced in the snapshot shown: the longest names) | { check: NAME }
// Phase 1: the main and part screens (lanes and faders: later phases, skipped).
//   ./build.sh --host && node workbench/tools/parity.mjs [OUTDIR]
import { execFileSync } from "node:child_process";
import { mkdtempSync, readFileSync, writeFileSync, existsSync, mkdirSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { deflateSync } from "node:zlib";
import { Lcd, W, H, drawScreen } from "../ui/device.js";
import data from "../ui/screens.js";
import { ROWS } from "../rows.js";

const root = new URL("../../", import.meta.url).pathname;
const HOST = join(root, "app/build/host/vnl_host");
if (!existsSync(HOST)) { console.log(`parity: no ${HOST} (./build.sh --host first)`); process.exit(1); }
const outDir = process.argv[2];
if (outDir) mkdirSync(outDir, { recursive: true });
const opts = JSON.parse(readFileSync(join(root, "workbench/options.json"), "utf8"));

// phase 1's screens: those without lanes or faders
const PHASE1 = Object.keys(data.screens).filter((id) => !data.screens[id].bands.some((b) => b.draw.some((it) => it.lanes || it.faders)));

const CONTROL = { press: "tap", turn: "turn", wait: "wait" };
// a script on the simulator: its checks' frames and bags, in order
function run(seed, steps) {
  const dir = mkdtempSync(join(tmpdir(), "vnl-parity-"));
  const lines = [], checks = [];
  for (const s of steps) {
    if (s.press) lines.push(`tap ${s.press}`);
    else if (s.turn) lines.push(`turn ${s.turn[0]} ${s.turn[1]}`);
    else if (s.wait) lines.push(`wait ${s.wait}`);
    else if (s.screen) lines.push(`uiscreen ${s.screen}`, "wait 20");
    else if (s.row) lines.push(`uirow ${s.row[0]} ${s.row[1]}`, "wait 20");
    else if (s.check) { lines.push("uistate", `fbdump ${checks.length}.raw`); checks.push(s.check); }
  }
  writeFileSync(join(dir, "s.vnl"), lines.join("\n") + "\n");
  let out = "";
  try { out = execFileSync(HOST, [join(dir, "s.vnl"), dir], { env: { ...process.env, VNL_SEED: String(seed) }, encoding: "utf8", maxBuffer: 1 << 26 }); }
  catch (e) { out = e.stdout ?? ""; console.log(e.stderr ?? ""); }
  const states = [...out.matchAll(/^uistate (.*)$/gm)].map((m) => JSON.parse(m[1]));
  return checks.map((name, i) => {
    const b = readFileSync(join(dir, `${i}.raw`)), px = new Uint16Array(W * H);
    for (let k = 0; k < W * H; k++) px[k] = (b[2 * k] << 8) | b[2 * k + 1];
    return { name, fb: px, state: states[i] };
  });
}

// the mock's screen from the device's bag
function mock(state) {
  const lcd = new Lcd();
  const screen = data.screens[state.screen];
  drawScreen(lcd, screen, state.values, data);
  return lcd.fb;
}

const diff = (a, b) => a.map((v, i) => (v === b[i] ? (v >> 2) & 0x39e7 : 0xf800));
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

let ok = true;
function compare(c, extra = "") {
  if (!c.state) { console.log(`  FAIL ${c.name}: no uistate`); ok = false; return; }
  const mk = mock(c.state);
  let same = 0;
  for (let i = 0; i < W * H; i++) same += c.fb[i] === mk[i];
  // nothing drawn outside the screen's bands (the device clips at a band, as the mock does)
  const screen = data.screens[c.state.screen], inBand = new Uint8Array(H);
  for (const b of screen.bands) for (let y = b.y; y < b.y + b.h; y++) inBand[y] = 1;
  let outside = 0;
  for (let y = 0; y < H; y++) if (!inBand[y]) for (let x = 0; x < W; x++) outside += c.fb[y * W + x] !== 0;
  const good = same === W * H && !outside;
  console.log(`  ${good ? "ok  " : "FAIL"} ${c.name}: ${((100 * same) / (W * H)).toFixed(3)}% of pixels match` +
              `${same === W * H ? "" : ` (${W * H - same} differ)`}${outside ? `, ${outside} drawn outside the bands` : ""}${extra}`);
  if (outDir) png(join(outDir, `${c.name}.png`), [c.fb, mk, diff(c.fb, mk)]);
  ok = ok && good;
}

console.log(`parity: the device's screens (the simulator, app/ui) against the mock's: ${PHASE1.join(", ")}`);
const SEED = 72;
{   // every phase-1 screen at bar 13 or so, playing; then stopped (held); then between songs; and at boot (ready)
  const steps = [];
  for (const id of PHASE1) steps.push({ screen: id }, { check: `boot-${id}` });
  steps.push({ press: "PLAY" }, { wait: 30000 });
  for (const id of PHASE1) steps.push({ screen: id }, { check: `playing-${id}` });
  steps.push({ press: "PLAY" }, { wait: 1500 });
  for (const id of PHASE1) steps.push({ screen: id }, { check: `stopped-${id}` });
  steps.push({ press: "PLAY" }, { press: "SEQ" }, { wait: 1500 });
  for (const id of ["main", "pComp"]) steps.push({ screen: id }, { check: `between-${id}` });
  for (const c of run(SEED, steps)) compare(c);
}
{   // Review Focus 1: each options row at its longest choice name, on its part screen: clipped at the band, as the mock
  const steps = [{ press: "PLAY" }, { wait: 4000 }];
  const fields = Object.entries(data.fields).filter(([, d]) => d.options && !["loop", "endless"].includes(d.row));
  const screenOf = (k) => PHASE1.find((id) => (data.screens[id].knobs ?? []).some((kn) => kn.field === k));
  for (const [k, d] of fields) {
    const id = screenOf(k);
    if (!id) continue;
    const row = d.row === "rhythm" ? "comp" : d.row;
    const src = ROWS.find((r) => r.id === row)?.source;
    const role = ROWS.find((r) => r.id === row)?.role, ROLE = { CHORD: "keys", LEAD: "lead", BASS: "bass", PAD: "pad" };
    const names = { instruments: opts.instruments.filter((i) => i.roles.includes(ROLE[role])), kits: opts.kits.filter((x) => x.id < 16),
                    patterns: opts.patterns, leadStyles: opts.leadStyles,
                    comping: [...opts.compRhythms.map((c) => ({ ...c, id: c.value })), ...opts.comping] }[src];
    if (!names) continue;
    const best = names.reduce((a, x) => ((x.short ?? x.name).length > (a.short ?? a.name).length ? x : a));
    steps.push({ screen: id }, { row: [row, best.id] }, { check: `longest-${k}` });
  }
  for (const c of run(SEED, steps)) compare(c);
}
console.log(ok ? "parity: all match" : "parity: FAILED");
process.exit(ok ? 0 : 1);

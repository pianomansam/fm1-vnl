// SPDX-License-Identifier: GPL-3.0-or-later
// The workbench's smoke test: wb.wasm loaded directly (as the worklet does), no browser.
//   sh workbench/build.sh && node workbench/tools/smoke.mjs
//   (WB_WASM, WB_OPTIONS, WB_GEN: another build, its options.json and patterns_gen.c: candidates_smoke.sh's, with a
//   candidate in every family)
import { readFileSync } from "node:fs";

const env = (k, rel) => (process.env[k] ? process.env[k] : new URL(rel, import.meta.url));
const WASM = readFileSync(env("WB_WASM", "../../app/build/workbench/wb.wasm"));
// the generated tables' statuses (patterns_gen.c's *_STATUS: 1 approved, 0 candidate, 2 retired), by array name
const GEN_STATUS = Object.fromEntries([...readFileSync(env("WB_GEN", "../../app/song/patterns_gen.c"), "utf8")
  .matchAll(/const uint8_t (\w+)_STATUS\[[^\]]*\] = \{([^}]*)\}/g)]
  .map((m) => [m[1], m[2].split(",").map((x) => ["candidate", "approved", "retired"][+x])]));
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
  ex.wb_apply();
  const t = ex.wb_roll_get(RF.TEMPO), bars = ex.wb_roll_get(RF.BARS);
  check(RF.N === Object.keys(RF).length - 1 && OV.N === Object.keys(OV).length - 1, "the RF and OV tables are whole");
  check(t >= 70 && t <= 90 && bars >= 36 && bars <= 76 && bars % 4 === 0, `RF.TEMPO ${t} and RF.BARS ${bars} in range`);
  check(ex.wb_ov_get(OV.COMP) === -1 && ex.wb_ov_get(OV.KEYS_OCT) === 0, "overrides start as rolled");
}

console.log("seed 21, 20 s");
const ref = await (async () => {
  const ex = await load();
  ex.wb_seed(21);
  ex.wb_apply();
  ex.wb_play(1);
  return render(ex, 20 * SR);
})();
{
  const level = db(ref);
  check(level > -45, `RMS ${level.toFixed(1)} dBFS > -45`);
  const ex = await load();
  ex.wb_seed(21);
  ex.wb_apply();
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
  ex.wb_apply();
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
  ex.wb_apply();
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
  ex.wb_roll_set(RF.BARS, 36);    // the shortest song, to reach its end sooner (play lands it)
  ex.wb_apply();
  ex.wb_play(1);
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
  ex.wb_apply();
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
  ex.wb_apply();
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
  ex.wb_apply();
  const before = [V.CHORD, V.BASS, V.LEAD, V.PAD].map((r) => ex.wb_role_get(r));
  const other = before[1] === before[0] ? before[2] : before[0];
  ex.wb_role_set(V.BASS, other);
  const after = [V.CHORD, V.BASS, V.LEAD, V.PAD].map((r) => ex.wb_role_get(r));
  check(ex.wb_roll_get(RF.SET) === C.LIB_SET_WB, "the song switches to the custom set");
  check(after[1] === other && [0, 2, 3].every((i) => after[i] === before[i]), "only the bass changed");
  ex.wb_play(1);
  ex.wb_apply();
  check(db(render(ex, 10 * SR)) > -45, "and it plays");
}


// ---- sections, jumps, mute and solo, the committed song's events (Task 2) ----
const sections = (ex) => JSON.parse(cstr(ex, ex.wb_sections()));
const BUSES = [V.CHORD, V.BASS, V.LEAD, V.PAD, V.KICK].reduce((m, r) => m | (1 << r), 0);
const fresh = async (seed = 21) => {
  const ex = await load();
  ex.wb_seed(seed);
  ex.wb_apply();
  return ex;
};
// the hash wb_heard_hash keeps, over records { bar, step, role, note }
const evHash = (evs) => evs.reduce((h, e) => [e.bar, e.step, e.role, e.note].reduce((h, x) => Math.imul(h ^ x, 16777619) >>> 0, h), 2166136261);
// one quantum's level in dB (-300: silent)
const qdb = (o) => db(o) < -290 ? -300 : db(o);

console.log("sections");
{
  const ex = await fresh(), secs = sections(ex), bars = ex.wb_roll_get(RF.BARS);
  const names = new Set(["intro", "groove", "lead", "break", "outro"]);
  let at = 0, ok = true;
  for (const x of secs) {
    ok = ok && x.start === at && x.bars > 0 && x.start % 4 === 0 && names.has(x.name);
    at += x.bars;
  }
  check(secs.length >= 3 && ok && at === bars,
        `the plan's blocks, back to back from bar 0, sum to the song's ${bars} bars (${secs.map((x) => `${x.name} ${x.bars}`).join(", ")})`);
  const before = cstr(ex, ex.wb_sections());
  for (let i = 0; i < 20 && ex.wb_roll_get(RF.BARS) === bars; i++) ex.wb_roll_dice(RF.BARS);
  check(ex.wb_roll_get(RF.BARS) !== bars && cstr(ex, ex.wb_sections()) === before, "a staged length leaves them (the committed song's)");
  ex.wb_apply();
  const after = sections(ex);
  check(after.reduce((n, x) => n + x.bars, 0) === ex.wb_roll_get(RF.BARS), `committed, they follow it (${ex.wb_roll_get(RF.BARS)} bars)`);
}

console.log("a jump to section 3 while playing");
{
  const ex = await fresh(), s3 = sections(ex)[2], bpm = ex.wb_roll_get(RF.TEMPO);
  ex.wb_play(1);
  until(ex, () => ex.wb_bar() === 2, 60 * SR);
  render(ex, SR / 4);
  ex.wb_jump(s3.start);
  render(ex, Q);
  check(ex.wb_bar() === 2 && ex.wb_catchups() === 0 && ex.wb_pending() === 1, "staged: bar 2 plays on");
  const waited = until(ex, () => ex.wb_bar() !== 2, 60 * SR);
  check(ex.wb_bar() === s3.start + 1 && ex.wb_catchups() === 1 && waited <= barLen(bpm),
        `at the next bar line it lands at the section's first bar (bar ${ex.wb_bar()}, wanted ${s3.start + 1}, after ${waited} frames)`);
  const out = render(ex, 2 * SR);
  check(longestZeros(out) <= Q && db(out) > -45, "with no gap, and it sounds");
}

console.log("Review Focus 1: a staged tempo change and a jump in the same bar");
for (const order of ["apply, jump", "jump, apply"]) {
  const ex = await fresh(), s3 = sections(ex)[2];
  const bpm = ex.wb_roll_get(RF.TEMPO) === 85 ? 75 : 85;
  ex.wb_play(1);
  until(ex, () => ex.wb_bar() === 3, 60 * SR);
  render(ex, SR / 4);
  ex.wb_roll_set(RF.TEMPO, bpm);
  if (order === "apply, jump") { ex.wb_apply(); ex.wb_jump(s3.start); } else { ex.wb_jump(s3.start); ex.wb_apply(); }
  render(ex, Q);
  until(ex, () => ex.wb_bar() !== 3, 60 * SR);
  check(ex.wb_bar() === s3.start + 1 && ex.wb_catchups() === 1 && ex.wb_live_get(RF.TEMPO) === bpm,
        `${order}: one landing (${ex.wb_catchups()}), at bar ${ex.wb_bar()} (wanted ${s3.start + 1}), at ${ex.wb_live_get(RF.TEMPO)} BPM`);
  const len = measureBar(ex, s3.start + 2);
  render(ex, 4 * SR);
  check(Math.abs(len - barLen(bpm)) <= Q && ex.wb_catchups() === 1 && ex.wb_bar() > s3.start + 2,
        `${order}: the next bar lasts ${len} frames (${bpm} BPM), no second catch-up, the song goes on`);
}

console.log("a jump while stopped");
{
  const ex = await fresh(), secs = sections(ex);
  ex.wb_jump(secs[2].start);
  render(ex, Q);
  check(ex.wb_catchups() === 0 && longestZeros(render(ex, SR / 2)) === SR / 2, "nothing plays");
  ex.wb_play(1);
  render(ex, Q);
  check(ex.wb_bar() === secs[2].start + 1, `play starts at section 3's first bar (bar ${ex.wb_bar()})`);
  render(ex, 2 * SR);
  ex.wb_play(0);
  render(ex, SR / 10);
  ex.wb_jump(secs[1].start);
  ex.wb_play(1);
  render(ex, Q);
  check(ex.wb_bar() === secs[1].start + 1, `stopped mid-song, a jump to section 2: play starts there (bar ${ex.wb_bar()})`);
  ex.wb_play(0);
  ex.wb_seed(22);
  ex.wb_apply();
  ex.wb_jump(sections(ex)[1].start);
  ex.wb_play(1);
  render(ex, Q);
  check(ex.wb_live_get(RF.N) === 22 && ex.wb_bar() === sections(ex)[1].start + 1, "a new seed committed, then a jump: its song, at its section 2");
}

// a lead-only render (wb_solo) of seed 2 from bar 1, the bed off, to a quantum where the lead sounds (seed 21 until
// the vibes-on-keys sets, 2026-11-05: its new song's wet tails read 0.3 dB over the mute's 20 dB)
const leadOnly = async () => {
  const ex = await load();
  ex.wb_seed(2);
  for (const f of [RF.DUST, RF.NOISE, RF.AMBIENCE]) ex.wb_roll_set(f, 0);
  ex.wb_apply();
  ex.wb_solo(V.LEAD);
  ex.wb_play(1);
  return ex;
};
const loudAt = await (async () => {
  const ex = await leadOnly();
  let n = 0;
  for (; n < 120 * SR; n += Q) if (qdb(render(ex, Q)) > -30 && n > 4 * SR) break;
  return n + Q;   // frames rendered up to the loud quantum's end
})();

console.log("mute and solo: listening only");
{
  const quiet = await leadOnly(), ref = await leadOnly();
  const a = render(quiet, loudAt), b = render(ref, loudAt);
  check(a.l.every((x, i) => x === b.l[i]), `the same up to a lead note (${(loudAt / SR).toFixed(2)} s)`);
  quiet.wb_solo(-1);
  quiet.wb_mute(BUSES);   // every bus: the lead goes out
  const q1 = render(quiet, Q), r1 = render(ref, Q), q2 = render(quiet, Q), r2 = render(ref, Q);
  check(qdb(r1) > -40 && qdb(r2) > -40 && qdb(q2) < qdb(r2) - 20,
        `muted mid-note: the next quantum ${qdb(q1).toFixed(1)} dB (ramping), then ${qdb(q2).toFixed(1)} dB against ${qdb(r2).toFixed(1)} (only the wet tails)`);
  quiet.wb_mute(0);
  quiet.wb_solo(V.LEAD);  // back two quanta later, the same note still sounding in the reference
  const q3 = render(quiet, Q), r3 = render(ref, Q);
  check(qdb(r3) > -40 && qdb(q3) < qdb(r3) - 20, `Review Focus 2: unmuted, the old note isn't heard (${qdb(q3).toFixed(1)} dB against ${qdb(r3).toFixed(1)})`);
  const q4 = render(quiet, 8 * SR), r4 = render(ref, 8 * SR);
  check(Math.abs(db(q4) - db(r4)) < 3, `the notes struck after play (${db(q4).toFixed(1)} dB, the reference ${db(r4).toFixed(1)})`);
}
{
  const solo = await fresh(), mute = await fresh();
  solo.wb_solo(V.CHORD);
  mute.wb_mute(BUSES & ~(1 << V.CHORD));
  solo.wb_play(1);
  mute.wb_play(1);
  const a = render(solo, 6 * SR), b = render(mute, 6 * SR);
  check(a.l.every((x, i) => x === b.l[i] && a.r[i] === b.r[i]) && db(a) > -60, `solo keys = mute every other bus (${db(a).toFixed(1)} dB)`);
  const hats = await fresh(), drums = await fresh();
  hats.wb_solo(V.HAT);
  drums.wb_mute(BUSES & ~(1 << V.KICK));
  hats.wb_play(1);
  drums.wb_play(1);
  const c = render(hats, 4 * SR), d = render(drums, 4 * SR);
  check(c.l.every((x, i) => x === d.l[i]) && db(c) > -60, "a drum voice's solo is the drums' bus");
}
{
  const ex = await load();   // every bus muted, the bed off: silence, across a landing and a jump too
  ex.wb_seed(21);
  for (const f of [RF.DUST, RF.NOISE, RF.AMBIENCE]) ex.wb_roll_set(f, 0);
  ex.wb_apply();
  ex.wb_mute(BUSES);
  ex.wb_play(1);
  let out = render(ex, 4 * SR), silent = longestZeros(out) === out.l.length;
  ex.wb_ov_set(OV.VOICING_SALT, 9);
  ex.wb_apply();
  ex.wb_jump(sections(ex)[1].start);
  out = render(ex, 4 * SR);
  check(silent && longestZeros(out) === out.l.length && ex.wb_catchups() === 1 && ex.wb_bar() > sections(ex)[1].start,
        "every bus muted (the bed off): silence from play, and on after a landing (mutes persist)");
  ex.wb_mute(0);
  check(db(render(ex, 4 * SR)) > -45, "unmuted, it sounds");
}

console.log("the committed song's events (wb_events, on a probe instance)");
{
  const { readEvents } = await import("../wb.js");
  const play = await load();
  play.wb_seed(21);
  play.wb_roll_set(RF.TEMPO, 90);
  play.wb_roll_set(RF.BARS, 36);   // (after the tempo: it sets the length)
  play.wb_ov_set(OV.BASS_SALT, 5);
  play.wb_apply();
  const bars = play.wb_roll_get(RF.BARS);
  play.wb_roll_set(RF.KEY, (play.wb_roll_get(RF.KEY) + 1) % 12);   // staged, not committed: not in the events
  const pex = await load();
  new Uint8Array(pex.memory.buffer, pex.wb_commit_ptr(), pex.wb_stage_size())
    .set(new Uint8Array(play.memory.buffer, play.wb_commit_ptr(), play.wb_stage_size()));
  const t0 = performance.now(), n = pex.wb_events(0), ms = performance.now() - t0, evs = readEvents(pex);
  check(n > 500 && evs.length === n && evs.every((e) => e.bar < bars && e.step < 16) && evs.at(-1).bar === bars - 1,
        `${n} events over ${bars} bars (${ms.toFixed(0)} ms)`);
  check(evs.filter((e) => e.role >= V.KICK).every((e) => e.note === e.role - V.KICK && e.role <= V.RIDE) &&
        evs.some((e) => e.role === V.KICK) && evs.some((e) => e.role === V.LEAD) && evs.some((e) => e.role === V.CHORD && e.len > 0),
        "drums as V_KICK..V_RIDE with the lane as note; the lead's and the keys' notes, with lengths");
  play.wb_mute(1 << V.LEAD);   // listening only: the events stay the same
  play.wb_play(1);
  until(play, () => play.wb_bar() === bars, 300 * SR);
  render(play, barLen(90) + SR / 2);   // its last bar, into the gap
  check(play.wb_heard_count() === n && (play.wb_heard_hash() >>> 0) === evHash(evs),
        `round trip: the song as played (lead muted) ticked ${play.wb_heard_count()} events, (bar, step, role, note) the same as wb_events'`);
  check(pex.wb_events(10) === -1 && pex.wb_events_count() === 10, "more than max: -1, max kept");
}

console.log("the piano roll (roll.js: layout on the committed song's events; draw on a stub canvas)");
{
  const { readEvents, readChords } = await import("../wb.js");
  const { layout, draw, BAR_PX } = await import("../roll.js");
  const probe = async (seed) => {   // a seed's committed song on a probe instance
    const play = await load();
    play.wb_seed(seed);
    play.wb_apply();
    const pex = await load();
    new Uint8Array(pex.memory.buffer, pex.wb_commit_ptr(), pex.wb_stage_size())
      .set(new Uint8Array(play.memory.buffer, play.wb_commit_ptr(), play.wb_stage_size()));
    pex.wb_events(0);
    return { play, pex, bars: play.wb_roll_get(RF.BARS), evs: readEvents(pex), chords: readChords(pex), secs: JSON.parse(cstr(play, play.wb_sections(0))) };
  };
  let song = null;
  for (let seed = 1; seed < 80 && !song; seed++) { const t = await probe(seed); if (t.bars >= 76) song = t; }
  const { bars, evs, chords, secs } = song;
  const t0 = performance.now(), lay = layout(evs, secs, bars, V, chords), tl = performance.now() - t0;
  const lane = (r) => lay.lanes.find((l) => l.role === r);
  const total = lay.lanes.reduce((n, l) => n + l.notes.length, 0);
  check(lay.lanes.map((l) => l.role).join() === "keys,bass,lead,pad,drums" && total === evs.length,
        `${bars} bars, ${evs.length} events: one rectangle each, in the five lanes (keys, bass, lead, pad, drums)`);
  const part = (e) => (e.role >= V.KICK ? "drums" : { [V.CHORD]: "keys", [V.BASS]: "bass", [V.LEAD]: "lead", [V.PAD]: "pad" }[e.role]);
  const at = (r) => evs.filter((e) => part(e) === r);
  check(["keys", "bass", "lead", "pad", "drums"].every((r) => at(r).every((e, i) => {
    const n = lane(r).notes[i];
    return n.x === e.bar + e.step / 16 && n.w === Math.max(e.len, 1) / 16 && n.shade === e.vel / 127 && n.y >= 0 && n.y < 1;
  })), "each rectangle sits at its event's bar and step, its length as width, velocity as shade, inside its lane");
  const rows = V.RIDE - V.KICK + 1;
  check(lane("drums").notes.every((n, i) => Math.abs(n.y - at("drums")[i].note / rows) < 1e-9) &&
        lane("drums").notes[at("drums").findIndex((e) => e.role === V.KICK)].y === 0, "drums map to their lane's row (the kick on the first)");
  check(lay.sections.join() === secs.map((s) => s.start).join() && secs.length > 1, `section starts as lines (${lay.sections.length})`);
  // the chord changes: one per change, on the bar the progression moves, the root of the song's chord there
  const pcOf = (n) => n % 12;
  const bassAt = (bar) => evs.find((e) => e.role === V.BASS && e.bar === bar && e.step === 0);
  check(chords.length > 3 && chords[0].bar === 0 && chords.every((c, i) => c.root < 12 && (!i || c.bar > chords[i - 1].bar)) &&
        lay.chords.length === chords.length && lay.chords.every((x, i) => x === chords[i].bar + chords[i].step / 16),
        `${chords.length} chord changes over ${bars} bars, each one line in layout, in order from bar 1`);
  const sameRoot = chords.filter((c) => bassAt(c.bar) && pcOf(bassAt(c.bar).note) === c.root).length;
  check(sameRoot >= chords.filter((c) => bassAt(c.bar)).length / 2 && sameRoot > 0, `the chord roots agree with the bass' downbeat notes where it plays one (${sameRoot} of ${chords.filter((c) => bassAt(c.bar)).length})`);
  const empty = layout(evs.filter((e) => e.role !== V.PAD), secs, bars, V, chords);
  check(empty.lanes.find((l) => l.role === "pad").notes.length === 0, "a song with no pad: an empty pad lane");
  // a stub 2d context: records each note-colored fill and the lane label text
  let calls = 0, fill = "", fills = {}, labels = [];
  const ctx = new Proxy({}, { get: (t, k) => (k === "fillRect" ? (x, y) => { calls++; fills[fill] = (fills[fill] || 0) + 1; }
                                              : k === "fillText" ? (txt) => { calls++; labels.push(txt); }
                                              : typeof k === "string" && /^[a-z]/.test(k) && !(k in t) ? () => { calls++; } : t[k]),
                              set: (t, k, v) => { if (k === "fillStyle") fill = v; t[k] = v; return true; } });
  const canvas = { width: 0, height: 0, getContext: () => ctx };
  globalThis.getComputedStyle = () => ({ getPropertyValue: () => "#b0562b" });
  const t1 = performance.now(), x = draw(canvas, lay, 12, { mute: new Set(["lead"]), solo: null }), td = performance.now() - t1;
  check(x === 11 * BAR_PX && canvas.width === bars * BAR_PX && calls > total && (fills["#9a9a9a"] || 0) === lane("lead").notes.length &&
        labels.includes("lead (muted)") && !labels.some((l) => /^(keys|bass|pad|drums) \(muted\)/.test(l)),
        `draw: the playhead at bar 12 is x=${x}; mute greys only its lane (${fills["#9a9a9a"]} lead notes grey, labeled)`);
  fills = {}; labels = [];
  const stopped = draw(canvas, lay, 0, { mute: new Set(), solo: "bass" });
  check(stopped === null && (fills["#9a9a9a"] || 0) === total - lane("bass").notes.length && labels.includes("bass") &&
        labels.includes("keys (muted)") && !labels.includes("bass (muted)"),
        "stopped: no playhead; solo greys every other lane (the pad's empty one aside) and not the soloed one");
  check(tl + td < 200, `layout ${tl.toFixed(1)} ms + draw ${td.toFixed(1)} ms on ${bars} bars, all parts: under 200 ms`);
}

// ---- the decision rows (rows.js, options.json) and the desk (desk.js) over the engine ----
const { ROWS, row, optionsFor, pickFor, conflicts, familyOf, fromEngine, COMP_PAT_BASE } = await import("../rows.js");
const { Desk } = await import("../desk.js");
const OPTS = JSON.parse(readFileSync(env("WB_OPTIONS", "../options.json"), "utf8"));
const HDR = readFileSync(new URL("../../app/song/library.h", import.meta.url), "utf8") +
            readFileSync(new URL("../engine/wb_drums.h", import.meta.url), "utf8");
const enumOf = (first) => {   // the enum whose first name is `first`: its names in order
  const body = HDR.slice(HDR.indexOf(`enum { ${first}`)).split("}")[0];
  return body.replace(/\/\*[\s\S]*?\*\//g, "").replace(/enum \{/, "").split(",").map((x) => x.trim()).filter(Boolean);
};
const lcg = (s) => () => ((s = (s * 1103515245 + 12345) >>> 0) / 4294967296);
const STR = new Set(["wb_consts", "wb_state", "wb_live_state", "wb_library", "wb_sections"]);
// the page's call (the worklet's): JSON exports parsed, wb_stage_bytes a copy of the staged state
const caller = (ex) => async (fn, ...a) => {
  if (fn === "wb_stage_bytes") return new Uint8Array(ex.memory.buffer, ex.wb_stage_ptr(), ex.wb_stage_size()).slice();
  if (fn === "wb_commit_bytes") return new Uint8Array(ex.memory.buffer, ex.wb_commit_ptr(), ex.wb_stage_size()).slice();
  if (fn === "wb_write") return void new Float32Array(ex.memory.buffer, a[0], a[1].length).set(a[1]);
  if (!ex[fn]) throw new Error(`no export ${fn}`);
  const v = ex[fn](...a);
  return STR.has(fn) ? JSON.parse(cstr(ex, v)) : v;
};
// wb.js's probe: a second instance, never played
const prober = async () => {
  const pex = await load();
  return async (bytes) => {
    new Uint8Array(pex.memory.buffer, pex.wb_stage_ptr(), pex.wb_stage_size()).set(bytes);
    return pex.wb_lead_oct_probe();
  };
};
const ins = (name) => OPTS.instruments.find((i) => i.name === name).id;
const famOf = (id, row) => (row === "kit" ? OPTS.kits : OPTS.instruments).find((x) => x.id === id)?.family;

console.log("options.json");
{
  const INS = enumOf("INS_RHODES"), KIT = enumOf("KIT_DUSTY"), PRE = enumOf("WB_KIT_PT808");
  check(OPTS.instruments.every((i) => INS[i.id] === i.sym) && OPTS.instruments.length === INS.length,
        "every instrument's id is its library.h enum's");
  const presets = OPTS.kits.filter((k) => k.id >= C.DRUMS.PRESET), engine = OPTS.kits.filter((k) => k.id < C.DRUMS.PRESET);
  check(engine.every((k) => KIT[k.id] === k.sym) && engine.length === KIT.length - 1 &&
        presets.every((k) => PRE[k.id - C.DRUMS.PRESET] === k.sym) && presets.length === PRE.length - 1,
        "every kit's id is its enum's (library.h's KIT_*, wb_drums.h's presets past WB_KIT_PRESET)");
  const synth = presets.filter((k) => k.set === undefined), sampled = presets.filter((k) => k.set !== undefined);
  check(synth.length === 3 && synth.every((k) => k.family === "synth" && k.status === "approved" && !k.ride),
        "the three synth presets are approved synth kits with no ride");
  check(sampled.length === 3 && sampled.every((k, i) => k.family === "acoustic" && k.status === "approved" && k.ride &&
                                                        k.set === i && k.id === C.DRUMS.PRESET + C.DRUMS.GOGODZE + i),
        "the three shipped sampled kits are approved acoustic kits with a ride, their sets in order");
  const retired = (xs, names) => names.every((n) => xs.find((x) => x.name.startsWith(n))?.status === "retired");
  check(retired(OPTS.instruments, ["Muted trumpet", "Sax", "Guitar lick", "String lead", "Cello"]) &&
        ["Felt piano", "Vibes"].every((n) => OPTS.instruments.find((i) => i.name.startsWith(n))?.roles.includes("lead") &&
                                             OPTS.instruments.find((i) => i.name.startsWith(n)).status === "approved"),
        "leads (Sam, 2026-10-29): the felt piano and the vibes offered, the trumpet, sax, guitar lick, string lead and cello retired");
  const leads = optionsFor(row("lead"), { values: {} }, OPTS).map((o) => o.name).sort();
  check(retired(OPTS.instruments, ["Kalimba", "Saw lead", "Synth pluck"]) &&
        JSON.stringify(OPTS.instruments.find((i) => i.name === "Emilyguitar").roles) === '["keys"]' &&
        JSON.stringify(leads) === JSON.stringify(["Felt piano", "Flute", "Knight piano", "Vibes (modeled vibraphone)"]),
        `the lead styles build: the lead row offers the four (${leads.join(", ")}); the kalimba, saw lead and synth pluck retired, the Emilyguitar on the keys only`);
  check(retired(OPTS.instruments, ["Upright bass"]) && retired(OPTS.comping, ["Arpeggio", "Offbeats"]) &&
        retired(OPTS.kits, ["Dusty", "Soft", "Our 808", "Warm", "Dark", "Rusty", "Gentle", "Clean"]),
        "the removed sounds are retired (upright, our synth kits, the mixed sampled kits, arpeggio, offbeats)");
  // the instruments removed with their models (2026-10-31: no patch; the Wurli and the DX electric piano, 2026-11-04)
  // can't play: no row offers one, even with "show retired"
  const GONE = ["Upright bass", "Kalimba", "Music box", "Bells", "Saw lead", "Synth pluck", "Wurli", "DX electric piano"];
  const shown = ROWS.flatMap((r) => optionsFor(r, { values: {} }, OPTS, { showRetired: true }))
    .filter((o) => GONE.includes(o.name)).map((o) => o.name);
  check(retired(OPTS.instruments, GONE) && !shown.length,
        `the removed instruments are offered nowhere, even with "show retired" (${shown.join(", ") || "none"})`);
}

// an acoustic song as the rows see it (the electric bass, of either family: the upright, the acoustic bass, went
// 2026-10-31)
const acoustic = () => ({ values: { keys: ins("Knight piano"), lead: ins("Flute"), bass: ins("Electric bass"),
  pad: ins("Strings"), kit: 3, comp: -1, family: "acoustic", pattern: 1, kick: "" }, locks: new Set() });

console.log("rows: retired options, dice, families");
{
  const st = acoustic();
  st.values.bass = ins("Electric bass");
  let leaked = [];
  for (const r of ROWS) for (const o of optionsFor(r, st, OPTS)) if (o.status === "retired" && !o.current) leaked.push(`${r.id}:${o.name}`);
  check(!leaked.length, `no retired option offered with "show retired" off (${leaked.join(", ") || "none"})`);
  const kits = optionsFor(row("kit"), st, OPTS, { showRetired: true });
  check(kits.some((o) => o.status === "retired") && optionsFor(row("comp"), st, OPTS).every((o) => o.name !== "Arpeggio"),
        "\"show retired\" adds them (marked); the arpeggio isn't offered by default");
  st.values.kit = 6;
  const cur = optionsFor(row("kit"), st, OPTS);
  check(cur.filter((o) => o.status === "retired").every((o) => o.current && o.value === 6), "a retired current value shows, marked current");

  const rnd = lcg(7);
  let bad = [];
  const crossed = new Set();
  for (const fam of ["acoustic", "synth"]) {
    const s = acoustic();
    s.values.family = fam;
    if (fam === "synth") Object.assign(s.values, { keys: ins("Chillwave pad"), lead: ins("Vibes (modeled vibraphone)"),
                                                   bass: ins("FM bass"), pad: ins("Warm pad") });
    for (const id of ["keys", "lead", "bass", "pad", "padOn", "kit", "comp", "pattern", "kick", "family", "leadOct"])
      for (let i = 0; i < 40; i++) {
        const v = pickFor(row(id), s, OPTS, rnd);
        if (v === null) continue;
        const f = id === "kit" ? famOf(v, id) : null;   // (the instruments mix freely: the family is the drums')
        if (String(v) === String(s.values[id]) || (f && f !== fam && f !== "electric" && f !== "any")) bad.push(`${fam} ${id}=${v}`);
        if (["keys", "bass", "pad"].includes(id) && !["electric", "any", fam].includes(famOf(v))) crossed.add(id);
        const o = optionsFor(row(id), s, OPTS).find((x) => String(x.value) === String(v));
        if (id !== "family" && (o?.status ?? "approved") !== "approved") bad.push(`${fam} ${id}=${v} not approved`);
      }
  }
  check(!bad.length, `pickFor never returns the current value, another family's kit or an unapproved one (${bad.slice(0, 4).join(", ") || "none"})`);
  check(["keys", "bass", "pad"].every((id) => crossed.has(id)),
        `families choose the drums only (Sam, 2026-10-29): the keys, bass and pad dice cross families (${[...crossed].join(", ")})`);

  const c = conflicts(row("family"), "synth", acoustic(), OPTS, lcg(3));
  check(c.forced.length === 1 && !c.blocked,
        `family -> synth forces the kit only, the keys, lead, bass and pad as they were (${c.forced.map((f) => f.row).join(", ")})`);
  const synthKits = OPTS.kits.filter((k) => k.family === "synth" && k.status === "approved").map((k) => k.id);
  check(c.forced.some((f) => f.row === "kit" && synthKits.includes(f.to)) && !c.stuck.length,
        `family -> synth forces the kit to an approved synth kit (${c.forced.find((f) => f.row === "kit")?.to})`);

  const locked = acoustic();
  locked.locks = new Set(["keys", "lead"]);   // the Knight and the flute, both acoustic
  check(!conflicts(row("family"), "synth", locked, OPTS, lcg(3)).blocked,
        "acoustic keys and lead locked (the Knight, the flute) don't block family -> synth (the family is the drums')");
  locked.locks = new Set(["kit"]);
  const b = conflicts(row("family"), "synth", locked, OPTS, lcg(3));
  check(b.blocked?.label === "Kit", `Review Focus 3: the kit locked (Brushes, acoustic) blocks family -> synth (${b.blocked?.label})`);

  const st2 = acoustic();
  st2.values.comp = 1;   // stabs, on the Knight
  const k = conflicts(row("keys"), ins("Rhodes"), st2, OPTS);
  check(k.forced.some((f) => f.row === "comp" && f.to === -1), "keys -> Rhodes forces stab comping back to as rolled");
  st2.locks = new Set(["comp"]);
  check(conflicts(row("keys"), ins("Rhodes"), st2, OPTS).blocked?.label === "Comping", "... and a locked stab comping blocks it");
  check(!optionsFor(row("comp"), { values: { keys: ins("Rhodes") } }, OPTS).some((o) => o.name === "Strum") &&
        optionsFor(row("comp"), { values: { keys: ins("Nylon guitar") } }, OPTS).some((o) => o.name === "Strum"),
        "strum is offered on the guitars only");
  const compNames = (g) => optionsFor(row("comp"), { values: { keys: ins(g) } }, OPTS).filter((o) => !o.rolled)
    .map((o) => o.name);
  check(["Nylon guitar", "Emilyguitar"].every((g) => JSON.stringify(compNames(g)) === '["Strum"]'),
        "the guitars are offered Strum only (Sam, 2026-11-04: the guitars only strum)");
  const st4 = acoustic();
  st4.values.keys = ins("Nylon guitar");
  st4.values.comp = 5;
  check(conflicts(row("keys"), ins("Rhodes"), st4, OPTS).forced.some((f) => f.row === "comp" && f.to === -1),
        "keys -> Rhodes forces strum comping back to as rolled");
  const kl = conflicts(row("lead"), ins("Knight piano"), acoustic(), OPTS, lcg(5));
  check(kl.forced.some((f) => f.row === "keys" && f.to !== ins("Knight piano")), "the Knight on the lead moves the keys off it");
  const st3 = acoustic();
  st3.values.keys = ins("Felt piano");
  const fl = conflicts(row("lead"), ins("Felt piano"), st3, OPTS, lcg(5));
  check(fl.forced.some((f) => f.row === "keys" && f.to !== ins("Felt piano")), "the felt piano on the lead moves the keys off it (never felt over felt)");
}

console.log("desk: rows over the engine");
{
  const ex = await load();
  const call = caller(ex);
  const lib = await call("wb_library");
  ex.wb_seed(21);
  ex.wb_apply();
  ex.wb_play(1);
  render(ex, Q);
  const desk = new Desk(call, C, OPTS, lib, { rnd: lcg(11), probe: await prober() });
  await desk.read();
  check(ROWS.every((r) => desk.values[r.id] !== undefined), "every row has a value");
  check(desk.legal.prog.includes(desk.values.prog) && desk.legal.prog.length < lib.progs.length,
        `the progression offers only the legal ones (${desk.legal.prog.length} of ${lib.progs.length})`);

  // every row with another value on offer: the change lands at the next bar line, and the song sounds
  const missed = [];
  for (const r of ROWS) {
    if (r.kind === "seed" || r.kind === "family" || r.regenerates) continue;
    const o = optionsFor(r, desk.state, OPTS, { lib }).find((x) => String(x.value) !== String(desk.values[r.id]) && !x.current);
    const was = ex.wb_catchups();
    const res = await desk.set(r.id, r.kind === "reroll" ? 12345 : o?.value);
    if (!o && r.kind !== "reroll") continue;
    until(ex, () => ex.wb_catchups() > was, 60 * SR);
    if (!res.ok || ex.wb_catchups() === was) missed.push(`${r.id} (${res.message})`);
  }
  check(!missed.length, `each row's change lands at the next bar (${missed.join(", ") || "all"})`);
  check(db(render(ex, 4 * SR)) > -45, "and the song still sounds");

  // Review Focus 3, end to end: the kit hand-set to an acoustic kit and locked; family -> synth is blocked, nothing
  // staged; unlocked, the kit follows and nothing else (families choose the drums only: Sam, 2026-10-29)
  await desk.set("family", "acoustic");
  await desk.set("bass", ins("Electric bass"));
  await desk.set("kit", 3);
  desk.lock("kit", true);
  const before = cstr(ex, ex.wb_state());
  const res = await desk.set("family", "synth");
  check(!res.ok && /Kit/.test(res.message) && cstr(ex, ex.wb_state()) === before && desk.values.family === "acoustic",
        `Review Focus 3: family -> synth with the kit locked: blocked, nothing changed ("${res.message}")`);
  desk.lock("kit", false);
  const ins0 = ["keys", "lead", "bass", "pad"].map((id) => desk.values[id]).join();
  const ok = await desk.set("family", "synth");
  check(ok.ok && famOf(desk.values.kit, "kit") === "synth" && ok.moved.includes("kit") &&
        ["keys", "lead", "bass", "pad"].map((id) => desk.values[id]).join() === ins0 &&
        !ok.moved.some((id) => ["keys", "lead", "bass", "pad"].includes(id)),
        `unlocked, the kit follows and is highlighted, the keys, lead, bass and pad unchanged (${ok.moved.join(", ")})`);

  // a locked progression: a mood that can't keep it is blocked; the others keep it
  desk.lock("prog", true);
  const prog = desk.values.prog, moods = [];
  for (let m = 0; m < 4; m++) {
    if (m === desk.values.mood) continue;
    const st = cstr(ex, ex.wb_state()), r = await desk.set("mood", m);
    moods.push(r.ok ? "ok" : "blocked");
    if (!r.ok && !(/Progression/.test(r.message) && cstr(ex, ex.wb_state()) === st)) moods.push("BAD");
    if (desk.values.prog !== prog) moods.push("MOVED");
  }
  check(moods.includes("blocked") && !moods.includes("BAD") && !moods.includes("MOVED"),
        `a mood that would move the locked progression is blocked and names it (${moods.join(", ")})`);
  desk.lock("prog", false);
}

console.log("desk: locks and hand edits survive a regenerate");
{
  const ex = await load();
  const call = caller(ex);
  const desk = new Desk(call, C, OPTS, await call("wb_library"), { rnd: lcg(23), probe: await prober() });
  ex.wb_seed(21);
  ex.wb_apply();
  ex.wb_play(1);
  render(ex, Q);
  await desk.read();
  const tempo = desk.values.tempo === 85 ? 74 : 85;
  await desk.set("tempo", tempo);
  await desk.set("keys", ins("Rhodes"));
  await desk.set("lead", ins("Felt piano"));
  desk.lock("lead", true);
  await desk.set("kick", "k5");
  await desk.set("kit", 3);
  desk.lock("kit", true);
  await desk.set("comp", 4);
  await desk.set("phrase", 777);
  desk.lock("phrase", true);
  const keep = { tempo, keys: ins("Rhodes"), lead: ins("Felt piano"), kick: "k5", kit: 3, comp: 4, phrase: 777 };
  const kept = () => Object.entries(keep).filter(([k, v]) => String(desk.values[k]) !== String(v)).map(([k]) => k);
  const seed = desk.values.seed;
  const r1 = await desk.dice("seed");
  check(r1.ok && desk.values.seed !== seed && !kept().length, `a new seed (${desk.values.seed}) keeps them (lost: ${kept().join(", ") || "none"})`);
  const st = await call("wb_state");
  check(st.roll[C.RF.TEMPO] === tempo && st.roles[C.V.LEAD] === ins("Felt piano") && st.kit === 3 && st.kick[1] === 0x481 &&
        st.ov[C.OV.COMP] === 4 && st.ov[C.OV.PHRASE_SALT] === 777, "and the engine has them staged");
  const map = desk.values.map, other = optionsFor(row("map"), desk.state, OPTS, { lib: await call("wb_library") })
    .find((o) => o.value !== map).value;
  const r2 = await desk.set("map", other);
  check(r2.ok && desk.values.map === other && !kept().length, `a new map keeps them too (lost: ${kept().join(", ") || "none"})`);
  until(ex, () => ex.wb_bar() === 2, 60 * SR);
  check(ex.wb_roll_get(C.RF.MAP) === other && db(render(ex, 3 * SR)) > -45, "the new song plays from bar 1 with them");
  const r3 = await desk.rerollAll();
  check(r3.ok && desk.values.lead === ins("Felt piano") && desk.values.kit === 3 && desk.values.phrase === 777,
        "re-roll all unlocked keeps the locked rows");
}

console.log("desk: dice and registers");
{
  const ex = await load();
  const call = caller(ex);
  const desk = new Desk(call, C, OPTS, await call("wb_library"), { rnd: lcg(5), probe: await prober() });
  ex.wb_seed(1);   // (seed 5 until the vibes-on-keys sets, 2026-11-05, moved its song to one whose lead goes down)
  ex.wb_apply();
  ex.wb_play(1);
  render(ex, Q);
  await desk.read();
  const salts = [];
  for (let i = 0; i < 3; i++) { await desk.dice("voicings"); salts.push(desk.values.voicings); }
  check(salts.every((s) => s > 0) && new Set(salts).size === 3, `the voicings' dice set new nonzero salts (${salts.join(", ")})`);
  const t = desk.values.tempo, r = await desk.dice("tempo");
  check(r.ok && desk.values.tempo !== t && ex.wb_roll_get(C.RF.TEMPO) === desk.values.tempo, `the tempo's dice is the engine's (${t} -> ${desk.values.tempo})`);
  const k = await desk.set("keysOct", 1);   // (where harmony_keys_oct keeps them, the engine's place shows)
  check(k.ok && desk.values.keysOct === ex.wb_keys_oct_applied() && ex.wb_ov_get(C.OV.KEYS_OCT) === desk.values.keysOct,
        `the keys register shows what the engine applies (asked +1, plays ${desk.values.keysOct}${k.message ? `: "${k.message}"` : ""})`);
  const probe = await prober(), stagedLead = async () => probe(await call("wb_stage_bytes"));
  const l = await desk.set("leadOct", -1);   // seed 1: the lead can't go down (lead.c keeps it)
  check(l.ok && desk.values.leadOct === 0 && ex.wb_ov_get(C.OV.LEAD_OCT) === 0 && /Lead register/.test(l.message),
        `the lead register shows what the engine applies (asked -1, plays ${desk.values.leadOct}: "${l.message}")`);
  ex.wb_ov_set(C.OV.LEAD_OCT, -1);
  const refused = await stagedLead();
  let up = 0;                        // (a seed whose lead an octave up moves: the rolled style's register allows it)
  ex.wb_ov_set(C.OV.LEAD_OCT, 1);
  for (let seed = 21; seed <= 60 && !up; seed++) {
    ex.wb_seed(seed);
    if ((await stagedLead()) === 1) up = seed;
  }
  check(refused === 0 && up > 0, `the probe instance: seed 1 down refused (${refused}), seed ${up} up applied`);
}

console.log("desk: a quantum rendered before every call (the worklet's interleaving)");
{
  const ex = await load();
  const call = caller(ex);
  let watch = null, bad = 0, quanta = 0;
  const heard = new Set();             // every seed that played a quantum
  const step = lcg(99);
  const callR = async (fn, ...a) => {   // 1-16 quanta before each call: a bar line can fall anywhere inside a change
    const k = 1 + Math.floor(step() * 16);
    render(ex, k * Q);
    quanta += k;
    if (watch) {
      heard.add(ex.wb_live_get(C.RF.N));
      if (ex.wb_live_get(C.RF.TEMPO) !== watch.tempo) bad++;
    }
    return call(fn, ...a);
  };
  ex.wb_seed(21);
  ex.wb_apply();
  ex.wb_play(1);
  render(ex, Q);
  const desk = new Desk(callR, C, OPTS, await call("wb_library"), { rnd: lcg(31), probe: await prober() });
  await desk.read();
  const tempo = desk.values.tempo === 88 ? 71 : 88;
  await desk.set("tempo", tempo);
  desk.lock("tempo", true);
  await desk.set("lead", ins("Flute"));
  desk.lock("lead", true);
  until(ex, () => ex.wb_live_get(C.RF.TEMPO) === tempo, 60 * SR);
  desk.lock("prog", true);
  watch = { tempo };
  const blocked = [], landed = [];
  for (let s = 100; s < 160 && (blocked.length < 3 || landed.length < 3); s++)   // back to back: an earlier seed's
    ((await desk.set("seed", s)).ok ? landed : blocked).push(s);                  // landing falls inside the next one
  until(ex, () => ex.wb_live_get(C.RF.N) === landed.at(-1), 60 * SR);
  const leaked = blocked.filter((s) => heard.has(s));
  check(blocked.length >= 3 && landed.length >= 3 && !leaked.length && !bad && ex.wb_live_get(C.RF.TEMPO) === tempo,
        `blocked seeds never play, every landed state has the locks (${blocked.length} blocked, ${landed.length} landed, ` +
        `${quanta} quanta, ${heard.size} seeds heard; ${leaked.length} blocked heard, ${bad} quanta without the locked tempo)`);
  desk.lock("prog", false);
  bad = 0;
  const r = await desk.rerollAll();
  until(ex, () => ex.wb_live_get(C.RF.N) === desk.values.seed, 60 * SR);
  render(ex, 2 * Math.ceil(barLen(70)));
  watch = null;
  check(r.ok && !bad && ex.wb_live_get(C.RF.TEMPO) === tempo, `re-roll all with bar lines inside it: every landed state keeps the locked tempo (${bad} bad)`);
}

console.log("feedback: snapshots, replay, the queue");
{
  const { replay, wav } = await import("./replay.mjs");
  const { sendFeedback, flushFeedback, queued } = await import("../wb.js");
  const ex = await load();
  const call = caller(ex);
  ex.wb_seed(21);
  ex.wb_apply();
  let at = { seconds: 0, bar: 0 };
  const desk = new Desk(call, C, OPTS, await call("wb_library"), { rnd: lcg(41), probe: await prober(), position: () => at });
  await desk.read(true);
  await desk.set("seed", 77);
  at = { seconds: 12.5, bar: 6 };
  await desk.set("tempo", desk.values.tempo === 75 ? 80 : 75);
  await desk.set("keys", ins("Rhodes"));
  await desk.dice("leadStyle");
  desk.lock("tempo", true);
  const snap = await desk.snapshot();
  const ch = desk.changes;
  check(snap.version === 1 && snap.seed === 77 && snap.locks.includes("tempo") && ROWS.every((r) => snap.rows[r.id] !== undefined),
        "the snapshot holds every row, the locks and the seed, with a version");
  check(ch.length >= 3 && ch.every((c) => "seconds" in c && "bar" in c && c.row && "from" in c && "to" in c) &&
        ch.some((c) => c.row === "tempo" && c.seconds === 12.5 && c.bar === 6 && c.to === snap.rows.tempo),
        `the change log has each applied change (${ch.length}: ${ch.map((c) => c.row).join(", ")})`);
  ex.wb_play(1);
  const live = render(ex, 8 * SR);   // the page's song from bar 1, the snapshot's state
  const a = await replay(JSON.parse(JSON.stringify(snap)), 8), b = await replay(snap, 8);
  const same = (x, y) => Buffer.compare(wav(x), wav(y)) === 0;
  check(!a.mismatch.length && same(a, b), `a snapshot replayed twice gives identical WAV bytes (${wav(a).length} B; mismatch: ${a.mismatch.join(",") || "none"})`);
  check(same(a, live), "... and they are the samples the page's engine plays from bar 1");
  check(db(a) > -45, `the replay is not silent (${db(a).toFixed(1)} dBFS)`);

  // Review: a click just after a change snapshots what is heard, not the change still waiting for its bar line
  {
    const ex2 = await load(), call2 = caller(ex2);
    ex2.wb_seed(21);
    ex2.wb_apply();
    ex2.wb_play(1);
    render(ex2, 3 * Q);
    const d2 = new Desk(call2, C, OPTS, await call2("wb_library"), { rnd: lcg(5), probe: await prober() });
    await d2.read(true);
    const before = await d2.snapshot(), t = d2.values.tempo, nt = t === 80 ? 75 : 80;
    await d2.set("tempo", nt);
    const mid = await d2.snapshot();
    check(before.pending === null && mid.pending?.tempo === nt && mid.rows.tempo === t && mid.rows.family === d2.values.family,
          `a snapshot before the bar line holds the heard state (tempo ${mid.rows.tempo}) and the waiting one (${mid.pending?.tempo})`);
    until(ex2, () => ex2.wb_live_get(C.RF.TEMPO) === nt, 60 * SR);
    const after = await d2.snapshot();
    check(after.pending === null && after.rows.tempo === nt, "after the bar line the snapshot is the new state, nothing pending");
    const pre = await replay(before, 4), midr = await replay(mid, 4), post = await replay(after, 4);
    check(same(pre, midr) && !same(midr, post), "the snapshot taken before the bar line replays the pre-change song");
  }

  // Review Focus 5: the server down, the entry stays queued, the page can say so, and it goes when the server returns
  const store = new Map(), storage = { getItem: (k) => store.get(k) ?? null, setItem: (k, v) => store.set(k, v) };
  let up = false, posted = [];
  const fake = async (url, o) => {
    if (!up) throw new TypeError("fetch failed");
    posted.push(JSON.parse(o.body));
    return { ok: true };
  };
  const entry = (n) => ({ time: "t", snapshot: snap, position: at, about: "song", rating: "up", note: `n${n}`, changes: ch });
  const r1 = await sendFeedback(entry(1), { fetch: fake, storage });
  const r2 = await sendFeedback(entry(2), { fetch: fake, storage });
  check(!r1.saved && !r2.saved && r2.queued === 2 && queued(storage) === 2 && JSON.parse(store.get("wb-feedback-queue"))[0].note === "n1",
        "server down: both entries stay queued in localStorage and the result says not saved");
  const still = await flushFeedback({ fetch: fake, storage });
  up = true;
  const back = await Promise.all([flushFeedback({ fetch: fake, storage }), flushFeedback({ fetch: fake, storage })]);
  check(!still.saved && back[1].saved && queued(storage) === 0 && posted.map((p) => p.note).join() === "n1,n2",
        "server back: queued entries are resent once each, in order, and the queue empties");
  const bad = await sendFeedback(entry(3), { fetch: async () => ({ ok: false }), storage });
  check(!bad.saved && queued(storage) === 1, "a failing server (5xx) keeps the entry queued too");
  const posts = [];
  const four = await sendFeedback(entry(4), { fetch: async (u, o) => { const b = JSON.parse(o.body); posts.push(b); return b.note === "n3" ? { ok: false, status: 400 } : { ok: true }; }, storage });
  check(four.saved && four.dead === 1 && queued(storage) === 0 && posts.map((p) => p.note).join() === "n3,n4",
        "an entry the server refuses (4xx) goes to the dead letters and doesn't block the one behind it");
  check(posts.every((p) => typeof p.id === "string" && p.id) && new Set(posts.map((p) => p.id)).size === 2, "every entry carries its own id");
}

// ---- the drum lanes (wb_drums.c): the synth presets, a lane's override, levels, auditions ----
console.log("drums: synth presets and lanes");
{
  const { SRC, DRUMS } = C, L = DRUMS.LANES, SEEDS = [21, 77];
  const lanesOf = (ex) => JSON.parse(cstr(ex, ex.wb_state())).drums;
  const toDb = (ms) => 10 * Math.log10(ms + 1e-30);
  // seed's song from bar 1 with stage() applied, secs rendered; each lane's mean square and hash, the drums' whole, the mix
  async function song(seed, stage, secs) {
    const ex = await load();
    ex.wb_seed(seed);
    stage(ex);
    ex.wb_apply();
    ex.wb_play(1);
    ex.wb_drum_meter_reset();
    const out = render(ex, secs * SR);
    const lane = [...Array(L)].map((_, i) => ({ ms: ex.wb_drum_meter(i), hash: ex.wb_drum_hash(i) }));
    return { ex, out, lane, all: ex.wb_drum_meter(L) };
  }
  const only = (ex, keep) => { for (let i = 0; i < L; i++) if (!keep.includes(i)) ex.wb_lane_src(i, SRC.NONE, 0, 0); };
  const NAMES = ["poketrack 808", "poketrack CR-78", "Optimist LO-FI"];

  for (let p = 0; p < DRUMS.GOGODZE; p++) {
    const r = await song(21, (ex) => ex.wb_kit_set(DRUMS.PRESET + p), 60);   // (seed 21's drums come in at bar 8)
    const d = lanesOf(r.ex);
    check(d.preset === p && JSON.parse(cstr(r.ex, r.ex.wb_state())).kit === DRUMS.PRESET + p && d.lanes.every((l) => l[0] !== SRC.NONE),
          `${NAMES[p]}: staged as the kit row's value, every lane from the kit`);
    check(r.lane[0].ms > 1e-6 && r.lane[1].ms > 1e-6 && db(r.out) > -45,
          `${NAMES[p]}: a song with non-silent kick and snare lanes (${toDb(r.lane[0].ms).toFixed(1)}, ${toDb(r.lane[1].ms).toFixed(1)} dB; mix ${db(r.out).toFixed(1)} dBFS)`);
  }

  // the CR-78's snare in the 808 kit: only the snare lane's samples change
  const cr = await load();
  cr.wb_kit_preset(DRUMS.PTCR78);
  const crSnare = lanesOf(cr).lanes[1];
  for (const seed of SEEDS) {
    const a = await song(seed, (ex) => ex.wb_kit_preset(DRUMS.PT808), 60);
    const b = await song(seed, (ex) => { ex.wb_kit_preset(DRUMS.PT808); ex.wb_lane_src(1, crSnare[0], crSnare[1], crSnare[2]); }, 60);
    const same = a.lane.map((l, i) => l.hash === b.lane[i].hash), heard = a.lane.filter((l) => l.ms > 1e-6).length;
    check(!same[1] && same.every((s, i) => s || i === 1) && b.lane[1].ms > 1e-6 && heard >= 3 && lanesOf(b.ex).preset === DRUMS.PT808,
          `seed ${seed}: 808 kit + the CR-78's snare: only the snare lane's samples change (same: ${same.map((s, i) => (s ? i : "-")).join("")}; ${heard} lanes heard), the kit row stays the 808`);
  }

  // levels: each preset's kick and snare against the Brushes kit's (its own lanes through the hook) on the same songs
  for (const seed of SEEDS) {
    const br = await song(seed, (ex) => { ex.wb_kit_set(3); ex.wb_lane_src(0, SRC.BUILTIN, 3, 0); only(ex, [0, 1]); }, 60);
    const got = [];
    for (let p = 0; p < DRUMS.GOGODZE; p++) {
      const r = await song(seed, (ex) => { ex.wb_kit_preset(p); only(ex, [0, 1]); }, 60);
      got.push(toDb(r.all) - toDb(br.all));
    }
    check(br.all > 1e-6 && got.every((g) => Math.abs(g) <= 2),
          `seed ${seed}: each preset's kick + snare within 2 dB of Brushes' (${got.map((g, p) => `${NAMES[p]} ${g >= 0 ? "+" : ""}${g.toFixed(2)}`).join(", ")} dB)`);
  }

  // an audition: heard at once, stopped or playing, and the song doesn't move
  {
    const ex = await load();
    ex.wb_seed(21);
    ex.wb_apply();
    ex.wb_kit_preset(DRUMS.OPTLOFI);   // staged only: the audition plays the staged lane
    ex.wb_audition(1, 110);
    const s = render(ex, SR / 2);
    check(db(s) > -50 && ex.wb_seconds() === 0 && ex.wb_bar() === 0,
          `stopped: a LO-FI snare audition is heard (${db(s).toFixed(1)} dBFS) and the song stays at its start`);
    const tail = render(ex, 3 * SR);
    check(longestZeros({ l: tail.l.subarray(2 * SR), r: tail.r.subarray(2 * SR) }) === SR, "... and ends in silence");
    const ex2 = await load();
    ex2.wb_seed(21);
    ex2.wb_apply();
    ex2.wb_audition(0, 110);           // no lanes: the song's own kit's kick
    check(db(render(ex2, SR / 2)) > -50, "with no lanes, the audition plays the staged kit's lane");
    const ref = await load();             // the same song, played without the second audition
    ref.wb_seed(21);
    ref.wb_apply();
    ref.wb_audition(0, 110);
    render(ref, SR / 2);
    for (const x of [ex2, ref]) { x.wb_play(1); render(x, SR); }
    const t0 = ex2.wb_seconds(), b0 = ex2.wb_bar();
    ex2.wb_kit_preset(DRUMS.PT808);
    ex2.wb_audition(0, 127);
    const withA = render(ex2, SR), without = render(ref, SR);
    const diff = { l: withA.l.map((x, i) => x - without.l[i]), r: withA.r.map((x, i) => x - without.r[i]) };
    check(Math.abs(ex2.wb_seconds() - t0 - 1) < 1e-3 && ex2.wb_bar() >= b0 && db(diff) > -50,
          `playing: an audition is heard over the song (${db(diff).toFixed(1)} dBFS against it) and doesn't advance it`);
  }

  // the lanes are staged with everything else: undo, snapshot and replay
  {
    const ex = await load(), call = caller(ex);
    ex.wb_seed(21);
    ex.wb_apply();
    const desk = new Desk(call, C, OPTS, await call("wb_library"), { rnd: lcg(3) });
    await desk.read(true);
    await desk.set("kit", DRUMS.PRESET + DRUMS.PT808);
    ex.wb_stage_mark();
    ex.wb_lane_src(1, crSnare[0], crSnare[1], crSnare[2]);
    ex.wb_lane_db(4, -6);
    const edited = lanesOf(ex);
    ex.wb_stage_undo();
    check(lanesOf(ex).lanes[1][1] !== crSnare[1] && lanesOf(ex).lanes[4][3] === 0, "wb_stage_undo puts the lanes back");
    ex.wb_lane_src(1, crSnare[0], crSnare[1], crSnare[2]);
    ex.wb_lane_db(4, -6);
    ex.wb_apply();
    const snap = await desk.snapshot();
    check(snap.version === 1 && JSON.stringify(snap.drums) === JSON.stringify(edited) && snap.rows.kit === DRUMS.PRESET + DRUMS.PT808,
          "the snapshot holds the kit row's preset and the lanes (the CR-78 snare, the shaker at -6 dB)");
    ex.wb_play(1);
    const live = render(ex, 30 * SR);
    const { replay, wav } = await import("./replay.mjs");
    const rp = await replay(JSON.parse(JSON.stringify(snap)), 30);
    check(!rp.mismatch.length && Buffer.compare(wav(rp), wav(live)) === 0, "its replay plays the page's samples");
    ex.wb_seed(5);
    check(lanesOf(ex).preset === -1 && lanesOf(ex).lanes.every((l) => l[0] === SRC.NONE), "a new seed's song drops them (they go with the custom set)");
  }
}

// ---- the shipped sampled kits (samples/kits.json, prep_kits.py's; git-ignored) and the lanes' first edit ----
console.log("drums: sampled kits, the first lane edit");
{
  const { SRC, DRUMS } = C, L = DRUMS.LANES, SEEDS = [21, 77];
  const { Samples } = await import("../wb.js");
  const { existsSync } = await import("node:fs");
  const here = (p) => new URL(`../${p}`, import.meta.url);
  // workbench/'s files as the page fetches them; missing: names that look absent
  const disk = (missing = new Set()) => ({
    exists: async (p) => !missing.has(p) && existsSync(here(p)),
    get: async (p) => {
      if (missing.has(p) || !existsSync(here(p))) return null;
      const b = readFileSync(here(p));
      return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
    },
  });
  const lanesOf = (ex) => JSON.parse(cstr(ex, ex.wb_state())).drums;
  const toDb = (ms) => 10 * Math.log10(ms + 1e-30);
  const sampledKits = OPTS.kits.filter((k) => k.set !== undefined);
  async function song(seed, stage, secs, sets = []) {
    const ex = await load(), smp = new Samples(caller(ex), C, disk());
    await smp.check(OPTS);
    for (const set of sets) await smp.load(set);
    ex.wb_seed(seed);
    stage(ex);
    ex.wb_apply();
    ex.wb_play(1);
    ex.wb_drum_meter_reset();
    const out = render(ex, secs * SR);
    const lane = [...Array(L)].map((_, i) => ({ ms: ex.wb_drum_meter(i), hash: ex.wb_drum_hash(i) }));
    return { ex, out, lane, all: ex.wb_drum_meter(L), smp };
  }
  const only = (ex, keep) => { for (let i = 0; i < L; i++) if (!keep.includes(i)) ex.wb_lane_src(i, SRC.NONE, 0, 0); };

  // a missing file (Review Focus 4): the kit is unavailable, offered marked, never picked, refused by the desk; the
  // engine stages nothing for a kit it hasn't loaded (no silent lanes)
  {
    const idx = existsSync(here("samples/kits.json")) ? JSON.parse(readFileSync(here("samples/kits.json"), "utf8")) : null;
    const gone = idx ? `samples/${idx.kits[0].pieces[1].layers[0].files[0]}` : "samples/kits.json";
    const ex = await load(), call = caller(ex), smp = new Samples(call, C, disk(new Set([gone])));
    await smp.check(OPTS);
    const gg = DRUMS.PRESET + DRUMS.GOGODZE;
    check(smp.unavailable.has(gg), `with ${gone} missing, Gogodze is unavailable`);
    ex.wb_seed(21);
    ex.wb_apply();
    const desk = new Desk(call, C, OPTS, await call("wb_library"), { rnd: lcg(5), samples: smp });
    await desk.read(true);
    desk.values.family = "acoustic";
    const o = optionsFor(row("kit"), desk.state, OPTS).find((x) => x.value === gg);
    const rnd = lcg(11), picks = [...Array(60)].map(() => pickFor(row("kit"), desk.state, OPTS, rnd));
    check(o && o.unavailable && !picks.includes(gg), "the picker shows it unavailable and the dice never pick it");
    const before = desk.values.kit, res = await desk.set("kit", gg);
    check(!res.ok && /unavailable/.test(res.message) && desk.values.kit === before, `choosing it is refused: "${res.message}"`);
    ex.wb_kit_preset(DRUMS.GOGODZE);
    check(lanesOf(ex).lanes.every((l) => l[0] === SRC.NONE) && JSON.parse(cstr(ex, ex.wb_state())).kit !== gg,
          "the engine stages nothing for a sampled kit that isn't loaded");
  }

  // a set that passes the boot's check but loses a file before its first use: unavailable then, refused, no lanes
  if (existsSync(here("samples/kits.json"))) {
    const idx = JSON.parse(readFileSync(here("samples/kits.json"), "utf8"));
    const gone = new Set(), ex = await load(), call = caller(ex), smp = new Samples(call, C, disk(gone));
    await smp.check(OPTS);
    const rusty = DRUMS.PRESET + DRUMS.RUSTY, was = smp.unavailable.has(rusty);
    gone.add(`samples/${idx.kits[1].pieces[5].layers[2].files[1]}`);   // (after the check: removed since boot)
    ex.wb_seed(21);
    ex.wb_apply();
    const desk = new Desk(call, C, OPTS, await call("wb_library"), { samples: smp });
    await desk.read(true);
    const before = desk.values.kit, res = await desk.set("kit", rusty);
    check(!was && smp.unavailable.has(rusty) && !res.ok && /unavailable/.test(res.message) && desk.values.kit === before &&
          lanesOf(ex).lanes.every((l) => l[0] === SRC.NONE) && !ex.wb_lane_src(6, SRC.SAMPLE, 1, 5),
          `a file gone after the check: the first use fails, the kit turns unavailable, the choice is refused, no lane plays it ("${res.message}")`);
    const desk2 = new Desk(call, C, OPTS, await call("wb_library"), { samples: smp });
    const { missing: miss } = await desk2.stageDrums({ lanes: [[SRC.SAMPLE, 1, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0]] });
    check(miss.length === 1 && /Big Rusty \(sticks\): samples missing/.test(miss[0]), `a snapshot's lanes on that set name the cause (${miss[0]})`);
  }

  // a set loaded while the song plays: the audio thread's work between quanta (each call the worklet runs, and the
  // quantum after it); logged (workbench/README.md), checked against the 2.9 ms quantum
  if (existsSync(here("samples/kits.json"))) {
    const ex = await load();
    ex.wb_seed(21);
    ex.wb_apply();
    ex.wb_play(1);
    render(ex, 10 * SR);
    const base = caller(ex), times = { alloc: 0, write: 0, render: 0, calls: 0 };
    const timed = async (fn, ...a) => {
      const t0 = performance.now(), v = await base(fn, ...a), t1 = performance.now();
      ex.wb_render(Q);
      const t2 = performance.now(), k = fn === "wb_sample_alloc" ? "alloc" : fn === "wb_write" ? "write" : null;
      if (k) times[k] = Math.max(times[k], t1 - t0);
      times.render = Math.max(times.render, t2 - t1);
      times.calls++;
      return v;
    };
    const smp = new Samples(timed, C, disk());
    await smp.check(OPTS);
    const t0 = performance.now(), ok = await smp.load(1), all = performance.now() - t0;
    const worst = Math.max(times.alloc, times.write) + times.render;
    console.log(`  (Big Rusty (sticks) loaded while playing: ${times.calls} calls, ${all.toFixed(0)} ms in all; worst alloc ${(times.alloc * 1000).toFixed(0)} us, ` +
                `worst write ${(times.write * 1000).toFixed(0)} us, worst quantum after one ${(times.render * 1000).toFixed(0)} us)`);
    check(ok && worst < 2.9, `a set loaded while playing: no call plus its quantum over the 2.9 ms quantum (worst ${worst.toFixed(2)} ms)`);
  }

  const missing = await new Samples(caller(await load()), C, disk()).check(OPTS);
  if (missing.size) {
    console.log("  SKIP the sampled kits' sound: their files aren't in workbench/samples (python3 -I workbench/tools/prep_kits.py FETCH_DIR workbench/samples)");
  } else {
    const idx = JSON.parse(readFileSync(here("samples/kits.json"), "utf8"));
    const br = {};   // Brushes' kick and snare on each seed (its own lanes through the hook)
    for (const seed of SEEDS)
      for (const lane of [0, 1])
        br[`${seed}/${lane}`] = (await song(seed, (ex) => { ex.wb_kit_set(3); ex.wb_lane_src(lane, SRC.BUILTIN, 3, lane); only(ex, [lane]); }, 60)).all;
    for (const k of sampledKits) {
      const e = idx.kits.find((x) => x.id === k.set), p = k.id - DRUMS.PRESET;
      const r = await song(21, (ex) => ex.wb_kit_set(k.id), 60, [k.set]);
      const d = lanesOf(r.ex), live = JSON.parse(cstr(r.ex, r.ex.wb_live_state()));
      check(!r.smp.unavailable.has(k.id) && d.preset === p && live.kit === k.id &&
            d.lanes.every((l, i) => (e.lanes[i] === null ? l[0] === SRC.NONE : l[0] === SRC.SAMPLE && l[1] === k.set && l[2] === e.lanes[i])),
            `${k.name}: staged as the kit row's value, its lanes kits.json's (${e.lanes.map((x) => x ?? "-").join(" ")})`);
      check(r.lane[0].ms > 1e-6 && r.lane[1].ms > 1e-6 && db(r.out) > -45,
            `${k.name}: a song with non-silent kick and snare lanes (${toDb(r.lane[0].ms).toFixed(1)}, ${toDb(r.lane[1].ms).toFixed(1)} dB; mix ${db(r.out).toFixed(1)} dBFS)`);
      const got = [];
      for (const seed of SEEDS)
        for (const lane of [0, 1]) {
          const s = await song(seed, (ex) => { ex.wb_kit_preset(p); only(ex, [lane]); }, 60, [k.set]);
          got.push(toDb(s.all) - toDb(br[`${seed}/${lane}`]));
        }
      check(got.every((g) => Math.abs(g) <= 2),
            `${k.name}: kick and snare each within 2 dB of Brushes' on seeds ${SEEDS.join(", ")} (${got.map((g) => `${g >= 0 ? "+" : ""}${g.toFixed(2)}`).join(", ")} dB)`);
    }
    // the ride section plays on them: a song where it does, and the ride lane heard
    {
      const rusty = DRUMS.PRESET + DRUMS.RUSTY;
      const r = await song(21, (ex) => { ex.wb_kit_set(rusty); ex.wb_ov_set(C.OV.RIDE, 1); }, 90, [1]);
      check(r.lane[6].ms > 1e-6, `Big Rusty: the ride section throughout plays its ride (${toDb(r.lane[6].ms).toFixed(1)} dB)`);
    }
    // a lane's override on a sampled kit: only that lane changes
    {
      const a = await song(21, (ex) => ex.wb_kit_preset(DRUMS.GOGODZE), 60, [0, 2]);
      const b = await song(21, (ex) => { ex.wb_kit_preset(DRUMS.GOGODZE); ex.wb_lane_src(1, SRC.SAMPLE, 2, 1); }, 60, [0, 2]);
      const same = a.lane.map((l, i) => l.hash === b.lane[i].hash);
      check(!same[1] && same.every((s, i) => s || i === 1) && b.lane[1].ms > 1e-6,
            `Gogodze + the brush snare: only the snare lane changes (same: ${same.map((s, i) => (s ? i : "-")).join("")})`);
    }
    // a snapshot with sampled lanes replays (the replay loads the sets it names)
    {
      const r = await song(21, (ex) => { ex.wb_kit_preset(DRUMS.BRUSHES); ex.wb_lane_db(0, -3); }, 1, [2]);
      const desk = new Desk(caller(r.ex), C, OPTS, JSON.parse(cstr(r.ex, r.ex.wb_library())), { samples: r.smp });
      await desk.read(true);
      const snap = await desk.snapshot();
      const { replay } = await import("./replay.mjs");
      const rp = await replay(JSON.parse(JSON.stringify(snap)), 20);
      const s2 = await song(21, (x) => { x.wb_kit_preset(DRUMS.BRUSHES); x.wb_lane_db(0, -3); }, 20, [2]);
      check(!rp.mismatch.length && rp.l.every((x, i) => x === s2.out.l[i]), "a snapshot of Big Rusty (brushes) replays sample for sample");
    }
  }

  // the first lane edit copies the song's kit onto every lane: a synth kit's as BUILTIN; a retired mixed sampled kit's
  // is refused (its hits are processed copies, not sources), with a message
  for (const [kit, name] of [[0, "Dusty"], [1, "Soft"]]) {
    const r = await song(21, (ex) => { ex.wb_kit_set(kit); ex.wb_lane_src(1, SRC.BUILTIN, 3, 1); }, 30);
    const d = lanesOf(r.ex).lanes;
    check(d.every((l, i) => (i === 1 ? l[0] === SRC.BUILTIN && l[1] === 3 : i <= 5 ? l[0] === SRC.BUILTIN && l[1] === kit && l[2] === i : l[0] === SRC.NONE)) &&
          r.all > 1e-6, `a ${name} song: the first lane edit (the Brushes snare) keeps ${name} on the kick, hats, shaker and rim; the drums play`);
  }
  {
    const ex = await load(), call = caller(ex);
    ex.wb_seed(21);
    ex.wb_kit_set(4);                    // KIT_S_WARM
    ex.wb_apply();
    const desk = new Desk(call, C, OPTS, await call("wb_library"));
    await desk.read(true);
    const res = await desk.lane(1, SRC.BUILTIN, 3, 1);
    check(!res.ok && /lane by lane/.test(res.message) && lanesOf(ex).lanes.every((l) => l[0] === SRC.NONE) &&
          JSON.parse(cstr(ex, ex.wb_state())).kit === 4, `a Warm (mixed samples) song: the first lane edit is refused: "${res.message}"`);
    ex.wb_lane_db(2, -6);
    ex.wb_lane_db(2, NaN);
    check(lanesOf(ex).lanes[2][3] === -6, "a lane's level ignores NaN");
  }
  // the no-lanes audition plays the staged kit (not the one playing): Dusty and Soft staged on the same song differ
  {
    const aud = async (kit) => {
      const ex = await load();
      ex.wb_seed(21);
      ex.wb_apply();
      ex.wb_kit_set(kit);
      ex.wb_audition(1, 110);
      return render(ex, SR / 2);
    };
    const a = await aud(0), b = await aud(1), c = await aud(0);
    check(db(a) > -50 && db(b) > -50 && a.l.some((x, i) => x !== b.l[i]) && a.l.every((x, i) => x === c.l[i]),
          "with no lanes, the audition plays the staged kit's lane (Dusty's snare and Soft's differ)");
  }
  // a BUILTIN hit on another kit leaves drums.c on the song's: a Dusty song after a Brushes kick audition (a sine: the
  // noise stream stays the song's) plays on as without it, to the compressor's last trace
  {
    const run = async (kit) => {
      const ex = await load();
      ex.wb_seed(21);
      ex.wb_kit_set(0);
      ex.wb_apply();
      ex.wb_play(1);
      render(ex, 25 * SR);
      if (kit >= 0) {
        ex.wb_lane_src(0, SRC.BUILTIN, kit, 0);   // staged only
        ex.wb_audition(0, 127);
      }
      render(ex, 5 * SR);
      return render(ex, 20 * SR);
    };
    const ref = await run(-1), a = await run(3);
    const diff = { l: a.l.map((x, i) => x - ref.l[i]), r: a.r.map((x, i) => x - ref.r[i]) };
    check(db(diff) - db(ref) < -60, `after a Brushes kick audition, a Dusty song's drums are Dusty's again (the next 20 s: ${(db(diff) - db(ref)).toFixed(1)} dB against it; stuck on Brushes: about -5)`);
  }
}

// ---- the drum screen (drums.js): the grid, kick phrases, lane sounds, saving and loading ----
console.log("drum screen: the grid");
{
  const { SRC, DRUMS } = C;
  const { sounds, soundOptions, pickSound, cellOf, cycle, patternFile, kitFile, save, listSaved, getSaved, laneSources } = await import("../drums.js");
  const grid = (ex) => JSON.parse(cstr(ex, ex.wb_state())).drums.grid;
  const KICK = 0, STEP = 6;
  // seed 77 from bar 1 on the 808 kit (the lanes' meters), played to the middle of bar 5; edit: staged there
  const run = async (edit) => {
    const ex = await load(), call = caller(ex);
    ex.wb_seed(77);
    ex.wb_kit_preset(DRUMS.PT808);
    ex.wb_apply();
    ex.wb_play(1);
    const desk = new Desk(call, C, OPTS, await call("wb_library"));
    await desk.read(true);
    until(ex, () => ex.wb_bar() === 5, 60 * SR);
    render(ex, Math.round(barLen(ex.wb_roll_get(C.RF.TEMPO)) / 2));
    if (edit) await desk.step(KICK, 1, STEP, 1);   // bar 6 is bar B (song bars 0-based: 5 is odd)
    else ex.wb_apply();                              // the same landing, nothing changed
    // the kick lane's samples (its meter and hash, before the drum bus): the rest of bar 5, then bar 6 up to the
    // edited step (a quantum late at most: half a step short of it) and through it
    const step = barLen(ex.wb_roll_get(C.RF.TEMPO)) / 16, bars = {};
    const lane = () => ({ ms: ex.wb_drum_meter(KICK), hash: ex.wb_drum_hash(KICK) });
    ex.wb_drum_meter_reset();
    until(ex, () => ex.wb_bar() === 6, 60 * SR);
    bars[5] = lane();
    ex.wb_drum_meter_reset();
    render(ex, Math.round((STEP - 0.5) * step));
    bars.before = lane();
    render(ex, Math.round(step));
    bars.at = lane();
    return { ex, desk, bars };
  };
  const ref = await run(false), ed = await run(true);
  check(ref.bars[5].ms > 1e-6, `seed 77's kick plays in bar 5 (${(10 * Math.log10(ref.bars[5].ms)).toFixed(1)} dB)`);
  check(ed.ex.wb_pat_get(KICK, 1, STEP) === 1 && ed.ex.wb_pat_get(KICK, 0, STEP) === 0 && grid(ed.ex).edited === 1,
        "the edit is in the custom pattern's bar B only (wb_pat_get)");
  check(ed.bars[5].hash === ref.bars[5].hash, "edited in bar 5: bar 5's kick lane is untouched");
  check(ed.bars.before.hash === ref.bars.before.hash, `bar 6 up to step ${STEP + 1}: the kick lane is the same`);
  check(ed.bars.at.hash !== ref.bars.at.hash && ed.bars.at.ms > ref.bars.at.ms,
        `... and at step ${STEP + 1} it has the new hit (${(10 * Math.log10(ed.bars.at.ms)).toFixed(1)} dB against ${(10 * Math.log10(ref.bars.at.ms + 1e-30)).toFixed(1)})`);

  // states: a click cycles empty -> hit -> ghost -> empty; undo and the snapshot carry the grid
  {
    const ex = await load(), call = caller(ex);
    ex.wb_seed(21);
    ex.wb_apply();
    const desk = new Desk(call, C, OPTS, await call("wb_library"));
    await desk.read(true);
    const NAME = ["empty", "hit", "ghost"], first = ex.wb_pat_get(1, 0, 3), seen = [first];
    for (let i = 0; i < 3; i++) {
      await desk.step(1, 0, 3, cycle(seen.at(-1)));
      seen.push(cellOf(desk.drums.grid.lanes[1], 0, 3));
    }
    check(seen.slice(1).join() === [1, 2, 3].map((k) => (first + k) % 3).join() && ex.wb_pat_get(1, 0, 3) === first,
          `snare bar 1 step 4 clicked three times: ${seen.map((x) => NAME[x]).join(" -> ")}`);
    await desk.step(2, 1, 5, 2);
    const g = JSON.stringify(grid(ex));
    ex.wb_stage_mark();
    ex.wb_pat_step(2, 1, 5, 0);
    ex.wb_stage_undo();
    check(JSON.stringify(grid(ex)) === g, "wb_stage_undo puts the grid back");
    const snap = await desk.snapshot();
    check(snap.version === 1 && JSON.stringify(snap.drums.grid) === g && snap.rows.pattern === desk.values.pattern,
          "the snapshot holds the grid (drums.grid; version 1)");
    const ex2 = await load(), call2 = caller(ex2);
    const d2 = new Desk(call2, C, OPTS, await call2("wb_library"));
    const miss = await d2.restore(JSON.parse(JSON.stringify(snap)));
    check(!miss.length && JSON.stringify(grid(ex2)) === g, `restored, the grid is the same (mismatch: ${miss.join(", ") || "none"})`);
    const { replay, wav } = await import("./replay.mjs");
    ex.wb_play(1);
    const live = render(ex, 20 * SR), rp = await replay(JSON.parse(JSON.stringify(snap)), 20);
    check(Buffer.compare(wav(rp), wav(live)) === 0, "its replay plays the page's samples");
    desk.lock("pattern", true);
    const res = await desk.step(0, 0, 1, 1);
    check(!res.ok && /locked/.test(res.message) && JSON.stringify(grid(ex)) === g, `a locked pattern refuses the grid: "${res.message}"`);
    desk.lock("pattern", false);
    const seed = desk.values.seed;
    await desk.dice("seed");
    check(desk.values.seed !== seed && JSON.stringify(grid(ex)) === g, "a new seed keeps the drawn grid (a hand edit)");
  }

  // kick phrases write the kick lane's bars A and B, over the grid's other edits
  {
    const ex = await load(), call = caller(ex);
    ex.wb_seed(21);
    ex.wb_apply();
    const desk = new Desk(call, C, OPTS, await call("wb_library"));
    await desk.read(true);
    await desk.step(1, 0, 2, 1);
    await desk.set("kick", "k5");
    const steps = (b) => [...Array(16).keys()].filter((s) => ex.wb_pat_get(0, b, s) === 1).join(",");
    check(steps(0) === "0,10" && steps(1) === "0,7,10" && desk.values.kick === "k5" && ex.wb_pat_get(1, 0, 2) === 1,
          `k5: kick lane A = [${steps(0)}], B = [${steps(1)}]; the snare edit stays`);
    await desk.step(0, 0, 4, 2);
    check(desk.values.kick === "grid", "a kick drawn in the grid: the kick row reads \"Drawn in the grid\"");
    await desk.set("kick", "");
    const base = OPTS.patterns.find((p) => p.id === desk.values.pattern);
    check(desk.values.kick === "" && ex.wb_pat_get(1, 0, 2) === 1, `the pattern's own kick (${base.name}) back, the snare edit kept`);
    await desk.set("pattern", desk.values.pattern === 1 ? 2 : 1);
    check(!grid(ex).edited && ex.wb_pat_get(1, 0, 2) === cellOf(grid(ex).lanes[1], 0, 2), "choosing a pattern replaces the grid");
  }

  // a poly3 shaker (pattern 5) shows its first two bars; editing it makes a two-bar loop
  {
    const ex = await load();
    ex.wb_seed(21);
    ex.wb_roll_set(C.RF.PATTERN, 5);
    const sh = (b) => [...Array(16).keys()].filter((s) => ex.wb_pat_get(4, b, s)).join(",");
    check(sh(0) === "0,3,6,9,12,15" && sh(1) === "2,5,8,11,14", `pattern 5's poly3 shaker: bar A ${sh(0)}, bar B ${sh(1)}`);
    ex.wb_pat_step(4, 0, 1, 1);
    check(sh(0) === "0,1,3,6,9,12,15" && sh(1) === "2,5,8,11,14", "edited: the same two bars plus the new step");
  }
  // another lane's edit on pattern 5 leaves the poly3 shaker as it was: its samples from bar 3 (where a two-bar loop
  // would differ) the same as without the edit
  {
    const shaker = async (edit) => {
      const ex = await load();
      ex.wb_seed(77);
      ex.wb_roll_set(C.RF.PATTERN, 5);
      ex.wb_kit_preset(DRUMS.PT808);
      if (edit) ex.wb_pat_step(2, 0, 1, 1);   // a hat
      ex.wb_apply();
      ex.wb_play(1);
      until(ex, () => ex.wb_bar() === 3, 120 * SR);
      ex.wb_drum_meter_reset();
      until(ex, () => ex.wb_bar() === 12, 120 * SR);
      return { ms: ex.wb_drum_meter(4), hash: ex.wb_drum_hash(4), hat: ex.wb_drum_hash(2), custom: ex.wb_roll_get(C.RF.PATTERN) };
    };
    const a = await shaker(false), b = await shaker(true);
    check(b.custom === 255 && a.ms > 1e-7 && a.hash === b.hash && a.hat !== b.hat,
          `pattern 5 with a hat edit: the hat lane changes, the poly3 shaker's bars 3-11 don't (${(10 * Math.log10(a.ms)).toFixed(1)} dB)`);
  }

  console.log("drum screen: lane sounds");
  const ALL = sounds(C, OPTS);
  const offered = soundOptions(ALL, "", false), retired = soundOptions(ALL, "", true);
  const REMOVED = /upright|x0x|909|boombap|vintage|dusty|soft|our 808|warm|dark|gentle|clean|mixed/i;
  check(offered.length > 40 && offered.every((s) => s.status === "approved") && !offered.some((s) => REMOVED.test(`${s.name} ${s.kitName}`)),
        `the lane pickers offer ${offered.length} approved sounds, none removed`);
  const kits = new Set(offered.map((s) => s.kitName));
  check(["poketrack 808", "poketrack CR-78", "Optimist LO-FI", "Gogodze", "Big Rusty (sticks)", "Big Rusty (brushes)", "Brushes"].every((k) => kits.has(k)),
        `from every approved kit (${[...kits].join(", ")})`);
  check(retired.length > offered.length && retired.filter((s) => s.status === "retired").every((s) => /Dusty|Soft|Our 808/.test(s.name)),
        `"show retired" adds the retired synth kits' (${retired.length - offered.length})`);
  const cow = ALL.find((s) => s.kind === SRC.OPT && s.b === 12);
  check(cow?.name === "LO-FI cowbell", `Optimist LO-FI's lane 12 is "${cow?.name}"`);
  const ok = await load();
  check(ALL.every((s) => s.kind === SRC.SAMPLE || ok.wb_lane_src(0, s.kind, s.a, s.b) === 1), "every synth sound is a source the engine takes");
  const rnd = lcg(7), picks = new Set([...Array(40)].map(() => pickSound(ALL, 0, null, "synth", rnd)?.kitName));
  check([...picks].every((k) => ["poketrack 808", "poketrack CR-78", "Optimist LO-FI"].includes(k)) && picks.size === 3,
        `a synth song's kick 🎲 picks the synth kits' kicks (${[...picks].join(", ")})`);
  {
    const ex = await load(), call = caller(ex);
    ex.wb_seed(21);
    ex.wb_kit_set(3);
    ex.wb_apply();
    const desk = new Desk(call, C, OPTS, await call("wb_library"));
    await desk.read(true);
    const src = laneSources(desk.drums, C);
    check(src.every((l, i) => (i < 6 ? l[0] === SRC.BUILTIN && l[1] === 3 && l[2] === i : l[0] === SRC.NONE)),
          "with no lanes set, the screen shows the kit's own (Brushes')");
    await desk.level(2, -9);
    const d = desk.drums;
    check(d.lanes[2][3] === -9 && d.lanes[0][0] === SRC.BUILTIN && d.lanes[0][1] === 3, "a level on the kit's own lanes copies them first (heard): hats at -9 dB");
  }
  // edited lanes go with a kept kit through a new seed (staging the kit again would reset them to the kit as shipped)
  {
    const ex = await load(), call = caller(ex);
    ex.wb_seed(21);
    ex.wb_apply();
    const desk = new Desk(call, C, OPTS, await call("wb_library"), { rnd: lcg(9) });
    await desk.read(true);
    await desk.set("kit", DRUMS.PRESET + DRUMS.PT808);
    await desk.lane(1, SRC.OPT, 0, 1);
    await desk.level(1, -5);
    desk.lock("kit", true);
    const lanes = JSON.stringify(desk.drums.lanes), seed = desk.values.seed;
    const r = await desk.dice("seed");
    ex.wb_play(1);
    until(ex, () => ex.wb_live_get(C.RF.N) === desk.values.seed, 10 * SR);
    check(r.ok && desk.values.seed !== seed && JSON.stringify(desk.drums.lanes) === lanes &&
          JSON.stringify(JSON.parse(cstr(ex, ex.wb_live_state())).drums.lanes) === lanes,
          `kit locked, a new seed (${desk.values.seed}) keeps the LO-FI snare at -5 dB on the 808, staged and heard`);
    desk.lock("kit", false);
    const r2 = await desk.dice("seed");
    check(r2.ok && JSON.stringify(desk.drums.lanes) === lanes, "unlocked, the lane edits are hand edits: kept too");
    await desk.rerollAll();
    check(JSON.stringify(desk.drums.lanes) !== lanes, "re-roll all lets them go");
    const now = cstr(ex, ex.wb_state()), ref = await desk.lane(1, 99, 0, 0);
    check(!ref.ok && ref.message && cstr(ex, ex.wb_state()) === now, `a lane the engine refuses changes nothing: "${ref.message}"`);
  }

  console.log("drum screen: save and load (server.py)");
  {
    const { spawn } = await import("node:child_process");
    const { mkdtempSync, rmSync } = await import("node:fs");
    const { tmpdir } = await import("node:os");
    const dir = mkdtempSync(`${tmpdir()}/wb-saved-`);
    const py = `import importlib.util, sys, pathlib
from functools import partial
from http.server import ThreadingHTTPServer
spec = importlib.util.spec_from_file_location("s", sys.argv[1]); m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
m.DATA = pathlib.Path(sys.argv[2]); m.Handler.log_message = lambda *a: None
srv = ThreadingHTTPServer(("127.0.0.1", 0), partial(m.Handler, directory=sys.argv[2]))
print(srv.server_address[1], flush=True); srv.serve_forever()`;
    const srv = spawn("python3", ["-I", "-c", py, new URL("../server.py", import.meta.url).pathname, dir]);
    const port = await new Promise((ok, fail) => { srv.stdout.once("data", (b) => ok(+String(b).trim())); srv.once("error", fail); });
    const f = (p, o) => fetch(`http://127.0.0.1:${port}${p}`, o);
    try {
      const ex = await load(), call = caller(ex);
      ex.wb_seed(21);
      ex.wb_apply();
      const desk = new Desk(call, C, OPTS, await call("wb_library"));
      await desk.read(true);
      await desk.set("kit", DRUMS.PRESET + DRUMS.PTCR78);
      await desk.lane(1, SRC.OPT, 0, 1);
      await desk.level(0, -3);
      await desk.set("kick", "k6");
      for (const [l, b, s, v] of [[1, 1, 14, 2], [2, 0, 1, 1], [5, 1, 7, 1], [4, 0, 0, 1]]) await desk.step(l, b, s, v);
      const g = desk.drums.grid, lanes = desk.drums.lanes;
      const pr = await save("pattern", patternFile("smoke-pattern", g, OPTS), f);
      const kr = await save("kit", kitFile("smoke-kit", lanes, ALL, "poketrack CR-78"), f);
      const names = [await listSaved("pattern", f), await listSaved("kit", f)];
      check(pr.ok && kr.ok && names[0].includes("smoke-pattern") && names[1].includes("smoke-kit"), `saved and listed (${names.flat().join(", ")})`);
      const pf = await getSaved("pattern", "smoke-pattern", f), kf = await getSaved("kit", "smoke-kit", f);
      check(pf.drawn.Snare.includes("o") && kf.sounds.Snare === "LO-FI snare" && kf.sounds.Kick === "CR-78 kick",
            `the files name what they hold (snare: ${pf.drawn.Snare}; ${kf.sounds.Kick}, ${kf.sounds.Snare})`);
      const ex2 = await load(), call2 = caller(ex2);
      ex2.wb_seed(5);
      ex2.wb_apply();
      const d2 = new Desk(call2, C, OPTS, await call2("wb_library"));
      await d2.read(true);
      const r1 = await d2.loadPattern(pf), r2 = await d2.loadKit(kf);
      check(r1.ok && JSON.stringify(d2.drums.grid.lanes) === JSON.stringify(g.lanes) && d2.values.kick === "k6",
            "loaded on another song, the pattern reproduces the grid");
      check(r2.ok && JSON.stringify(d2.drums.lanes) === JSON.stringify(lanes), "... and the kit its lanes and levels");
      const nope = await d2.loadPattern({ name: "no-base", grid: { ...pf.grid, base: 99 } });
      check(!nope.ok && /can't load/.test(nope.message) && JSON.stringify(d2.drums.grid.lanes) === JSON.stringify(g.lanes),
            `a pattern over a base the song can't take is refused, nothing staged: "${nope.message}"`);
      const bad = await save("pattern", { name: "../x" }, f);
      check(!bad.ok && /name/.test(bad.error), `a bad name is refused: "${bad.error}"`);
    } finally {
      srv.kill();
      rmSync(dir, { recursive: true, force: true });
    }
  }
}

console.log("whole-branch review fixes");
{
  const { SRC, DRUMS, PT } = C, retired = (id) => OPTS.kits.find((k) => k.id === id)?.status === "retired";
  const fresh = async (seed, rnd = lcg(5)) => {
    const ex = await load(), call = caller(ex);
    ex.wb_seed(seed);
    ex.wb_apply();
    const desk = new Desk(call, C, OPTS, await call("wb_library"), { rnd });
    await desk.read(true);
    return { ex, desk };
  };
  const LANES808 = ["BD", "SD", "CH", "OH", "MA", "RS", "CH"].map((v) => [SRC.PT, PT.M808, PT[v], 0]);

  // 1: a kit's lanes on a song whose kit is a retired mixed sampled kit (Clean, seed 18: 16 until the DX's sets
  // retired, 2026-11-04, then 17 until the vibes-on-keys sets, 2026-11-05): refused, nothing staged
  {
    const { ex, desk } = await fresh(18);
    const before = cstr(ex, ex.wb_state()), was = desk.values.kit;
    const r = await desk.loadKit({ name: "x", lanes: LANES808 });
    check(was === 8 && !r.ok && /Clean/.test(r.message) && cstr(ex, ex.wb_state()) === before && !ex.wb_pending() && !desk.hand.has("kit"),
          `load kit on the Clean (mixed) kit: refused, nothing staged or pinned ("${r.message}")`);
    const r2 = await desk.loadKit({ name: "y", from: "poketrack 808", lanes: LANES808 });
    check(r2.ok && desk.values.kit === DRUMS.PRESET + DRUMS.PT808 && JSON.stringify(desk.drums.lanes) === JSON.stringify(LANES808),
          `a kit file saved from the 808: the kit row shows poketrack 808 after loading (${desk.values.kit})`);
    const { desk: d3 } = await fresh(18);
    d3.lock("kit", true);
    const r3 = await d3.loadKit({ name: "y", from: "poketrack 808", lanes: LANES808 });
    check(!r3.ok && /Kit is locked/.test(r3.message) && d3.values.kit === 8, `... with the kit locked: blocked ("${r3.message}")`);
  }
  // 1: a kept drawn grid the engine won't take back (every base is legal today: wb_pat_load refused here) is reported
  {
    const { ex } = await fresh(21), call = caller(ex);
    let refuse = false;
    const desk = new Desk((fn, ...a) => (refuse && fn === "wb_pat_load" ? Promise.resolve(0) : call(fn, ...a)), C, OPTS, await call("wb_library"));
    await desk.read(true);
    await desk.step(0, 1, 3, 1);
    refuse = true;
    const r = await desk.set("seed", 300);
    check(r.ok && /drawn pattern couldn't be kept/.test(r.message), `a kept drawn grid that can't be staged again is reported ("${r.message}")`);
  }
  // 2: a regenerate never lands on a retired kit unless the kit row is kept
  {
    const { desk } = await fresh(21);
    const bad = [], forced = [];
    for (let s = 1; s <= 200; s++) {
      const r = await desk.set("seed", s, { dice: true });
      if (retired(desk.values.kit)) bad.push(`${s}:${desk.values.kit}`);
      if (r.moved.includes("kit")) forced.push(s);
    }
    check(!bad.length && forced.length > 100, `seeds 1-200: no landed kit is retired (${bad.join(" ") || "none"}); ${forced.length} replaced, highlighted`);
    const a = (await desk.set("seed", 16, { dice: true }), desk.values.kit);
    await desk.set("seed", 17, { dice: true });
    const b = (await desk.set("seed", 16, { dice: true }), desk.values.kit);
    check(a === b && OPTS.kits.find((k) => k.id === a).family === desk.values.family, `the replacement is the seed's own (16: ${a}, both times), of the family`);
    await desk.set("kit", 6);
    await desk.set("seed", 40);
    check(desk.values.kit === 6, "a hand-set retired kit (show retired) stays through a new seed");
    desk.lock("kit", true);
    await desk.dice("seed", { release: true });
    check(desk.values.kit === 6, "a locked one stays through re-roll all");
    const { desk: d2 } = await fresh(16);
    await d2.set("map", (d2.values.map + 1) % (await d2.call("wb_library")).maps.length);
    check(!retired(d2.values.kit), `a new map regenerates onto an approved kit too (${d2.values.kit})`);
  }
  // 4: a call that throws in the worklet still answers, with the error
  {
    let Proc = null;
    const posted = [];
    globalThis.AudioWorkletProcessor = class { constructor() { this.port = { postMessage: (m) => posted.push(m) }; } };
    globalThis.registerProcessor = (n, cls) => { Proc = cls; };
    await import("../wb-worklet.js");
    const w = new Proc();
    w.ex = { wb_boom: () => { throw new RangeError("out of bounds"); } };
    await w.onMessage({ type: "call", fn: "wb_boom", args: [], id: 7 }).catch(() => {});
    check(posted.some((m) => m.type === "result" && m.id === 7 && /out of bounds/.test(m.error)), "a call that throws in the worklet answers with its error");
  }
  // 5: the ride lane follows the desk's ride row (no ride on a non-sampled kit)
  {
    const { laneInactive } = await import("../drums.js");
    const st = (kit) => ({ values: { kit } });
    check(laneInactive(6, st(16), OPTS) && laneInactive(6, st(3), OPTS) && !laneInactive(6, st(19), OPTS) && !laneInactive(1, st(16), OPTS),
          "the ride lane is inactive on the 808 and Brushes, not on Gogodze; the other lanes always active");
  }
}

console.log("lead loop row");
{
  const ex = await load(), call = caller(ex);
  ex.wb_seed(21);
  ex.wb_apply();
  const desk = new Desk(call, C, OPTS, await call("wb_library"), { rnd: lcg(9) });
  await desk.read(true);
  const ids = ROWS.map((x) => x.id), r = row("leadLoop"), opts = optionsFor(r, desk.state, OPTS);
  check(r && r.kind === "ov" && r.key === "LEAD_LOOP" && ids.indexOf("leadLoop") === ids.indexOf("leadStyle") + 1 &&
        JSON.stringify(opts.map((o) => [o.value, o.name])) === JSON.stringify([[-1, "Default"], [0, "Off"], [1, "On"]]) &&
        desk.values.leadLoop === -1 && ex.wb_ov_get(OV.LEAD_LOOP) === -1, "the Lead loop row follows Lead style: default, off, on; new songs default");
  await desk.set("leadLoop", 1);
  check(desk.values.leadLoop === 1 && ex.wb_ov_get(OV.LEAD_LOOP) === 1, "setting it round-trips through wb_ov_get");
  const old = await desk.snapshot();
  delete old.rows.leadLoop;
  const e2 = await load(), d2 = new Desk(caller(e2), C, OPTS, await caller(e2)("wb_library"));
  const miss = await d2.restore(old);
  check(!miss.length && !d2.message && d2.values.leadLoop === -1 && e2.wb_ov_get(OV.LEAD_LOOP) === -1,
        "an old snapshot without the row restores to Default, no message");
}

console.log("Task 1b: no ear candy; the pad on/off row");
{
  const ex = await load(), call = caller(ex);
  ex.wb_seed(21);
  ex.wb_apply();
  const desk = new Desk(call, C, OPTS, await call("wb_library"), { rnd: lcg(9) });
  await desk.read(true);
  const r = row("padOn"), names = optionsFor(r, desk.state, OPTS).map((o) => o.name);
  check(!row("candy") && !("CANDY" in V) && !OPTS.instruments.some((i) => i.roles.includes("candy")) && !OPTS.families.follow.includes("candy"),
        "no ear candy row, role or option");
  check(r && r.kind === "ov" && JSON.stringify(names) === JSON.stringify(["As rolled", "Off", "On"]) && desk.values.padOn === -1 &&
        ex.wb_ov_get(OV.PAD_ON) === -1, `a "Pad on/off" row: as rolled, off, on (${names.join(", ")}); a new song's is as rolled`);
  const pads = [];   // seed 2: its pad plays from bar 1
  for (const v of [0, 1]) {
    const e = await load();
    e.wb_seed(2);
    e.wb_ov_set(OV.PAD_ON, v);
    e.wb_apply();
    e.wb_play(1);
    pads.push(render(e, 12 * SR));
  }
  check(pads[0].l.some((x, i) => x !== pads[1].l[i]), "pad off and pad on play different songs");
  const d = await desk.dice("padOn");
  check(d.ok && [0, 1].includes(desk.values.padOn) && ex.wb_ov_get(OV.PAD_ON) === desk.values.padOn,
        `its dice pick off or on (${desk.values.padOn}), never "as rolled"`);
  desk.lock("padOn", true);
  const kept = desk.values.padOn;
  await desk.set("seed", 99);
  check(desk.values.padOn === kept && ex.wb_ov_get(OV.PAD_ON) === kept, "locked, it survives a new seed");
  // a snapshot from before this task: an ear candy row, no pad on/off row; it restores, the pad as rolled
  const old = await desk.snapshot();
  delete old.rows.padOn;
  old.rows.candy = 14;
  old.locks = [];
  const e2 = await load(), d2 = new Desk(caller(e2), C, OPTS, await caller(e2)("wb_library"));
  const miss = await d2.restore(old);
  check(!miss.length && d2.values.padOn === -1 && e2.wb_ov_get(OV.PAD_ON) === -1,
        `an older snapshot (no pad row, an ear candy row) restores with the pad as rolled (mismatch: ${miss.join(", ") || "none"})`);
  // a snapshot naming a retired lead (the sax): it restores, the song on the approved lead the engine stands in (the
  // flute), said in the message, not a mismatch
  const sx = await desk.snapshot();
  sx.rows.lead = ins("Sax");
  sx.locks = [];
  const e3 = await load(), d3 = new Desk(caller(e3), C, OPTS, await caller(e3)("wb_library"));
  const miss3 = await d3.restore(sx);
  check(!miss3.length && d3.values.lead === ins("Flute") && JSON.parse(cstr(e3, e3.wb_state())).roles[C.V.LEAD] === ins("Flute") &&
        /Sax is retired/.test(d3.message),
        `a snapshot with the retired sax restores on the flute (mismatch: ${miss3.join(", ") || "none"}; "${d3.message}")`);
  // the leads retired by the lead styles build: the kalimba, saw lead and synth pluck, and the Emilyguitar as a lead
  // (it stays on the keys): each restores on its stand-in (the flute or the vibes), said
  for (const [name, to, said] of [["Kalimba", "Flute", "Kalimba is retired;"], ["Saw lead", "Vibes (modeled vibraphone)", "Saw lead is retired;"],
                                  ["Synth pluck", "Flute", "Synth pluck is retired;"], ["Emilyguitar", "Flute", "Emilyguitar is retired as a lead;"]]) {
    const sn = await desk.snapshot();
    sn.rows.lead = ins(name);
    sn.locks = [];
    const e4 = await load(), d4 = new Desk(caller(e4), C, OPTS, await caller(e4)("wb_library"));
    const miss4 = await d4.restore(sn);
    check(!miss4.length && d4.values.lead === ins(to) && JSON.parse(cstr(e4, e4.wb_state())).roles[C.V.LEAD] === ins(to) &&
          d4.message.includes(said),
          `a snapshot with the ${name} on the lead restores on the ${to} (mismatch: ${miss4.join(", ") || "none"}; "${d4.message}")`);
  }
  { // the upright (removed 2026-10-31) on the bass: it restores on the electric bass, said
    const sn = await desk.snapshot();
    sn.rows.bass = ins("Upright bass");
    sn.locks = [];
    const e5 = await load(), d5 = new Desk(caller(e5), C, OPTS, await caller(e5)("wb_library"));
    const miss5 = await d5.restore(sn);
    check(!miss5.length && d5.values.bass === ins("Electric bass") &&
          JSON.parse(cstr(e5, e5.wb_state())).roles[C.V.BASS] === ins("Electric bass") &&
          d5.message.includes("Bass: Upright bass is retired;"),
          `a snapshot with the upright on the bass restores on the electric bass (mismatch: ${miss5.join(", ") || "none"}; "${d5.message}")`);
  }
  // the keys removed 2026-11-04 (the Wurli, the DX electric piano): each restores on the Rhodes, said
  for (const name of ["Wurli", "DX electric piano"]) {
    const sn = await desk.snapshot();
    sn.rows.keys = ins(name);
    sn.locks = [];
    const e6 = await load(), d6 = new Desk(caller(e6), C, OPTS, await caller(e6)("wb_library"));
    const miss6 = await d6.restore(sn);
    check(!miss6.length && d6.values.keys === ins("Rhodes") &&
          JSON.parse(cstr(e6, e6.wb_state())).roles[C.V.CHORD] === ins("Rhodes") &&
          d6.message.includes(`Keys: ${name} is retired;`),
          `a snapshot with the ${name} on the keys restores on the Rhodes (mismatch: ${miss6.join(", ") || "none"}; "${d6.message}")`);
  }
  const s8 = JSON.parse(cstr(ex, ex.wb_state()));
  s8.ov = s8.ov.slice(0, 8);
  check(fromEngine(s8, C, OPTS).padOn === -1 && fromEngine({ ...s8, ov: s8.ov.concat([0, 0, 0]) }, C, OPTS).padOn === -1,
        "an 8- or 11-entry ov list reads the pad as rolled");
}

console.log("the section strip, mute/solo and listening state (Task 3)");
{
  const { sectionAt, PARTS, positionOf, loopAt } = await import("../desk.js");
  const ex = await fresh(), call = caller(ex);
  const desk = new Desk(call, C, OPTS, await call("wb_library"), { rnd: lcg(13), probe: await prober() });
  await desk.read(true);
  const secs = await desk.sections();
  // looping a section: only its last bar answers, with its start; played through, it comes back to its first bar
  {
    const s = secs[1], last = s.start + s.bars;
    check(loopAt(secs, last) === s.start && loopAt(secs, last - 1) === (s.bars === 1 ? s.start : null) && loopAt(secs, 0) === null,
          "loopAt: a section's last bar → its start, other bars → null");
    const lx = await fresh();
    lx.wb_jump(s.start); lx.wb_play(1);
    let back = 0;
    for (let pass = 0; pass < 2; pass++) {
      until(lx, () => lx.wb_bar() === last, 120 * SR);
      lx.wb_jump(loopAt(secs, lx.wb_bar()));
      until(lx, () => lx.wb_bar() !== last, 60 * SR);
      if (lx.wb_bar() === s.start + 1) back++;
    }
    check(back === 2, `a looped section comes back to its first bar each time (${back} of 2)`);
  }
  check(secs.length >= 3 && sectionAt(secs, 1) === secs[0] && sectionAt(secs, secs[2].start + 1) === secs[2] &&
        sectionAt(secs, secs.at(-1).start + secs.at(-1).bars) === secs.at(-1) && sectionAt(secs, 0) === null && sectionAt(null, 3) === null,
        "sectionAt: a 1-based bar's section (the first and last bars of each, none outside)");
  // clicking a section stages a jump that lands at its first bar
  const bpm = ex.wb_roll_get(RF.TEMPO);
  ex.wb_play(1);
  until(ex, () => ex.wb_bar() === 2, 60 * SR);
  const res = await desk.jumpTo(secs[2].start, secs[2].name);
  check(res.ok && ex.wb_bar() === 2 && ex.wb_pending() === 1, "a section click stages the jump: the bar sounding plays on");
  const waited = until(ex, () => ex.wb_bar() !== 2, 60 * SR);
  check(ex.wb_bar() === secs[2].start + 1 && waited <= barLen(bpm) + Q, `it lands at the section's first bar (bar ${ex.wb_bar()}, wanted ${secs[2].start + 1})`);

  {   // positionOf: the section of the song playing, not the committed one's (a change waiting)
    const p = positionOf({ seconds: 3.5, bar: secs[1].start + 2 }, secs);
    check(p.seconds === 3.5 && p.bar === secs[1].start + 2 && p.section === secs[1].name && positionOf({ seconds: 0, bar: 0 }, secs).section === null &&
          positionOf({ seconds: 0, bar: 1 }, []).section === null, "positionOf: { seconds, bar, section name or null }");
    const x = await fresh(), pend = sections(x);
    x.wb_play(1);
    until(x, () => x.wb_bar() === 2, 60 * SR);
    for (let i = 0; i < 20 && x.wb_roll_get(RF.BARS) === 56; i++) x.wb_roll_dice(RF.BARS);
    x.wb_apply();
    const heardNow = JSON.parse(cstr(x, x.wb_sections(1))), comNow = sections(x);
    check(JSON.stringify(heardNow) === JSON.stringify(pend) && JSON.stringify(comNow) !== JSON.stringify(pend),
          "wb_sections(1) is the song playing while a change waits (wb_sections(0): the committed one)");
  }
  {   // the drop is the engine's, in wb_apply itself
    const RF_SEED = Object.keys(RF).length;   // wb_live_get(RF_N): the seed: no bar line can land between a jump and the new song
    for (const playing of [true, false]) {
      const x = await fresh();
      const s = sections(x);
      if (playing) { x.wb_play(1); until(x, () => x.wb_bar() === 2, 60 * SR); }
      x.wb_jump(s[2].start);
      x.wb_seed(777);
      x.wb_apply();
      if (playing) until(x, () => x.wb_bar() !== 2, 60 * SR); else { x.wb_play(1); render(x, Q); }
      check(x.wb_bar() === 1 && x.wb_catchups() === (playing ? 1 : 0) && x.wb_live_get(RF.N) === 777,
            `${playing ? "playing" : "stopped"}: a jump, then a new seed through wb_apply alone: one landing, at bar ${x.wb_bar()} (wanted 1) of the new song`);
    }
    const x = await fresh(), s = sections(x);
    x.wb_play(1);
    until(x, () => x.wb_bar() === 2, 60 * SR);
    x.wb_jump(s[2].start);
    x.wb_roll_set(RF.TEMPO, x.wb_roll_get(RF.TEMPO) === 85 ? 75 : 85);
    x.wb_apply();
    until(x, () => x.wb_bar() !== 2, 60 * SR);
    check(x.wb_bar() === s[2].start + 1, "a change that is not a new song keeps the jump");
  }
  // Ruling: a seed or map change staged after a jump drops the jump (the new song starts at bar 1)
  for (const [what, row_, val] of [["seed", "seed", 4321], ["map", "map", null]]) {
    const e = await fresh(), cl = caller(e);
    const d = new Desk(cl, C, OPTS, await cl("wb_library"), { rnd: lcg(17), probe: await prober() });
    await d.read(true);
    const s = await d.sections();
    e.wb_play(1);
    until(e, () => e.wb_bar() === 2, 60 * SR);
    await d.jumpTo(s[2].start, s[2].name);
    const v = val ?? (d.values.map + 1) % (await cl("wb_library")).maps.length;
    const r = await d.set(row_, v);
    until(e, () => e.wb_bar() !== 2, 60 * SR);
    check(r.ok && e.wb_bar() === 1 && e.wb_catchups() === 1,
          `a ${what} change staged after a jump drops it: the new song lands at bar ${e.wb_bar()} (wanted 1), one catch-up (${e.wb_catchups()})`);
  }
  {   // stopped: Play starts at bar 1 of the new song, not at the old song's section
    const e = await fresh(), cl = caller(e);
    const d = new Desk(cl, C, OPTS, await cl("wb_library"), { rnd: lcg(17), probe: await prober() });
    await d.read(true);
    await d.jumpTo((await d.sections())[2].start);
    await d.set("seed", 999);
    e.wb_play(1);
    render(e, Q);
    check(e.wb_bar() === 1, `stopped: a jump then a new seed, Play starts at bar ${e.wb_bar()} (wanted 1)`);
  }

  // mute and solo: the page's state survives a new seed, the engine keeps it, snapshots carry it
  const e = await fresh(), cl = caller(e);
  const d = new Desk(cl, C, OPTS, await cl("wb_library"), { rnd: lcg(19), probe: await prober() });
  await d.read(true);
  for (const f of [RF.DUST, RF.NOISE, RF.AMBIENCE]) e.wb_roll_set(f, 0);
  e.wb_apply();
  for (const p of Object.keys(PARTS)) await d.mute(p, true);   // all five, the pad too (a song may have none)
  await d.mute("pad", false);
  await d.solo("keys");
  await d.set("seed", 31);
  check(d.listen.mute.size === 4 && !d.listen.mute.has("pad") && d.listen.solo === "keys", "mute/solo state survives a new seed (the page's)");
  e.wb_play(1);
  const solo = render(e, 6 * SR);
  check(db(solo) > -60, `the solo is still heard in the new song (${db(solo).toFixed(1)} dB)`);
  const snap = await d.snapshot();
  check(snap.version === 1 && snap.listen.solo === "keys" && [...snap.listen.mute].sort().join() === "bass,drums,keys,lead",
        "the snapshot holds listen { mute, solo }");
  await d.solo(null);
  for (const p of Object.keys(PARTS)) await d.mute(p, false);
  const e2 = await load(), c2 = caller(e2);
  const d2 = new Desk(c2, C, OPTS, await c2("wb_library"), { rnd: lcg(19), probe: await prober() });
  await d2.read(true);
  await d2.restore(JSON.parse(JSON.stringify(snap)));
  e2.wb_play(1);
  const heard = render(e2, 6 * SR);
  check(d2.listen.solo === "keys" && d2.listen.mute.size === 4 && db(heard) > -60,
        `a replayed snapshot restores mute and solo (solo ${d2.listen.solo}, ${d2.listen.mute.size} muted, ${db(heard).toFixed(1)} dB)`);
  // the soloed keys alone are quieter than the whole song
  const whole = await fresh(31);
  whole.wb_play(1);
  check(db(heard) < db(render(whole, 6 * SR)) - 1, "and it is the keys alone, not the whole song");

  // a version-1 snapshot from before this task (no listen) restores with no mute or solo, over a muted desk
  const old = JSON.parse(JSON.stringify(snap));
  delete old.listen;
  old.rows = { ...old.rows };
  await d2.mute("lead", true);
  await d2.restore(old);
  e2.wb_play(0);
  e2.wb_play(1);
  const open = render(e2, 6 * SR);
  check(d2.listen.mute.size === 0 && d2.listen.solo === null && db(open) > db(heard) + 1,
        `an old snapshot (no listen) restores with no mute or solo (${db(open).toFixed(1)} dB against ${db(heard).toFixed(1)})`);
  const s8 = JSON.parse(cstr(e2, e2.wb_state()));
  s8.ov = s8.ov.slice(0, 8);
  old.rows = fromEngine(s8, C, OPTS);
  const d3 = new Desk(c2, C, OPTS, await c2("wb_library"), { rnd: lcg(19), probe: await prober() });
  await d3.read(true);
  await d3.restore(old);
  check(d3.listen.mute.size === 0 && d3.listen.solo === null, "an old snapshot with an 8-entry override list restores with no mute or solo");
}

// ---- the parts: re-rolls, history, locks, reactions, feedback per part (Task 5) ----
console.log("parts: re-rolls, history, locks, reactions (Task 5)");
{
  const { PARTS, PART_ROW, notesOf } = await import("../desk.js");
  const { readEvents } = await import("../wb.js");
  const { layout } = await import("../roll.js");
  const { cellOf } = await import("../drums.js");
  const { replay, wav } = await import("./replay.mjs");
  // wb.js's events: the committed song's, on a probe instance
  const eventer = async () => {
    const pex = await load();
    return async (bytes) => {
      new Uint8Array(pex.memory.buffer, pex.wb_commit_ptr(), pex.wb_stage_size()).set(bytes);
      pex.wb_events(0);
      return { events: readEvents(pex) };
    };
  };
  const deskOn = async (seed, rnd = lcg(29)) => {
    const ex = await fresh(seed), call = caller(ex);
    const desk = new Desk(call, C, OPTS, await call("wb_library"), { rnd, probe: await prober(), events: await eventer() });
    await desk.read(true);
    await desk.refresh();
    return { ex, call, desk };
  };
  const evs = (desk) => JSON.stringify(desk.partEv);
  const names = Object.keys(PARTS);
  check(JSON.stringify(names) === JSON.stringify(["keys", "bass", "lead", "pad", "drums"]) && !names.includes("candy") &&
        names.every((p) => row(PART_ROW[p]).kind === "reroll"), "five parts (no ear candy), each re-rolled by its salt row");

  // each part's 🎲: its own events change, it lands at the next bar line, and the page lists whatever else moved
  {
    // seed 9: a pad, keys the voicing salt moves (seed 21's don't; seed 3's guitar strums since 2026-11-04, its shapes
    // fixed) and a bass line its salt moves (seed 2's only takes new velocities; seed 7's had only the root-fifth line
    // to roll to, gone 2026-11-04)
    const { ex, desk } = await deskOn(9);
    ex.wb_play(1);
    until(ex, () => ex.wb_bar() === 2, 60 * SR);
    const bpm = ex.wb_roll_get(RF.TEMPO), out = [];
    for (const p of names) {
      const before = { ...desk.partEv }, was = ex.wb_catchups(), salt = desk.values[PART_ROW[p]], bar = ex.wb_bar();
      const res = await desk.reroll(p);
      const own = notesOf(before[p]) !== notesOf(desk.partEv[p]);   // (heard: velocity alone doesn't count)
      const moved = names.filter((q) => q !== p && notesOf(before[q]) !== notesOf(desk.partEv[q]));
      const said = moved.every((q) => res.message.toLowerCase().includes(`${q} moved`)) &&
                   names.filter((q) => !moved.includes(q)).every((q) => !res.message.toLowerCase().includes(`${q} moved`));
      const waited = until(ex, () => ex.wb_catchups() > was, 60 * SR);
      const live = JSON.parse(cstr(ex, ex.wb_live_state())).ov[OV[row(PART_ROW[p]).key]];
      const ok = res.ok && own && said && desk.values[PART_ROW[p]] !== salt && desk.values[PART_ROW[p]] > 0 &&
                 live === desk.values[PART_ROW[p]] && waited <= barLen(bpm) + Q && ex.wb_bar() === bar + 1;
      out.push(`${p}${ok ? "" : " BAD"}${moved.length ? ` (${moved.join("+")} moved)` : ""}`);
    }
    check(out.every((x) => !x.includes("BAD")), `each part's 🎲 changes its events, lands at the next bar, lists what else moved: ${out.join(", ")}`);
    check(OPTS.kickPhrases.some((k) => k.id === desk.values.kick), `the drums' 🎲 drew an approved kick phrase (${desk.values.kick}) and a variation (${desk.values.drumsVar})`);

    // ◀ returns the previous roll exactly (every part's events), ▶ the next
    const res = [];
    for (const p of ["bass", "drums"]) {
      const a = evs(desk), ka = desk.values.kick;
      await desk.reroll(p);
      const b = evs(desk);
      const back = await desk.stepPart(p, -1), atA = evs(desk) === a && desk.values.kick === ka;
      const fwd = await desk.stepPart(p, 1), atB = evs(desk) === b;
      res.push(`${p} ${back.ok && fwd.ok && a !== b && atA && atB ? "ok" : "BAD"}`);
    }
    check(res.every((x) => x.endsWith("ok")), `◀ returns the previous roll exactly (all events, the drums' kick too), ▶ the next (${res.join(", ")})`);

    // Review Focus 4: ◀ at the first roll and ▶ at the last change nothing
    const h = desk.parts.bass;
    const last = await desk.stepPart("bass", 1);
    while (desk.parts.bass.at > 0) await desk.stepPart("bass", -1);
    const st1 = cstr(ex, ex.wb_state()), e1 = evs(desk);
    const first = await desk.stepPart("bass", -1);
    render(ex, Math.ceil(barLen(bpm) / Q) * Q + Q);
    check(h.at === h.history.length - 1 && !last.ok && desk.parts.bass.at === 0 && !first.ok &&
          cstr(ex, ex.wb_state()) === st1 && evs(desk) === e1 && desk.values.bassLine === h.history[0].salt,
          `Review Focus 4: ▶ at the last roll and ◀ at the first change nothing ("${last.message}" / "${first.message}")`);
  }

  // the page's buttons: ◀ ▶ disabled at the ends (cantRoll and the history say so)
  {
    const { desk } = await deskOn(21);
    const p0 = desk.parts.lead;
    await desk.reroll("lead");
    const p1 = desk.parts.lead;
    check(p0.history.length === 1 && p0.at === 0 && p1.history.length === 2 && p1.at === 1 && p1.salt === p1.history[1].salt,
          "a part's history: one roll as rolled, then each 🎲 on the end (at the newest)");
  }

  // lifetimes: a new seed or map resets unlocked parts (salts and histories), locked ones keep theirs; re-roll all
  // re-rolls the unlocked parts; a family change and the pad on/off keep the salts
  {
    const { ex, desk } = await deskOn(21);
    await desk.reroll("bass");
    await desk.reroll("lead");
    await desk.reroll("lead");
    desk.lock(PART_ROW.lead, true);
    const lead = desk.parts.lead;
    await desk.set("seed", 4242);
    const p = desk.parts;
    check(p.bass.salt === 0 && p.bass.history.length === 1 && ex.wb_ov_get(OV.BASS_SALT) === 0 &&
          p.lead.salt === lead.salt && p.lead.history.length === 3 && ex.wb_ov_get(OV.PHRASE_SALT) === lead.salt && p.lead.locked,
          `a new seed resets the unlocked parts (bass salt ${p.bass.salt}) and keeps the locked lead (${p.lead.salt}, ${p.lead.history.length} rolls)`);
    await desk.reroll("bass");
    await desk.reroll("keys");
    const map = desk.values.map, other = optionsFor(row("map"), desk.state, OPTS, { lib: desk.lib }).find((o) => o.value !== map).value;
    await desk.set("map", other);
    check(desk.parts.bass.salt === 0 && desk.parts.keys.salt === 0 && desk.parts.keys.history.length === 1 &&
          ex.wb_ov_get(OV.VOICING_SALT) === 0 && desk.parts.lead.salt === lead.salt,
          "a new map resets them too (the keys' voicings and the bass back to as rolled), the locked lead kept");
    await desk.reroll("bass");
    const bass = desk.values.bassLine, fam = desk.values.family === "acoustic" ? "synth" : "acoustic";
    const f = await desk.set("family", fam);
    check(f.ok && desk.values.bassLine === bass && ex.wb_ov_get(OV.BASS_SALT) === bass, `a family change keeps the salts (${f.ok ? "ok" : f.message})`);
    desk.lock("seed", true);
    const before = desk.parts;
    const all = await desk.rerollAll();
    // (a map its dice drew starts the unlocked parts' histories over first)
    const after = desk.parts, rolledP = names.filter((q) => after[q].salt !== before[q].salt && after[q].salt &&
                                                     after[q].history.at(-1).salt === after[q].salt && after[q].at === after[q].history.length - 1);
    // (a part with one roll in the song, or no pad: re-roll all names it)
    const want = names.filter((q) => q !== "lead" && !all.message.includes(`${q[0].toUpperCase()}${q.slice(1)}: `) && !(q === "pad" && /No pad/.test(all.message)));
    check(all.ok && want.length >= 2 && want.every((q) => rolledP.includes(q)) && after.lead.salt === lead.salt,
          `re-roll all (seed locked) re-rolls the unlocked parts (${rolledP.join(", ")}), not the locked lead`);
  }

  // the pad: absent from a song, its 🎲 and ◀ ▶ wait; its salt and history are kept, the same roll plays when it's back
  {
    const { ex, desk } = await deskOn(21);
    await desk.reroll("pad");
    await desk.reroll("bass");
    const pad = desk.parts.pad, padEv = JSON.stringify(desk.partEv.pad), bass = desk.values.bassLine;
    await desk.set("padOn", 0);
    const r = await desk.reroll("pad"), b = await desk.stepPart("pad", -1);
    check(desk.padless && !r.ok && /no pad/i.test(r.message) && !b.ok && desk.parts.pad.salt === pad.salt &&
          ex.wb_ov_get(OV.PAD_SALT) === pad.salt && desk.values.bassLine === bass,
          `no pad in the song: its 🎲 and ◀ are refused ("${r.message}"), its salt kept, the pad on/off resets no salt`);
    await desk.set("padOn", 1);
    check(!desk.padless && JSON.stringify(desk.parts.pad) === JSON.stringify(pad) && JSON.stringify(desk.partEv.pad) === padEv,
          "the pad back on plays the same roll (salt, history and events)");
    const { desk: d6 } = await deskOn(6);
    check(d6.padless && !(await d6.reroll("pad")).ok && d6.cantRoll("pad"), "a song rolled without a pad (seed 6): the pad's 🎲 waits");
  }

  // the drums' 🎲 is refused while the grid holds hand edits (◀ ▶ too); the kick phrase row keeps its own 🎲
  {
    const { ex, desk } = await deskOn(21);
    await desk.reroll("drums");
    const g = desk.drums.grid, cell = cellOf(g.lanes[1], 0, 3);
    await desk.step(1, 0, 3, cell ? 0 : 1);
    const st = cstr(ex, ex.wb_state());
    const r = await desk.reroll("drums"), b = await desk.stepPart("drums", -1);
    check(desk.drums.grid.edited && !r.ok && /drawn pattern/.test(r.message) && !b.ok && cstr(ex, ex.wb_state()) === st,
          `with hand edits in the grid the drums' 🎲 and ◀ are refused, nothing staged ("${r.message}")`);
    const was = desk.values.kick, k = await desk.dice("kick");
    check(k.ok && desk.values.kick !== was && OPTS.kickPhrases.some((x) => x.id === desk.values.kick),
          `the kick phrase row's own 🎲 still works (${was} -> ${desk.values.kick}${k.ok ? "" : `: ${k.message}`})`);
  }

  // a part whose 🎲 finds no other roll: seed 26's keys have one voicing path (seed 21's until the keys' minor 9th
  // ban, 2026-10-30, opened it a second) (the salt reaches the first chord's
  // spread only, and the loop converges); the strummed guitar never reads the salt. Refused, saying why; nothing staged,
  // nothing on the history; ◀ ▶ unaffected. Seed 2's keys roll (the each-part check above).
  {
    const { ex, desk } = await deskOn(26);
    const st = cstr(ex, ex.wb_state()), r = await desk.reroll("keys");
    check(!r.ok && /only one voicing fits this progression/.test(r.message) && desk.parts.keys.history.length === 1 &&
          desk.values.voicings === 0 && cstr(ex, ex.wb_state()) === st && desk.cantRoll("keys") === r.message && !desk.cantRoll("keys", true),
          `seed 26's keys 🎲: refused ("${r.message}"), history unchanged, nothing staged; the 🎲 waits`);
    const h = await desk.set("voicings", 999);
    check(h.ok && desk.parts.keys.history.length === 1, "a salt set by hand that changes no note doesn't go on the history");
    await desk.set("keys", ins("Nylon guitar"));
    const held = await desk.set("comp", 0);
    check(!held.ok && /the guitars only strum/.test(held.message),
          `held comping on the nylon: refused ("${held.message}")`);
    await desk.set("comp", 5);
    const g = await desk.reroll("keys");
    check(ex.wb_comp_applied() === 5 && !g.ok && /strummed guitar keeps its shapes/.test(g.message),
          `the strummed guitar's keys 🎲: refused ("${g.message}")`);
    const b = await desk.reroll("bass");
    check(b.ok && desk.parts.bass.history.length === 2 && desk.cantRoll("keys") === null,
          "any change lets the keys' 🎲 try again");
  }

  // the drums' 🎲 over a kick phrase set by hand says so
  {
    const { desk } = await deskOn(21);
    await desk.set("kick", "k3");
    const r = await desk.reroll("drums");
    check(r.ok && desk.values.kick !== "k3" && r.message.includes(`Kick phrase: ${desk.values.kick} (was your k3).`),
          `the drums' 🎲 replacing a hand-set kick phrase says so ("${r.message}")`);
  }

  // re-roll all (seed locked): the drums' history entry is the kick that plays (the kick row isn't rolled over it)
  {
    const { desk } = await deskOn(21, lcg(31));
    desk.lock("seed", true);
    await desk.rerollAll();
    const d = desk.parts.drums;
    check(d.history[d.at].kick === desk.values.kick && d.history[d.at].salt === desk.values.drumsVar,
          `re-roll all: the drums' newest roll is what plays (kick ${desk.values.kick})`);
  }

  // a lead roll that moves the keys gives the keys-room reason (a seed whose lead rolls move the keys' comping)
  {
    const said = [];
    for (let seed = 2; seed <= 12 && !said.length; seed++) {
      const { desk } = await deskOn(seed, lcg(37));
      for (let i = 0; i < 8 && !said.length; i++) {
        const r = await desk.reroll("lead");
        if (/Keys moved/.test(r.message)) said.push(r.message);
      }
    }
    check(said.length && said.every((m) => /Keys moved: they make room for the lead/.test(m)),
          `a lead roll that moves the keys: "${said[0] ?? "(none moved them)"}"`);
  }

  // Final review 1: a roll whose only difference is velocity (or order within a step) sounds the same: not a new roll.
  // A bass salt that moves only the bass's velocities is never committed; the drums with their kick locked (the drums
  // salt is velocity and lateness jitter only) are refused, saying why
  {
    const e = (bar, step, role, note, vel, len) => ({ bar, step, role, note, vel, len });
    check(notesOf([e(0, 0, 1, 40, 80, 4), e(0, 0, 2, 60, 90, 2)]) === notesOf([e(0, 0, 2, 60, 70, 2), e(0, 0, 1, 40, 99, 4)]) &&
          notesOf([e(0, 0, 1, 40, 80, 4)]) !== notesOf([e(0, 0, 1, 41, 80, 4)]) && notesOf([e(0, 0, 1, 40, 80, 4)]) !== notesOf([e(0, 0, 1, 40, 80, 3)]),
          "notesOf: velocity and the order within a step don't count; a note or a length does");
    // seed 21's bass salts: one that moves only velocities, one that moves notes (found on a probe)
    const probe = await load(), { readEvents: rd } = await import("../wb.js");
    const bassOf = (salt) => {
      probe.wb_seed(21);
      probe.wb_ov_set(OV.BASS_SALT, salt);
      probe.wb_apply();
      probe.wb_events(0);
      return rd(probe).filter((x) => x.role === V.BASS);
    };
    const b0 = bassOf(0);
    let velOnly = 0, moves = 0;
    for (let sl = 1; sl < 200 && !(velOnly && moves); sl++) {
      const b = bassOf(sl);
      if (notesOf(b) === notesOf(b0)) { if (!velOnly && JSON.stringify(b) !== JSON.stringify(b0)) velOnly = sl; }
      else moves ||= sl;
    }
    const at = (...salts) => { const xs = salts.map((x) => (x - 0.5) / 0x7ffffffe); let i = 0; return () => xs[Math.min(i++, xs.length - 1)]; };
    const { desk: dv } = await deskOn(21, at(velOnly));
    const rv = await dv.reroll("bass");
    check(velOnly && !rv.ok && /no other bass line/.test(rv.message) && dv.values.bassLine === 0 && dv.parts.bass.history.length === 1,
          `a bass salt that moves only velocities (${velOnly}) isn't a new roll: refused ("${rv.message}"), nothing on the history`);
    const { desk: dm } = await deskOn(21, at(velOnly, moves));
    const rm = await dm.reroll("bass");
    check(moves && rm.ok && dm.values.bassLine === moves && dm.parts.bass.history.length === 2,
          `... the 🎲 skips it and commits the next salt that moves notes (${dm.values.bassLine}, wanted ${moves})`);
    const { desk: dd } = await deskOn(21);
    dd.lock("kick", true);
    const rd2 = await dd.reroll("drums");
    check(!rd2.ok && /no other variation fits this pattern/.test(rd2.message) && dd.values.drumsVar === 0 && dd.parts.drums.history.length === 1,
          `seed 21's drums with the kick locked (jitter only): refused ("${rd2.message}")`);
  }

  // Final review 2: a locked drums part keeps its kick phrase through a new seed and re-roll all; the kick row
  // can't move under it
  {
    const { ex, desk } = await deskOn(21);
    await desk.reroll("drums");
    const kick = desk.values.kick, salt = desk.values.drumsVar;
    desk.lock(PART_ROW.drums, true);
    await desk.set("seed", 4343);
    const d1 = desk.parts.drums;
    check(desk.values.kick === kick && desk.values.drumsVar === salt && d1.history[d1.at].kick === kick && JSON.parse(cstr(ex, ex.wb_state())).kick[2] === 1,
          `a locked drums part keeps its kick (${kick}) and salt through a new seed`);
    await desk.rerollAll();
    const d2 = desk.parts.drums;
    check(desk.values.kick === kick && desk.values.drumsVar === salt && d2.history[d2.at].kick === kick, "... and through re-roll all");
    desk.lock("seed", true);
    await desk.rerollAll();
    const k = await desk.set("kick", kick === "k2" ? "k3" : "k2");
    check(desk.values.kick === kick && !k.ok && /drums part is locked/.test(k.message),
          `... and re-roll all with the seed locked; the kick row can't move it ("${k.message}")`);
  }

  // Final review 5: the kick row changed on its own (by hand or its 🎲) goes on the drums' history: "n of m" is what plays
  {
    const { desk } = await deskOn(21);
    await desk.set("kick", "k3");
    const h1 = desk.parts.drums;
    await desk.dice("kick");
    const h2 = desk.parts.drums;
    check(h1.history.length === 2 && h1.history[h1.at].kick === "k3" && h2.history.length === 3 && h2.at === 2 &&
          h2.history[h2.at].kick === desk.values.kick && desk.values.kick !== "k3",
          `the kick row's own changes go on the drums' history (${h2.history.map((x) => x.kick || "own").join(", ")})`);
    const back = await desk.stepPart("drums", -1);
    check(back.ok && desk.values.kick === "k3", "and ◀ steps back to the hand-picked kick");
  }

  // a keys-room reaction: a bass salt that moves the lead, and the keys make room for it. (The lead styles keep clear
  // of every bass line the salt draws in songs 1-100, so no song shows it: the reasons are checked as the desk words
  // them.)
  {
    const { reaction } = await import("../desk.js");
    const said = ["lead", "keys"].map((p) => reaction(p, "bass", ["lead", "keys"])).join(" ");
    check(said === "Lead moved: it keeps clear of the bass. Keys moved: they make room for the lead.",
          `a bass roll that moves the lead and the keys says so ("${said}")`);
  }

  // Review Focus 5: a muted part re-rolls (its roll greyed, new notes in it); unmuting plays the new roll
  {
    const { ex, desk } = await deskOn(21);
    await desk.mute("bass", true);
    const bassIn = desk.partEv.bass[0].bar + 1;   // re-rolled in the bass's first bar (1-based), so it lands where it plays
    ex.wb_play(1);
    until(ex, () => ex.wb_bar() === bassIn, 120 * SR);
    const committed = async () => (await desk.events(await desk.call("wb_commit_bytes"))).events;
    const old = desk.partEv.bass, all0 = await committed(), was = ex.wb_catchups();
    // (a roll whose notes differ in the bars heard next: the hash below can't see a velocity alone)
    const win = (evs) => evHash(evs.filter((e) => e.bar >= bassIn && e.bar < bassIn + 3));
    let r;
    for (let i = 0; i < 8 && (!r || win(await committed()) === win(all0)); i++) r = await desk.reroll("bass");
    const lay = layout([].concat(...Object.values(desk.partEv)), [], 40, V);
    check(r.ok && JSON.stringify(desk.partEv.bass) !== JSON.stringify(old) && desk.listen.mute.has("bass") &&
          lay.lanes.find((l) => l.role === "bass").notes.length === desk.partEv.bass.length,
          "a muted part re-rolls: new events, still muted (the roll draws them greyed)");
    until(ex, () => ex.wb_catchups() > was, 60 * SR);
    const landed = ex.wb_bar() - 1;   // from 0
    await desk.mute("bass", false);
    await desk.solo("bass");
    const heard = render(ex, 3 * Math.ceil(barLen(ex.wb_roll_get(RF.TEMPO))));   // three bars of it
    const n = ex.wb_heard_count(), from = (e) => e.bar >= landed;
    const want = evHash((await committed()).filter(from).slice(0, n)), oldHash = evHash(all0.filter(from).slice(0, n));
    check((ex.wb_heard_hash() >>> 0) === want && want !== oldHash && db(heard) > -60,
          `unmuted, the new roll plays (${n} events since the landing match the new roll's, not the old; bass alone ${db(heard).toFixed(1)} dB)`);
    await desk.solo(null);
  }

  // feedback on a part: the snapshot carries the parts; it replays to the audio heard; old snapshots restore as rolled
  {
    const { ex, desk } = await deskOn(21);
    await desk.reroll("bass");
    await desk.reroll("drums");
    await desk.reroll("lead");
    desk.lock(PART_ROW.lead, true);
    const snap = await desk.snapshot();
    const entry = { time: "t", snapshot: snap, position: { seconds: 0, bar: 0, section: null }, about: "part:bass", rating: "down", note: "too busy", changes: [] };
    check(entry.about === "part:bass" && snap.parts.bass.salt === snap.rows.bassLine && snap.parts.bass.history.length === 2 &&
          snap.parts.drums.history[1].kick === snap.rows.kick && snap.parts.lead.locked,
          "a part's feedback entry: about part:<name>, the snapshot holding every part's salt, history and lock");
    ex.wb_play(1);
    const live = render(ex, 8 * SR);
    const a = await replay(JSON.parse(JSON.stringify(entry.snapshot)), 8);
    {   // Final review 6: replay says when it renders with parts muted or soloed
      const quiet = { ...JSON.parse(JSON.stringify(entry.snapshot)), listen: { mute: ["bass"], solo: null } };
      const q = await replay(quiet, 1), heardAll = await replay({ ...quiet, listen: undefined }, 1);
      check(q.listen === "muted: bass" && heardAll.listen === "", `replay names the parts muted or soloed ("${q.listen}")`);
    }
    check(!a.mismatch.length && Buffer.compare(wav(a), wav(live)) === 0, `it replays to the samples heard (mismatch: ${a.mismatch.join(",") || "none"})`);
    const { ex: e2, desk: d2 } = await deskOn(5);
    await d2.restore(JSON.parse(JSON.stringify(snap)));
    check(JSON.stringify(d2.parts) === JSON.stringify(snap.parts) && e2.wb_ov_get(OV.BASS_SALT) === snap.rows.bassLine &&
          e2.wb_ov_get(OV.DRUMS_SALT) === snap.rows.drumsVar && d2.locks.has(PART_ROW.lead),
          "restore puts back every part's salt, history (at the roll heard) and lock");
    // a version-1 snapshot from before the parts: no parts, no listen, no part salt rows, an 8-entry override list
    const old = JSON.parse(JSON.stringify(snap));
    delete old.parts;
    delete old.listen;
    const s8 = JSON.parse(cstr(e2, e2.wb_state()));
    s8.ov = s8.ov.slice(0, 8);
    s8.ov[OV.PHRASE_SALT] = 0;
    s8.ov[OV.VOICING_SALT] = 0;
    old.rows = { ...fromEngine(s8, C, OPTS), family: old.rows.family };
    for (const id of ["bassLine", "padNotes", "drumsVar"]) delete old.rows[id];
    old.locks = [];
    const miss = await d2.restore(old);
    const p = d2.parts;
    check(!miss.length && names.every((q) => p[q].salt === 0 && p[q].history.length === 1 && !p[q].locked) &&
          [OV.BASS_SALT, OV.PAD_SALT, OV.DRUMS_SALT, OV.PHRASE_SALT, OV.VOICING_SALT].every((k) => e2.wb_ov_get(k) === 0),
          `an old snapshot (no parts or listen, an 8-entry override list) restores every part as rolled (mismatch: ${miss.join(", ") || "none"})`);
  }
}

console.log("the lead styles row and the retired lead engines (lead styles, Task 6)");
{
  const ex = await load(), call = caller(ex);
  ex.wb_seed(21);
  ex.wb_apply();
  const desk = new Desk(call, C, OPTS, await call("wb_library"), { rnd: lcg(9) });
  await desk.read(true);
  const r = row("leadStyle"), os = optionsFor(r, desk.state, OPTS), all = optionsFor(r, desk.state, OPTS, { showRetired: true });
  const GENRES = ["Impressionism", "Romanticism", "'60s Jazz", "Modal Jazz", "Jazz-Hop", "Film Score", "Bossa Nova", "Ambient"];
  check(os[0].rolled && JSON.stringify(os.slice(1).map((o) => [o.value, o.name])) === JSON.stringify(GENRES.map((g, i) => [i + 2, g])) &&
        all.filter((o) => o.status === "retired").map((o) => o.value).join() === "0,1" && desk.values.leadStyle === -1,
        `the lead style row: as rolled, then the eight styles by genre (2 + LS_*); the phrase planner and the hybrid (0, 1) retired (${os.map((o) => o.name).join(", ")})`);
  check(["leadDouble", "arpRhythm", "arpPattern"].every((id) => !row(id)) && ["LEAD_DOUBLE", "ARP_RHY", "ARP_PAT"].every((k) => C.OV[k] === undefined) &&
        OPTS.arpRhythms === undefined && OPTS.arpPatterns === undefined && ex.wb_lead_hybrid === undefined,
        "no lead-doubling or arpeggio rows, overrides or options (the old engines are gone)");
  // an old snapshot naming a retired engine (the hybrid, the phrase planner) and the lead-doubling switch: it restores
  // with the lead style as rolled, said in the message, the switch ignored
  const leadOf = (e) => {
    const s = JSON.parse(cstr(e, e.wb_state()));
    return s.ov[OV.LEAD_STYLE];
  };
  for (const [v, name] of [[1, "Hybrid"], [0, "Phrase planner"]]) {
    const old = await desk.snapshot();
    old.rows.leadStyle = v;
    old.rows.leadDouble = 1;
    old.rows.arpPattern = 2;
    old.locks = [];
    const e2 = await load(), d2 = new Desk(caller(e2), C, OPTS, await caller(e2)("wb_library"));
    const miss = await d2.restore(old);
    check(!miss.length && d2.values.leadStyle === -1 && leadOf(e2) === -1 && new RegExp(`Lead style: ${name} is retired`).test(d2.message) &&
          !("leadDouble" in d2.values) && !("arpPattern" in d2.values),
          `a snapshot with the retired ${name.toLowerCase()} and the lead-doubling switch restores on a rolled style, the switch ignored (mismatch: ${miss.join(", ") || "none"}; "${d2.message}")`);
  }
  const { inactive } = await import("../rows.js");
  await desk.set("leadStyle", 1);   // picked live under "show retired": said as the restore says it
  const why = inactive(row("leadStyle"), desk.state, OPTS);
  check(why === "Lead style: Hybrid is retired; the song plays a rolled style instead." && ex.wb_ov_get(OV.LEAD_STYLE) === 1 &&
        !inactive(row("leadStyle"), { ...desk.state, values: { ...desk.values, leadStyle: 6 } }, OPTS),
        `a retired style picked live says it plays as rolled ("${why}"); a style says nothing`);
  await desk.set("leadStyle", -1);
  const fresh2 = await desk.snapshot();
  fresh2.rows.leadStyle = 2 + 4;    // Jazz-Hop
  fresh2.locks = [];
  const e3 = await load(), d3 = new Desk(caller(e3), C, OPTS, await caller(e3)("wb_library"));
  const miss3 = await d3.restore(fresh2);
  check(!miss3.length && d3.values.leadStyle === 6 && leadOf(e3) === 6 && d3.message === "",
        `a snapshot naming a style restores it, nothing said (mismatch: ${miss3.join(", ") || "none"}; "${d3.message}")`);
}

console.log("candidates: the strum row, Approve (patterns and edits, Task 4)");
{
  const { readEvents } = await import("../wb.js");
  const { approval, inactive } = await import("../rows.js");
  const ex = await load(), call = caller(ex), lib = await call("wb_library");
  check(lib.progs.length === GEN_STATUS.LIB_PROGS.length && lib.progs.every((p, i) => p.status === GEN_STATUS.LIB_PROGS[i]),
        `wb_library's progressions carry their status (patterns_gen.c's: ${lib.progs.filter((p) => p.status !== "approved").length} not approved)`);
  const rows = [["strumPat", "STRUM_PAT", "strumPats", "STRUM_PAT"]];   // (the comp rhythms: the Comping row's, below)
  ex.wb_seed(21);
  ex.wb_apply();
  const desk = new Desk(call, C, OPTS, lib, { rnd: lcg(4) });
  await desk.read(true);
  for (const [id, key, src, table] of rows) {
    const r = row(id), os = optionsFor(r, desk.state, OPTS, { lib, showRetired: true }), n = OPTS[src].length;
    check(r?.kind === "ov" && r.key === key && C.OV[key] !== undefined && os.length === n + 1 && os[0].rolled &&
          n === GEN_STATUS[table].length &&
          os.slice(1).every((o, i) => o.value === i && o.status === GEN_STATUS[table][i] && o.name === OPTS[src][i].name) &&
          desk.values[id] === -1 && ex.wb_ov_get(C.OV[key]) === -1,
          `a ${r?.label} row (ov ${key}): as rolled, then options.json's ${src} by table index (${os.length - 1}); a new song's is as rolled`);
  }
  // what a part plays with an override set (a probe instance's wb_events of the committed song)
  const partOf = async (seed, role, ov, keys = null) => {
    const play = await load();
    play.wb_seed(seed);
    if (keys !== null) play.wb_role_set(V.CHORD, keys);
    for (const [k, v] of ov) play.wb_ov_set(k, v);
    play.wb_apply();
    const pex = await load();
    new Uint8Array(pex.memory.buffer, pex.wb_commit_ptr(), pex.wb_stage_size())
      .set(new Uint8Array(play.memory.buffer, play.wb_commit_ptr(), play.wb_stage_size()));
    pex.wb_events(0);
    return { comp: play.wb_comp_applied(), ev: JSON.stringify(readEvents(pex).filter((e) => e.role === role).map((e) => [e.bar, e.step, e.note, e.vel])) };
  };
  const nylon = ins("Nylon guitar"), strum = 21, gtr = [[OV.COMP, 5]];   // the nylon strums (comping: Strum)
  const base = await partOf(strum, V.CHORD, gtr, nylon), forced = [], ns = OPTS.strumPats.length;
  for (let i = 0; i < ns; i++) forced.push((await partOf(strum, V.CHORD, [...gtr, [OV.STRUM_PAT, i]], nylon)).ev);
  check(base.comp === 5 && forced.filter((e) => e !== base.ev).length === ns - 1 && new Set(forced).size === ns,
        `a strummed song plays each of the ${ns} forced strum patterns: the rolled one as rolled, the others each its own`);
  const d2 = new Desk(call, C, OPTS, lib, { rnd: lcg(4) });
  await d2.set("seed", strum);
  await d2.set("keys", nylon);
  await d2.set("comp", 5);
  const other = forced[0] === base.ev ? 1 : 0;
  const t = await d2.set("strumPat", other);
  check(t.ok !== false && d2.values.strumPat === other && ex.wb_ov_get(OV.STRUM_PAT) === other && d2.strums === true &&
        !inactive(row("strumPat"), d2.state, OPTS), "the strum row forces the engine's pattern on a strummed song");
  await d2.set("comp", -1);
  await d2.set("keys", ins("Rhodes"));
  check(d2.strums === false && inactive(row("strumPat"), d2.state, OPTS), "on a song whose keys don't strum, the strum row is inactive");
  const s12 = JSON.parse(cstr(ex, ex.wb_state()));
  check(fromEngine({ ...s12, ov: s12.ov.slice(0, 12) }, C, OPTS).strumPat === -1, "an older engine's 12-entry ov list reads it as rolled");

  // the strum fill row (Sam, 2026-11-04: "a way to ... audition and tweak them"): Default (the song's draw), Step back,
  // Drive; the two forced fills differ in the fill bars only, and the default is one of them
  const fr = row("strumFill");
  check(fr && fr.kind === "ov" && fr.key === "STRUM_FILL" && ROWS.indexOf(fr) === ROWS.indexOf(row("strumPat")) + 1 &&
        JSON.stringify(optionsFor(fr, { values: {} }, OPTS).map((o) => [o.value, o.name])) ===
        '[[-1,"Default"],[0,"Step back"],[1,"Drive"]]' && ex.wb_ov_get(OV.STRUM_FILL) === -1,
        "a Strum fill row after the strum pattern row: Default, Step back, Drive; a new song's is the default");
  const fills = [];
  for (const f of [0, 1])
    fills.push(JSON.parse((await partOf(strum, V.CHORD, [...gtr, [OV.STRUM_FILL, f]], nylon)).ev));
  const diffBars = new Set([...fills[0], ...fills[1]].map((e) => JSON.stringify(e))
    .filter((e) => !fills[0].some((x) => JSON.stringify(x) === e) || !fills[1].some((x) => JSON.stringify(x) === e))
    .map((e) => JSON.parse(e)[0]));
  const both = fills.map((f) => JSON.stringify(f));
  check(diffBars.size > 0 && both.includes(base.ev) && both[0] !== both[1],
        `the forced fills differ in ${diffBars.size} bar(s) (${[...diffBars].join(", ")}), the default plays one of ` +
        "them");
  await d2.set("keys", nylon);
  await d2.set("comp", 5);
  const tf = await d2.set("strumFill", 1);
  check(tf.ok !== false && ex.wb_ov_get(OV.STRUM_FILL) === 1 && !inactive(fr, d2.state, OPTS) &&
        JSON.parse(cstr(ex, ex.wb_state())).ov[OV.STRUM_FILL] === 1,
        "the strum fill row forces the drive on a strummed song");
  await d2.set("strumFill", -1);
  await d2.set("comp", -1);
  await d2.set("keys", ins("Rhodes"));
  check(inactive(fr, d2.state, OPTS) && fromEngine({ ...s12, ov: s12.ov.slice(0, 15) }, C, OPTS).strumFill === -1,
        "the strum fill row is inactive where the keys don't strum; a 15-entry ov list reads it as the default");

  // a candidate: offered (marked), never the dice's pick; approval names it for POST /approve
  // (a new entry past whatever options.json and the library hold, candidates already imported among them)
  const SN = OPTS.strumPats.length, PID = Math.max(...OPTS.patterns.map((p) => p.id)) + 1;
  const GID = Math.max(...lib.progs.map((p) => p.id)) + 1, KID = "k-new";
  const cand = { ...OPTS, strumPats: [...OPTS.strumPats, { id: "s-new", index: SN, name: "sam-strum", status: "candidate" }],
                 patterns: [...OPTS.patterns, { id: PID, name: "sam-groove", status: "candidate" }],
                 kickPhrases: [...OPTS.kickPhrases, { id: KID, a: [0], b: [0], status: "candidate" }] };
  const clib = { ...lib, progs: [...lib.progs, { id: GID, src: "imported from patterns/progressions/sam-vamp", status: "candidate" }] };
  const st = (values) => ({ values: { ...desk.values, ...values }, locks: new Set() });
  check(optionsFor(row("strumPat"), st({}), cand).at(-1).status === "candidate" &&
        optionsFor(row("prog"), st({}), cand, { lib: clib }).at(-1).status === "candidate", "a candidate is offered, marked");
  const okStrum = OPTS.strumPats.filter((p) => p.status === "approved").map((p) => p.index);
  const picks = new Set(Array.from({ length: 200 }, (_, i) => pickFor(row("strumPat"), st({ strumPat: okStrum[i % okStrum.length] }), cand, lcg(i))));
  check([...picks].every((v) => okStrum.includes(v)), `the dice pick approved strum patterns only (picked ${[...picks].sort().join(", ")})`);
  const ap = (id, v) => approval(row(id), st({ [id]: v }), cand, clib);
  const first = (xs) => xs.find((x) => x.status === "approved");
  check(JSON.stringify([ap("strumPat", SN), ap("pattern", PID), ap("prog", GID), ap("kick", KID)]) === JSON.stringify([
          { family: "strum", id: "s-new", name: "sam-strum" }, { family: "drums", id: String(PID), name: "sam-groove" },
          { family: "progressions", id: String(GID), name: `#${GID} sam-vamp` }, { family: "kick-phrases", id: KID, name: KID }]) &&
        [ap("strumPat", okStrum[0]), ap("strumPat", -1), ap("pattern", first(OPTS.patterns).id), ap("prog", first(lib.progs).id),
         ap("kick", ""), ap("mood", 1)].every((x) => x === null),
        "approval names a candidate (strum, drum pattern, progression, kick phrase), nothing else");
  {   // a candidate comp rhythm (patterns/comp-rhythms): in the Comping row, marked; Approve names it for POST /approve
    const CI = OPTS.compPats.length, cv = COMP_PAT_BASE + CI;
    const cc = { ...OPTS, compPats: [...OPTS.compPats, { id: "c-new", index: CI, name: "12-sam-comp", status: "candidate" }] };
    const sc = { values: { ...desk.values, keys: ins("Rhodes"), comp: cv }, locks: new Set() };
    const o = optionsFor(row("comp"), sc, cc).find((x) => x.value === cv);
    const held2 = OPTS.comping.find((c) => c.sym === "CS_HELD2").id;
    check(o?.status === "candidate" && o.name === "Sam comp" &&
          JSON.stringify(approval(row("comp"), sc, cc)) === '{"family":"comp-rhythms","id":"c-new","name":"Sam comp"}' &&
          approval(row("comp"), { ...sc, values: { ...sc.values, comp: COMP_PAT_BASE } }, cc) === null &&
          approval(row("comp"), { ...sc, values: { ...sc.values, comp: held2 } }, cc) === null,
          "a candidate comp rhythm is offered in the Comping row, marked, and Approve names it (comp-rhythms); an " +
          "approved one or a fixed style, nothing");
    const sentC = [];
    const r = await desk.approve(approval(row("comp"), sc, cc), async (url, init) => { sentC.push(init.body);
      return { ok: true, status: 200, json: async () => ({ ok: true }) }; });
    check(r.ok && desk.approved.has("comp-rhythms/c-new") && sentC[0] === '{"family":"comp-rhythms","id":"c-new"}',
          "Approve on a comp rhythm posts { family: comp-rhythms, id } to /approve (patterns/status.json, as strum)");
  }
  const sent = [];
  const fake = (ok) => async (url, init) => { sent.push([url, init.method, init.headers["Content-Type"], init.body]);
    return { ok, status: ok ? 200 : 400, json: async () => (ok ? { ok: true } : { ok: false, error: "no such strum pattern" }) }; };
  const res = await desk.approve(ap("strumPat", SN), fake(true));
  check(res.ok && desk.approved.has("strum/s-new") && /import\.sh/.test(res.message) &&
        JSON.stringify(sent[0]) === JSON.stringify(["/approve", "POST", "application/json", '{"family":"strum","id":"s-new"}']),
        "Approve posts { family, id } as JSON to /approve, and says to run tools/patterns/import.sh");
  const bad = await desk.approve(ap("pattern", PID), fake(false));
  check(!bad.ok && !desk.approved.has(`drums/${PID}`) && /no such strum pattern/.test(bad.message), "a refused approval says why");
}

console.log("comping: held on 2, held on 2 and 4 (approved, Sam 2026-11-05), light eighths (a candidate: the roll weighs it 0)");
{
  const HH = readFileSync(new URL("../../app/song/harmony.h", import.meta.url), "utf8");
  const CS = HH.slice(HH.indexOf("enum { CS_HELD")).split("}")[0].replace(/enum \{/, "").split(",")
    .map((x) => x.trim()).filter((x) => x && x !== "CS_N");
  check(OPTS.comping.every((c) => CS[c.id] === c.sym) && OPTS.comping.length === CS.length,
        "every comping style's id is its harmony.h enum's");
  const perc = ["Rhodes", "Felt piano", "Knight piano"].map(ins);   // (the Wurli and the DX too, to 2026-11-04)
  const off = ["Nylon guitar", "Emilyguitar", "Organ", "Chillwave pad"].map(ins);
  const NEW = ["Held on 2", "Held on 2 and 4", "Light eighths"];
  const offered = (k) => optionsFor(row("comp"), { values: { keys: k } }, OPTS).map((o) => o.name);
  check(perc.every((k) => NEW.every((n) => offered(k).includes(n))) && off.every((k) => NEW.every((n) => !offered(k).includes(n))),
        "the three are offered on the percussive keys only (not the guitars, organ or pad)");
  // the dice: approved choices only (held on 2, held on 2 and 4, the approved comp rhythms), never a candidate (light
  // eighths) nor CS_PAT alone (no entry of its own: the rhythms are offered by name)
  const cand = OPTS.comping.filter((c) => c.status === "candidate").map((c) => c.id);
  const pat = OPTS.comping.find((c) => c.sym === "CS_PAT").id;
  const picks = new Map();
  for (let i = 1; i <= 300; i++) {
    const v = pickFor(row("comp"), { values: { keys: perc[i % 3], comp: -1 }, locks: new Set() }, OPTS, lcg(i));
    picks.set(v, (picks.get(v) ?? 0) + 1);
  }
  const bad = [...picks.keys()].filter((v) => cand.includes(v) || v === pat);
  check(JSON.stringify(cand) === "[7]" && !bad.length && picks.has(6) && picks.has(8) &&
        OPTS.compPats.every((p) => picks.has(COMP_PAT_BASE + p.index) === (p.status === "approved")),
        `... light eighths a candidate, never the dice's pick; held on 2, held on 2 and 4 and each approved comp rhythm ` +
        `are (${[...picks].map(([v, n]) => `${v}: ${n}`).join(", ")})`);

  const { readEvents } = await import("../wb.js");
  const ex = await load(), call = caller(ex), lib = await call("wb_library");
  const keysEv = async () => {   // the committed song's keys notes, from a probe instance
    const pex = await load();
    new Uint8Array(pex.memory.buffer, pex.wb_commit_ptr(), pex.wb_stage_size())
      .set(new Uint8Array(ex.memory.buffer, ex.wb_commit_ptr(), ex.wb_stage_size()));
    pex.wb_events(0);
    return readEvents(pex).filter((e) => e.role === V.CHORD);
  };
  const d = new Desk(call, C, OPTS, lib, { rnd: lcg(8) });
  await d.set("seed", 21);
  await d.set("keys", ins("Rhodes"));
  const at = (es, s) => es.filter((e) => e.step === s).length;
  const bars = (es) => new Set(es.map((e) => e.bar)).size;
  const h2 = await d.set("comp", 6), e2 = await keysEv();
  check(h2.ok && ex.wb_comp_applied() === 6 && ex.wb_ov_get(OV.COMP) === 6 && e2.length && e2.every((e) => e.step === 4),
        `held on 2 picked on the Rhodes: struck on beat 2 only (${e2.length} notes)`);
  const h4 = await d.set("comp", 8), e4 = await keysEv();
  check(h4.ok && ex.wb_comp_applied() === 8 && e4.every((e) => e.step === 4 || e.step === 12) &&
        at(e4, 12) === at(e4, 4) && at(e4, 12) === 4 * bars(e4),
        `held on 2 and 4 picked on the Rhodes: beats 2 and 4 in every bar (${at(e4, 4)} and ${at(e4, 12)} notes)`);
  const l8 = await d.set("comp", 7), e8 = await keysEv();
  check(l8.ok && ex.wb_comp_applied() === 7 && e8.length && e8.every((e) => e.step % 2 === 0 && e.len <= 1) &&
        [0, 2, 4, 6, 8, 10, 12, 14].every((s) => at(e8, s) === 4 * bars(e8)),
        `light eighths picked on the Rhodes: every 8th of every bar, a 16th long (${e8.length} notes)`);
  for (const cs of [6, 7, 8]) {   // a busy lead changes none of them, the thinning row either way
    await d.set("comp", cs);
    const on = JSON.stringify(await keysEv());
    await d.set("busyThin", 1);
    const offEv = JSON.stringify(await keysEv());
    await d.set("busyThin", 0);
    check(on === offEv, `comping ${cs}: the same notes with the busy-lead thinning on or off`);
  }
  await d.set("keys", ins("Organ"));
  check(d.values.comp === -1 && ex.wb_ov_get(OV.COMP) === -1, "keys -> organ puts light eighths back to as rolled");
  for (const [keys, cs] of [["Organ", 7], ["Organ", 6], ["Organ", 8], ["Nylon guitar", 6], ["Nylon guitar", 8]]) {
    await d.set("keys", ins(keys));
    const st = cstr(ex, ex.wb_state()), r = await d.set("comp", cs);
    check(!r.ok && new RegExp(`doesn't suit the ${keys}`).test(r.message) && /Rhodes/.test(r.message) &&
          cstr(ex, ex.wb_state()) === st && ex.wb_ov_get(OV.COMP) === -1,
          `${NEW[[6, 8, 7].indexOf(cs)]} on the ${keys}: refused, nothing staged ("${r.message}")`);
  }
  ex.wb_ov_set(OV.COMP, 8);   // the engine on its own (a snapshot, a host render): held where the keys can't play it
  check(ex.wb_comp_applied() === 5, "forced on the nylon by the override alone, the engine strums instead");
  await d.set("keys", ins("Organ"));
  ex.wb_ov_set(OV.COMP, 8);
  check(ex.wb_comp_applied() === 0, "forced on the organ by the override alone, the engine holds instead");
  ex.wb_ov_set(OV.COMP, -1);
}

console.log("comp rhythms: in the Comping row by name, one place to pick comping (Sam, 2026-11-05)");
{
  const { readEvents } = await import("../wb.js");
  const { inactive } = await import("../rows.js");
  const ex = await load(), call = caller(ex), lib = await call("wb_library");
  const keysEv = async () => {   // the committed song's keys notes, from a probe instance
    const pex = await load();
    new Uint8Array(pex.memory.buffer, pex.wb_commit_ptr(), pex.wb_stage_size())
      .set(new Uint8Array(ex.memory.buffer, ex.wb_commit_ptr(), ex.wb_stage_size()));
    pex.wb_events(0);
    return JSON.stringify(readEvents(pex).filter((e) => e.role === V.CHORD).map((e) => [e.bar, e.step, e.note]));
  };
  check(!row("compPat") && !OPTS.comping.some((c) => /row below/.test(c.name)),
        "no Comp rhythm row, and no \"Comp rhythm (the row below)\" entry");
  const d = new Desk(call, C, OPTS, lib, { rnd: lcg(8) });
  await d.set("seed", 21);
  await d.set("keys", ins("Rhodes"));
  const names = optionsFor(row("comp"), d.state, OPTS).filter((o) => !o.rolled).map((o) => o.name);
  const pname = (p) => { const x = p.name.replace(/^\d+-/, "").replace(/-/g, " "); return x[0].toUpperCase() + x.slice(1); };
  const want = ["Held", "Half notes", "Held on 2", "Light eighths", "Held on 2 and 4", ...OPTS.compPats.map(pname)];
  check(JSON.stringify(names) === JSON.stringify(want) && names.includes("Charleston") && names.includes("Late one"),
        `the Comping row on the Rhodes lists every choice, the comp rhythms by name (${names.join(", ")})`);
  const vib = optionsFor(row("comp"), { values: { keys: ins("Vibes (modeled vibraphone)") } }, OPTS).filter((o) => !o.rolled)
    .map((o) => o.name);
  check(JSON.stringify(vib) === JSON.stringify(["Held", "Half notes", "Held on 2", "Held on 2 and 4", ...OPTS.compPats.map(pname)]),
        `the vibes on the keys are offered the comp rhythms too (Sam, 2026-11-05: ${vib.join(", ")})`);
  const n = OPTS.compPats.length, heard = new Set();
  check(d.values.comp === -1 && ex.wb_ov_get(OV.COMP_PAT) === -1 && ex.wb_ov_get(OV.COMP) === -1,
        "a new song's Comping is as rolled, no comp rhythm forced");
  let played = 0;
  for (const p of OPTS.compPats) {
    const r = await d.set("comp", COMP_PAT_BASE + p.index), es = await keysEv();
    played += r.ok && d.values.comp === COMP_PAT_BASE + p.index && ex.wb_comp_applied() === 9 &&
              ex.wb_ov_get(OV.COMP_PAT) === p.index && ex.wb_ov_get(OV.COMP) === -1 && es !== "[]";
    heard.add(es);
  }
  check(n > 0 && played === n && heard.size === n,
        `each of the ${n} comp rhythms picked in Comping plays (CS_PAT, the comp_pat override), each its own`);
  await d.set("comp", 6);
  check(ex.wb_ov_get(OV.COMP_PAT) === -1 && ex.wb_ov_get(OV.COMP) === 6 && ex.wb_comp_applied() === 6,
        "picking a style after a comp rhythm clears the comp rhythm");
  await d.set("comp", COMP_PAT_BASE);
  await d.set("comp", -1);
  check(ex.wb_ov_get(OV.COMP_PAT) === -1 && ex.wb_ov_get(OV.COMP) === -1, "as rolled clears both");
  await d.set("comp", COMP_PAT_BASE + 1);
  const ko = await d.set("keys", ins("Organ"));
  check(ko.ok && d.values.comp === -1 && ex.wb_ov_get(OV.COMP_PAT) === -1 && ex.wb_comp_applied() !== 9,
        "keys -> organ puts a comp rhythm back to as rolled (the percussive keys only), as the other styles");
  const rr = await d.set("comp", COMP_PAT_BASE + 1);
  check(!rr.ok && /doesn't suit the Organ/.test(rr.message) && /Rhodes/.test(rr.message) && ex.wb_ov_get(OV.COMP_PAT) === -1 &&
        !optionsFor(row("comp"), d.state, OPTS).some((o) => o.value >= COMP_PAT_BASE),
        `a comp rhythm on the organ: not offered, refused ("${rr.message}")`);
  // a snapshot (or the engine alone) with a rhythm on keys that don't play one: kept, the row greyed
  await d.set("keys", ins("Nylon guitar"));
  ex.wb_ov_set(OV.COMP_PAT, 0);
  await d.read();
  check(d.values.comp === COMP_PAT_BASE && ex.wb_comp_applied() === 5 && inactive(row("comp"), d.state, OPTS) &&
        optionsFor(row("comp"), d.state, OPTS).some((o) => o.value === COMP_PAT_BASE && o.current),
        "a comp rhythm left on the nylon (the engine's): it strums, the Comping row names it, greyed");
  ex.wb_ov_set(OV.COMP_PAT, -1);
  await d.read();
  check(!inactive(row("comp"), d.state, OPTS), "... and not greyed otherwise");

  // an old snapshot's Comp rhythm row (to 2026-11-05) restores as that rhythm in the Comping row
  const d2 = new Desk(call, C, OPTS, lib, { rnd: lcg(8) });
  await d2.set("seed", 21);
  await d2.set("keys", ins("Rhodes"));
  const snap = await d2.snapshot();
  const old = { ...snap, rows: { ...snap.rows, comp: -1, compPat: 2 } };
  const miss = await d2.restore(old);
  check(d2.values.comp === COMP_PAT_BASE + 2 && ex.wb_ov_get(OV.COMP_PAT) === 2 && ex.wb_comp_applied() === 9 &&
        !miss.length, `an old snapshot's comp rhythm 2 restores as ${OPTS.compPats[2].name} in Comping (mismatch: ${miss.join(", ") || "none"})`);
  const miss2 = await d2.restore({ ...snap, rows: { ...snap.rows, comp: 6, compPat: -1 } });
  check(d2.values.comp === 6 && ex.wb_ov_get(OV.COMP_PAT) === -1 && !miss2.length,
        "an old snapshot with no comp rhythm keeps its Comping style");
  const now = await d2.snapshot();
  check(!("compPat" in now.rows) && now.rows.comp === 6, "a snapshot holds the Comping row's value alone");
  await d2.set("comp", COMP_PAT_BASE + 1);
  const s1 = await d2.snapshot();
  await d2.set("comp", -1);
  const miss3 = await d2.restore(s1);
  check(d2.values.comp === COMP_PAT_BASE + 1 && ex.wb_ov_get(OV.COMP_PAT) === 1 && !miss3.length,
        "a new snapshot with a comp rhythm restores it");
}

console.log("the busy-lead thinning row (on by default; off, the existing comping keeps its rhythm)");
{
  const { readEvents } = await import("../wb.js");
  const { replay, wav } = await import("./replay.mjs");
  const ex = await load(), call = caller(ex);
  ex.wb_seed(21);
  ex.wb_apply();
  const desk = new Desk(call, C, OPTS, await call("wb_library"), { rnd: lcg(9) });
  await desk.read(true);
  const r = row("busyThin"), names = optionsFor(r, desk.state, OPTS).map((o) => o.name);
  check(r && r.kind === "ov" && r.key === "NO_BUSY_THIN" && r.label === "Busy lead thins the comping" &&
        JSON.stringify(names) === JSON.stringify(["On", "Off"]) && desk.values.busyThin === 0 && ex.wb_ov_get(OV.NO_BUSY_THIN) === 0,
        `a "Busy lead thins the comping" row: on, off (${names.join(", ")}); a new song's is on`);
  // a half-note song's keys with the row on or off (a probe instance's wb_events of the committed song)
  const keysOf = async (seed, noThin) => {
    const play = await load();
    play.wb_seed(seed);
    play.wb_role_set(V.CHORD, ins("Rhodes"));
    play.wb_ov_set(OV.COMP, 4);
    play.wb_ov_set(OV.NO_BUSY_THIN, noThin);
    play.wb_apply();
    const pex = await load();
    new Uint8Array(pex.memory.buffer, pex.wb_commit_ptr(), pex.wb_stage_size())
      .set(new Uint8Array(play.memory.buffer, play.wb_commit_ptr(), play.wb_stage_size()));
    pex.wb_events(0);
    return readEvents(pex).filter((e) => e.role === V.CHORD);
  };
  let seed = 0, on = null, offEv = null;
  for (let s = 1; s <= 40 && !seed; s++) {
    on = await keysOf(s, 0); offEv = await keysOf(s, 1);
    if (JSON.stringify(on) !== JSON.stringify(offEv)) seed = s;
  }
  const b3 = (es) => es.filter((e) => e.step === 8).length;
  check(seed > 0 && b3(offEv) > b3(on), `off, a busy-lead half-note song strikes beat 3 again (seed ${seed}: ${b3(on)} -> ${b3(offEv)} notes)`);
  const t = await desk.set("busyThin", 1);
  check(t.ok !== false && desk.values.busyThin === 1 && ex.wb_ov_get(OV.NO_BUSY_THIN) === 1, "the row turns it off");
  // it rides in snapshots and replays; an older one (no such row) restores it on
  await desk.set("seed", seed);
  await desk.set("keys", ins("Rhodes"));
  await desk.set("comp", 4);
  const snap = await desk.snapshot();
  const e2 = await load(), d2 = new Desk(caller(e2), C, OPTS, await caller(e2)("wb_library"));
  const miss = await d2.restore(snap);
  check(snap.rows.busyThin === 1 && !miss.length && d2.values.busyThin === 1 && e2.wb_ov_get(OV.NO_BUSY_THIN) === 1,
        `a snapshot keeps it off (mismatch: ${miss.join(", ") || "none"})`);
  const old = JSON.parse(JSON.stringify(snap));
  delete old.rows.busyThin;
  old.locks = [];
  const e3 = await load(), d3 = new Desk(caller(e3), C, OPTS, await caller(e3)("wb_library"));
  const miss3 = await d3.restore(old);
  check(!miss3.length && d3.values.busyThin === 0 && e3.wb_ov_get(OV.NO_BUSY_THIN) === 0,
        `an older snapshot (no such row) restores it on (mismatch: ${miss3.join(", ") || "none"})`);
  const secs = 60;
  const a = await replay(JSON.parse(JSON.stringify(snap)), secs), b = await replay(JSON.parse(JSON.stringify(old)), secs);
  check(!a.mismatch.length && !b.mismatch.length && Buffer.compare(wav(a), wav(b)) !== 0,
        `a replay plays it: off and on render differently (mismatch: ${[...a.mismatch, ...b.mismatch].join(", ") || "none"})`);
  const s16 = JSON.parse(cstr(ex, ex.wb_state()));
  check(fromEngine({ ...s16, ov: s16.ov.slice(0, 13) }, C, OPTS).busyThin === 0, "a 13-entry ov list reads it on");
  await desk.set("busyThin", 0);
  check(desk.values.busyThin === 0 && ex.wb_ov_get(OV.NO_BUSY_THIN) === 0, "and back on");
}

console.log("the choir: a pad layer a song may draw (1 in 4), and a choice on the pad row");
{
  const { replay, wav } = await import("./replay.mjs");
  const CHOIR = ins("Choir"), WARM = ins("Warm pad"), STR = ins("Strings"), ORGAN = ins("Organ");
  const PERC = ["Rhodes", "Felt piano", "Knight piano"].map(ins);
  const ex = await load(), call = caller(ex);
  ex.wb_seed(1);
  ex.wb_apply();
  const desk = new Desk(call, C, OPTS, await call("wb_library"), { rnd: lcg(12) });
  await desk.read(true);
  // the pad row: offered, approved, the dice's pick now and then (either family: it fits both)
  const o = optionsFor(row("pad"), desk.state, OPTS).find((x) => x.value === CHOIR);
  check(o && o.name === "Choir" && o.status === "approved", "the pad row offers the choir, approved");
  let picked = 0;
  for (let i = 0; i < 200; i++)
    picked += pickFor(row("pad"), { values: { pad: i & 2 ? WARM : STR, family: i & 1 ? "synth" : "acoustic" }, locks: new Set() },
                      OPTS, lcg(i)) === CHOIR;
  check(picked > 0, `the dice pick it (${picked} of 200)`);
  // the songs' draw: the row reads the pad each song plays (the set's, or the choir or the organ drawn over it)
  let choir = 0, cs = 0, other = 0, organ = 0;
  const odd = [];
  for (let s = 1; s <= 200; s++) {
    await desk.set("seed", s);
    const p = desk.values.pad;
    if (p !== ex.wb_role_get(V.PAD) || ![CHOIR, WARM, STR, ORGAN].includes(p)) odd.push(`${s}:${p}`);
    // (other: a song on percussive keys, so the pad's chord part sounds: over the organ it doesn't)
    if (p === CHOIR) { choir++; cs ||= s; } else if (p === ORGAN) organ++;
    else if (!other && PERC.includes(desk.values.keys)) other = s;
  }
  check(organ >= 15 && organ <= 45, `the organ is ${organ} of 200 songs' pad (about 1 in 7 drawn)`);
  check(!odd.length && choir >= 30 && choir <= 70,
        `the choir is ${choir} of 200 songs' pad (1 in 4 drawn); the row reads the engine's (off: ${odd.join(" ") || "none"})`);
  // a choir song keeps it through another row's edit (the custom set takes the song's pad), and the row moves it
  await desk.set("seed", cs);
  const bass = optionsFor(row("bass"), desk.state, OPTS).find((x) => x.status === "approved" && x.value !== desk.values.bass);
  await desk.set("bass", bass.value);
  check(desk.values.pad === CHOIR && ex.wb_role_get(V.PAD) === CHOIR, `seed ${cs}: a bass change keeps the choir pad`);
  const w = await desk.set("pad", WARM);
  check(w.ok !== false && desk.values.pad === WARM && ex.wb_role_get(V.PAD) === WARM, "the pad row puts the warm pad in its place");
  // a song that didn't draw it takes it from the row; snapshots and replays keep it
  await desk.set("seed", other);
  await desk.set("padOn", 1);
  const p = await desk.set("pad", CHOIR);
  check(p.ok !== false && desk.values.pad === CHOIR && ex.wb_role_get(V.PAD) === CHOIR, `seed ${other}: picked, the engine's pad is the choir`);
  const snap = await desk.snapshot();
  const e2 = await load(), d2 = new Desk(caller(e2), C, OPTS, await caller(e2)("wb_library"));
  const miss = await d2.restore(snap);
  check(snap.rows.pad === CHOIR && !miss.length && d2.values.pad === CHOIR && e2.wb_role_get(V.PAD) === CHOIR,
        `a snapshot keeps the choir pad (mismatch: ${miss.join(", ") || "none"})`);
  const old = JSON.parse(JSON.stringify(snap));
  old.rows.pad = WARM;
  old.locks = [];
  const a = await replay(JSON.parse(JSON.stringify(snap)), 60), b = await replay(old, 60);
  check(!a.mismatch.length && !b.mismatch.length && Buffer.compare(wav(a), wav(b)) !== 0,
        `a replay plays it: the choir and the warm pad render differently (mismatch: ${[...a.mismatch, ...b.mismatch].join(", ") || "none"})`);
  check(!ROWS.some((r) => r.id === "soloChoir") && OV.SOLO_CHOIR === undefined, "no vox pad solo row (the choir sings the solo notes)");
}

console.log("lead loop end to end (Jazz-Hop, the Lead loop row On)");
{
  const { readEvents } = await import("../wb.js");
  const JAZZHOP = OPTS.leadStyles.find((s) => s.name === "Jazz-Hop").id;
  // a 48-bar song, Jazz-Hop, the loop on, committed; its events from the top on a probe instance (wb_events)
  const song = async (seed) => {
    const ex = await load();
    ex.wb_seed(seed);
    ex.wb_roll_set(RF.TEMPO, 90);
    ex.wb_roll_set(RF.BARS, 48);   // (after the tempo: it sets the length)
    ex.wb_ov_set(OV.LEAD_STYLE, JAZZHOP);
    ex.wb_ov_set(OV.LEAD_LOOP, 1);
    ex.wb_apply();
    const pex = await load();
    new Uint8Array(pex.memory.buffer, pex.wb_commit_ptr(), pex.wb_stage_size())
      .set(new Uint8Array(ex.memory.buffer, ex.wb_commit_ptr(), ex.wb_stage_size()));
    pex.wb_events(0);
    return { ex, evs: readEvents(pex) };
  };
  const leadIn = (evs, a, b) => evs.some((e) => e.role === V.LEAD && e.bar >= a && e.bar < b);
  let seed = 1, s = await song(seed);   // the first seed whose lead plays at bar 30 and at bar 16
  while (seed < 40 && !(leadIn(s.evs, 30, 34) && leadIn(s.evs, 16, 20))) s = await song(++seed);
  check(leadIn(s.evs, 30, 34) && leadIn(s.evs, 16, 20), `seed ${seed}: the lead plays at bars 16 and 30`);
  const { ex, evs } = s;
  ex.wb_play(1);
  until(ex, () => ex.wb_bar() === 41, 300 * SR);
  check(ex.wb_lead_loop_stat(0) > 0, `after 40 bars the loop has replayed ${ex.wb_lead_loop_stat(0)} bars`);
  // a jump: from its landing to the next bar line the song plays wb_events' events from the bar it landed on (the
  // heard hash restarts at a landing); the lead's among them
  const window = (to) => {
    ex.wb_jump(to);
    until(ex, () => ex.wb_bar() === to + 1, 60 * SR);
    until(ex, () => ex.wb_bar() === to + 5, 60 * SR);
    const n = ex.wb_heard_count(), want = evs.filter((e) => e.bar >= to).slice(0, n);
    check(n > 0 && (ex.wb_heard_hash() >>> 0) === evHash(want) && want.some((e) => e.role === V.LEAD),
          `a jump to bar ${to}: its ${n} events (${want.filter((e) => e.role === V.LEAD).length} the lead's) as from the top`);
  };
  window(30);
  window(16);
}

console.log("options.json: statuses");
check(!OPTS.instruments.some((i) => i.status === "candidate") &&
      ["Chillwave pad", "Choir"].every((n) => OPTS.instruments.find((i) => i.name === n).status === "approved") &&
      OPTS.instruments.find((i) => i.name === "Vox pad").status === "retired",
      "no candidates among the instruments (the chillwave pad and the choir approved, the vox pad retired)");
{
  const { inactive } = await import("../rows.js");
  const st = (kit) => ({ values: { kit } });
  check(inactive(row("ride"), st(3), OPTS) && !inactive(row("ride"), st(4), OPTS), "the ride row is inactive on a kit with no ride (Brushes), not on a sampled kit");
}

console.log(failed ? `smoke: ${failed} FAILED` : "smoke: all passed");
process.exit(failed ? 1 : 0);

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


// ---- the decision rows (rows.js, options.json) and the desk (desk.js) over the engine ----
const { ROWS, row, optionsFor, pickFor, conflicts, familyOf } = await import("../rows.js");
const { Desk } = await import("../desk.js");
const OPTS = JSON.parse(readFileSync(new URL("../options.json", import.meta.url), "utf8"));
const HDR = readFileSync(new URL("../../app/song/library.h", import.meta.url), "utf8");
const enumOf = (first) => {   // the enum whose first name is `first`: its names in order
  const body = HDR.slice(HDR.indexOf(`enum { ${first}`)).split("}")[0];
  return body.replace(/\/\*[\s\S]*?\*\//g, "").replace(/enum \{/, "").split(",").map((x) => x.trim()).filter(Boolean);
};
const lcg = (s) => () => ((s = (s * 1103515245 + 12345) >>> 0) / 4294967296);
const STR = new Set(["wb_consts", "wb_state", "wb_live_state", "wb_library"]);
// the page's call (the worklet's): JSON exports parsed, wb_stage_bytes a copy of the staged state
const caller = (ex) => async (fn, ...a) => {
  if (fn === "wb_stage_bytes") return new Uint8Array(ex.memory.buffer, ex.wb_stage_ptr(), ex.wb_stage_size()).slice();
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
const famOf = (id) => OPTS.instruments.find((i) => i.id === id)?.family ?? OPTS.kits.find((k) => k.id === id)?.family;

console.log("options.json");
{
  const INS = enumOf("INS_RHODES"), KIT = enumOf("KIT_DUSTY");
  check(OPTS.instruments.every((i) => INS[i.id] === i.sym) && OPTS.instruments.length === INS.length,
        "every instrument's id is its library.h enum's");
  check(OPTS.kits.every((k) => KIT[k.id] === k.sym) && OPTS.kits.length === KIT.length - 1, "every kit's id is its enum's");
  const retired = (xs, names) => names.every((n) => xs.find((x) => x.name.startsWith(n))?.status === "retired");
  check(retired(OPTS.instruments, ["Upright bass"]) && retired(OPTS.comping, ["Arpeggio", "Offbeats"]) &&
        retired(OPTS.kits, ["Dusty", "Soft", "Our 808", "Warm", "Dark", "Rusty", "Gentle", "Clean"]),
        "the removed sounds are retired (upright, our synth kits, the mixed sampled kits, arpeggio, offbeats)");
}

// an acoustic song as the rows see it (the upright and the mixed kit hand-picked with "show retired")
const acoustic = () => ({ values: { keys: ins("Knight piano"), lead: ins("Flute"), bass: ins("Upright bass"),
  pad: ins("Strings"), candy: ins("Music box"), kit: 3, comp: -1, family: "acoustic", pattern: 1, kick: "" }, locks: new Set() });

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
  for (const fam of ["acoustic", "synth"]) {
    const s = acoustic();
    s.values.family = fam;
    if (fam === "synth") Object.assign(s.values, { keys: ins("DX electric piano"), lead: ins("Saw lead"), bass: ins("FM bass"),
                                                   pad: ins("Warm pad"), candy: ins("Bells") });
    for (const id of ["keys", "lead", "bass", "pad", "candy", "kit", "comp", "pattern", "kick", "family", "leadOct"])
      for (let i = 0; i < 40; i++) {
        const v = pickFor(row(id), s, OPTS, rnd);
        if (v === null) continue;
        const f = ["keys", "lead", "bass", "pad", "candy", "kit"].includes(id) ? famOf(v) : null;
        if (String(v) === String(s.values[id]) || (f && f !== fam && f !== "electric" && f !== "any")) bad.push(`${fam} ${id}=${v}`);
        const o = optionsFor(row(id), s, OPTS).find((x) => String(x.value) === String(v));
        if (id !== "family" && (o?.status ?? "approved") !== "approved") bad.push(`${fam} ${id}=${v} not approved`);
      }
  }
  check(!bad.length, `pickFor never returns the current value, another family's or an unapproved one (${bad.slice(0, 4).join(", ") || "none"})`);

  const c = conflicts(row("family"), "synth", acoustic(), OPTS, lcg(3));
  const to = Object.fromEntries(c.forced.map((f) => [f.row, f.to]));
  check(["bass", "pad", "candy"].every((id) => famOf(to[id]) === "synth") && !c.blocked,
        `family -> synth re-derives bass, pad and ear candy to synth (${["bass", "pad", "candy"].map((id) => `${id} ${to[id]}`).join(", ")})`);
  check(c.stuck.some((s) => s.row === "kit"), "the kit stays, reported: no approved synth kit yet (Tasks 8-9 add them)");
  const withKit = { ...OPTS, kits: [...OPTS.kits, { id: 99, name: "test synth kit", family: "synth", status: "approved" }] };
  const ck = conflicts(row("family"), "synth", acoustic(), withKit, lcg(3));
  check(ck.forced.some((f) => f.row === "kit" && f.to === 99), "with an approved synth kit, the kit is forced to it");

  const locked = acoustic();
  locked.locks = new Set(["bass"]);
  const b = conflicts(row("family"), "synth", locked, OPTS, lcg(3));
  check(b.blocked?.label === "Bass", `Review Focus 3: the bass locked to the upright blocks family -> synth (${b.blocked?.label})`);

  const st2 = acoustic();
  st2.values.comp = 1;   // stabs, on the Knight
  const k = conflicts(row("keys"), ins("Rhodes"), st2, OPTS);
  check(k.forced.some((f) => f.row === "comp" && f.to === -1), "keys -> Rhodes forces stab comping back to as rolled");
  st2.locks = new Set(["comp"]);
  check(conflicts(row("keys"), ins("Rhodes"), st2, OPTS).blocked?.label === "Comping", "... and a locked stab comping blocks it");
  check(!optionsFor(row("comp"), { values: { keys: ins("Rhodes") } }, OPTS).some((o) => o.name === "Strum") &&
        optionsFor(row("comp"), { values: { keys: ins("Nylon guitar") } }, OPTS).some((o) => o.name === "Strum"),
        "strum is offered on the guitars only");
  const kl = conflicts(row("lead"), ins("Knight piano"), acoustic(), OPTS, lcg(5));
  check(kl.forced.some((f) => f.row === "keys" && f.to !== ins("Knight piano")), "the Knight on the lead moves the keys off it");
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

  // Review Focus 3, end to end: the bass hand-set to the upright and locked; family -> synth is blocked, nothing staged
  await desk.set("family", "acoustic");
  await desk.set("bass", ins("Upright bass"));
  desk.lock("bass", true);
  const before = cstr(ex, ex.wb_state());
  const res = await desk.set("family", "synth");
  check(!res.ok && /Bass/.test(res.message) && cstr(ex, ex.wb_state()) === before && desk.values.family === "acoustic",
        `Review Focus 3: family -> synth with the bass locked: blocked, nothing changed ("${res.message}")`);
  desk.lock("bass", false);
  const ok = await desk.set("family", "synth");
  check(ok.ok && famOf(desk.values.bass) === "synth" && ok.moved.includes("bass"), `unlocked, the bass follows and is highlighted (${ok.moved.join(", ")})`);

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
  await desk.set("keys", ins("Wurli"));
  await desk.set("lead", ins("Sax"));
  desk.lock("lead", true);
  await desk.set("kick", "k5");
  await desk.set("kit", 3);
  desk.lock("kit", true);
  await desk.set("comp", 4);
  await desk.set("phrase", 777);
  desk.lock("phrase", true);
  const keep = { tempo, keys: ins("Wurli"), lead: ins("Sax"), kick: "k5", kit: 3, comp: 4, phrase: 777 };
  const kept = () => Object.entries(keep).filter(([k, v]) => String(desk.values[k]) !== String(v)).map(([k]) => k);
  const seed = desk.values.seed;
  const r1 = await desk.dice("seed");
  check(r1.ok && desk.values.seed !== seed && !kept().length, `a new seed (${desk.values.seed}) keeps them (lost: ${kept().join(", ") || "none"})`);
  const st = await call("wb_state");
  check(st.roll[C.RF.TEMPO] === tempo && st.roles[C.V.LEAD] === ins("Sax") && st.kit === 3 && st.kick[1] === 0x481 &&
        st.ov[C.OV.COMP] === 4 && st.ov[C.OV.PHRASE_SALT] === 777, "and the engine has them staged");
  const map = desk.values.map, other = optionsFor(row("map"), desk.state, OPTS, { lib: await call("wb_library") })
    .find((o) => o.value !== map).value;
  const r2 = await desk.set("map", other);
  check(r2.ok && desk.values.map === other && !kept().length, `a new map keeps them too (lost: ${kept().join(", ") || "none"})`);
  until(ex, () => ex.wb_bar() === 2, 60 * SR);
  check(ex.wb_roll_get(C.RF.MAP) === other && db(render(ex, 3 * SR)) > -45, "the new song plays from bar 1 with them");
  const r3 = await desk.rerollAll();
  check(r3.ok && desk.values.lead === ins("Sax") && desk.values.kit === 3 && desk.values.phrase === 777,
        "re-roll all unlocked keeps the locked rows");
}

console.log("desk: dice and registers");
{
  const ex = await load();
  const call = caller(ex);
  const desk = new Desk(call, C, OPTS, await call("wb_library"), { rnd: lcg(5), probe: await prober() });
  ex.wb_seed(5);
  ex.wb_apply();
  ex.wb_play(1);
  render(ex, Q);
  await desk.read();
  const salts = [];
  for (let i = 0; i < 3; i++) { await desk.dice("voicings"); salts.push(desk.values.voicings); }
  check(salts.every((s) => s > 0) && new Set(salts).size === 3, `the voicings' dice set new nonzero salts (${salts.join(", ")})`);
  const t = desk.values.tempo, r = await desk.dice("tempo");
  check(r.ok && desk.values.tempo !== t && ex.wb_roll_get(C.RF.TEMPO) === desk.values.tempo, `the tempo's dice is the engine's (${t} -> ${desk.values.tempo})`);
  const k = await desk.set("keysOct", 1);   // seed 5: the DX keys can't go up under its lead (harmony_keys_oct)
  check(k.ok && desk.values.keysOct === ex.wb_keys_oct_applied() && ex.wb_ov_get(C.OV.KEYS_OCT) === desk.values.keysOct,
        `the keys register shows what the engine applies (asked +1, plays ${desk.values.keysOct}${k.message ? `: "${k.message}"` : ""})`);
  const probe = await prober(), stagedLead = async () => probe(await call("wb_stage_bytes"));
  const l = await desk.set("leadOct", -1);   // seed 5: the lead can't go down (lead.c keeps it)
  check(l.ok && desk.values.leadOct === 0 && ex.wb_ov_get(C.OV.LEAD_OCT) === 0 && /Lead register/.test(l.message),
        `the lead register shows what the engine applies (asked -1, plays ${desk.values.leadOct}: "${l.message}")`);
  ex.wb_ov_set(C.OV.LEAD_OCT, -1);
  const refused = await stagedLead();
  ex.wb_seed(21);
  ex.wb_ov_set(C.OV.LEAD_OCT, 1);
  const applied = await stagedLead();
  check(refused === 0 && applied === 1, `the probe instance: seed 5 down refused (${refused}), seed 21 up applied (${applied})`);
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
  await desk.set("keys", ins("Wurli"));
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

console.log("options.json: statuses");
check(!OPTS.instruments.some((i) => i.status === "candidate") &&
      ["Chillwave pad", "Vox pad"].every((n) => OPTS.instruments.find((i) => i.name === n).status === "approved"),
      "no candidates among the instruments (the chillwave and vox pads approved)");
{
  const { inactive } = await import("../rows.js");
  const st = (kit) => ({ values: { kit } });
  check(inactive(row("ride"), st(3), OPTS) && !inactive(row("ride"), st(4), OPTS), "the ride row is inactive on a kit with no ride (Brushes), not on a sampled kit");
}

console.log(failed ? `smoke: ${failed} FAILED` : "smoke: all passed");
process.exit(failed ? 1 : 0);

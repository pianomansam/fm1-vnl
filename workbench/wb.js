// SPDX-License-Identifier: GPL-3.0-or-later
// The page's side of the engine: starts the AudioContext and the worklet (wb-worklet.js) on the first play, and
// calls wb.wasm's exports through it. Every change is staged, then wb_apply: it lands at the next bar line.

let ctx = null, node = null, nextId = 1;
const waiting = new Map();
const listeners = new Set();
export let C = null;   // the engine's indices (wb_consts): C.RF.TEMPO, C.OV.COMP, C.V.LEAD, C.LIB_SET_WB

export async function start() {
  if (node) return;
  ctx = new AudioContext({ sampleRate: 44100 });
  await ctx.audioWorklet.addModule("wb-worklet.js");
  node = new AudioWorkletNode(ctx, "workbench", { outputChannelCount: [2] });
  node.connect(ctx.destination);
  const wasm = await (await fetch("wb.wasm")).arrayBuffer();
  await new Promise((ready, fail) => {
    node.port.onmessage = (e) => {
      const m = e.data;
      if (m.type === "ready") { C = m.consts; ready(); }
      else if (m.type === "failed") fail(new Error(m.error));
      else if (m.type === "result") { waiting.get(m.id)?.(m); waiting.delete(m.id); }
      else if (m.type === "pos") listeners.forEach((f) => f(m));
    };
    node.port.postMessage({ type: "load", wasm }, [wasm]);
  });
}

// an export's result (a Promise; wb_consts, wb_state and wb_library come back parsed); calls sent together run between
// the same two quanta
export function call(fn, ...args) {
  const id = nextId++;
  return new Promise((ok, fail) => {
    waiting.set(id, (m) => (m.error ? fail(new Error(m.error)) : ok(m.value)));
    node.port.postMessage({ type: "call", fn, args, id });
  });
}

export function onPosition(f) { listeners.add(f); }

// A second wb.wasm on the main thread, never played: it runs the checks that need scratch songs (the lead register a
// staged state gets, the committed song's events), so the audio thread never does.
let probeEx = null;
async function probeInstance() {
  if (!probeEx) {
    let mem;
    const wasi = { fd_write: (fd, iov, n, out) => { new DataView(mem.buffer).setUint32(out, 0, true); return 0; } };
    const stub = new Proxy(wasi, { get: (t, k) => (k in t ? t[k] : () => 0) });
    const env = new Proxy({}, { get: () => () => 0 });
    const { instance } = await WebAssembly.instantiate(await (await fetch("wb.wasm")).arrayBuffer(),
                                                       { wasi_snapshot_preview1: stub, env });
    probeEx = instance.exports;
    mem = probeEx.memory;
    if (probeEx._initialize) probeEx._initialize();
    probeEx.wb_init();
  }
  return probeEx;
}

// the lead register the engine applies to a staged state; bytes: the playing instance's wb_stage_bytes
export async function probe(bytes) {
  const ex = await probeInstance();
  new Uint8Array(ex.memory.buffer, ex.wb_stage_ptr(), ex.wb_stage_size()).set(bytes);
  return ex.wb_lead_oct_probe();
}

// The committed song's events (every one from bar 1) and chord changes ({ events, chords }); bytes: the playing instance's wb_commit_bytes
export async function events(bytes) {
  const ex = await probeInstance();
  new Uint8Array(ex.memory.buffer, ex.wb_commit_ptr(), ex.wb_stage_size()).set(bytes);
  ex.wb_events(0);
  return { events: readEvents(ex), chords: readChords(ex) };
}

// wb_events' records out of an instance's memory: [{ bar (from 0), step, role (C.V; a drum's V_KICK..V_RIDE), note (a
// drum's lane), vel, len (16ths) }] in the song's order
export function readEvents(ex) {
  const n = ex.wb_events_count(), dv = new DataView(ex.memory.buffer, ex.wb_events_ptr(), 8 * n), out = new Array(n);
  for (let i = 0, o = 0; i < n; i++, o += 8)
    out[i] = { bar: dv.getUint16(o, true), step: dv.getUint8(o + 2), role: dv.getUint8(o + 3), note: dv.getUint8(o + 4),
               vel: dv.getUint8(o + 5), len: dv.getUint16(o + 6, true) };
  return out;
}

// wb_chords' records (the last wb_events run's chord changes): [{ bar (from 0), step, root (pitch class) }]
export function readChords(ex) {
  const n = ex.wb_chords_count(), dv = new DataView(ex.memory.buffer, ex.wb_chords_ptr(), 4 * n), out = new Array(n);
  for (let i = 0; i < n; i++) out[i] = { bar: dv.getUint16(4 * i, true), step: dv.getUint8(4 * i + 2), root: dv.getUint8(4 * i + 3) };
  return out;
}

// the AudioContext running (a click's: an audition while stopped)
export async function resume() {
  await start();
  await ctx.resume();
}

export async function play(on) {
  await start();
  if (on) await ctx.resume();
  return call("wb_play", on ? 1 : 0);
}

// Feedback: each entry is queued in localStorage and sent to the server (POST /feedback), oldest first; what the server
// can't take stays queued and goes again on the next flush. An entry the server refuses (a 4xx: it will never take it)
// moves to a dead-letter key so it doesn't block the ones behind it. Each has an id: the server skips one it has
// already written (a lost response). { saved, queued, dead }: saved when nothing is waiting; dead: entries refused.
const QKEY = "wb-feedback-queue", DKEY = "wb-feedback-dead";
const mem = { [QKEY]: [], [DKEY]: [] };   // the lists when localStorage can't be used
const readL = (s, k) => { try { return JSON.parse(s.getItem(k) || "[]"); } catch { return mem[k]; } };
const writeL = (s, k, q) => { mem[k] = q; try { s.setItem(k, JSON.stringify(q)); } catch { /* mem holds it */ } };
let counter = 0;

export function queued(storage = globalThis.localStorage) { return readL(storage, QKEY).length; }

let flushing = Promise.resolve();   // one flush at a time, so an entry is never sent twice at once
export function flushFeedback(opts = {}) { return (flushing = flushing.then(() => flushOnce(opts))); }

async function flushOnce({ fetch: f = globalThis.fetch, storage = globalThis.localStorage } = {}) {
  for (let q = readL(storage, QKEY); q.length; q = readL(storage, QKEY)) {
    let status;
    try {
      const r = await f("/feedback", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(q[0]) });
      status = r.ok ? 200 : r.status;
    } catch { break; }
    if (status !== 200 && !(status >= 400 && status < 500)) break;   // down or failing: try again later
    if (status !== 200) writeL(storage, DKEY, [...readL(storage, DKEY), q[0]]);
    writeL(storage, QKEY, readL(storage, QKEY).slice(1));
  }
  const n = readL(storage, QKEY).length;
  return { saved: n === 0, queued: n, dead: readL(storage, DKEY).length };
}

export function sendFeedback(entry, opts = {}) {
  const storage = opts.storage ?? globalThis.localStorage;
  const id = entry.id ?? `${entry.time}-${++counter}-${Math.random().toString(36).slice(2, 8)}`;
  writeL(storage, QKEY, [...readL(storage, QKEY), { ...entry, id }]);
  return flushFeedback(opts);
}


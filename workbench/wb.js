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
  await new Promise((ready) => {
    node.port.onmessage = (e) => {
      const m = e.data;
      if (m.type === "ready") { C = m.consts; ready(); }
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
// staged state gets), so the audio thread never does. bytes: the playing instance's wb_stage_bytes.
let probeEx = null;
export async function probe(bytes) {
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
  new Uint8Array(probeEx.memory.buffer, probeEx.wb_stage_ptr(), probeEx.wb_stage_size()).set(bytes);
  return probeEx.wb_lead_oct_probe();
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

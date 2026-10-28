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

// an export's result (a Promise); calls sent together run between the same two quanta
export function call(fn, ...args) {
  const id = nextId++;
  return new Promise((ok, fail) => {
    waiting.set(id, (m) => (m.error ? fail(new Error(m.error)) : ok(m.value)));
    node.port.postMessage({ type: "call", fn, args, id });
  });
}

export function onPosition(f) { listeners.add(f); }

export async function play(on) {
  await start();
  if (on) await ctx.resume();
  return call("wb_play", on ? 1 : 0);
}

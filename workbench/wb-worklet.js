// SPDX-License-Identifier: GPL-3.0-or-later
// The workbench's engine in an AudioWorklet: wb.wasm (workbench/engine/wb.c) renders each quantum; the page's calls
// to its wb_* exports arrive between quanta (so a group of stagings sent together lands as one), and the song's
// position goes back about ten times a second. Instantiated as base/web/emu/worklet.js does, without the device.

const STRINGS = new Set(["wb_consts", "wb_state", "wb_live_state", "wb_library", "wb_sections", "wb_screen", "wb_rolled"]);   // exports that return JSON (parsed here)

class Workbench extends AudioWorkletProcessor {
  constructor() {
    super();
    this.ex = null;
    this.lastPos = 0;
    this.port.onmessage = (e) => this.onMessage(e.data);
  }

  // a call that throws (a trap, a bad argument) still answers, with the error: the page's queue never waits forever
  async onMessage(m) {
    try {
      await this.handle(m);
    } catch (e) {
      this.port.postMessage(m.type === "call" ? { type: "result", id: m.id, error: String(e) } : { type: "failed", error: String(e) });
    }
  }

  async handle(m) {
    if (m.type === "load") {
      const wasi = {
        fd_write: (fd, iov, n, out) => { new DataView(this.ex.memory.buffer).setUint32(out, 0, true); return 0; },
        proc_exit: () => {},
      };
      const stub = new Proxy(wasi, { get: (t, k) => (k in t ? t[k] : () => 0) });
      const env = new Proxy({}, { get: () => () => 0 });
      const { instance } = await WebAssembly.instantiate(m.wasm, { wasi_snapshot_preview1: stub, env });
      this.ex = instance.exports;
      if (this.ex._initialize) this.ex._initialize();
      this.ex.wb_init();
      this.port.postMessage({ type: "ready", consts: JSON.parse(this.cstr(this.ex.wb_consts())) });
    } else if (m.type === "call") {   // { fn: "wb_...", args, id }: the result goes back with the id
      if (!this.ex) throw new Error("the engine isn't loaded yet");
      const ex = this.ex;
      if (m.fn === "wb_stage_bytes" || m.fn === "wb_commit_bytes") {   // the staged (committed) state's bytes, for
        const p = m.fn === "wb_stage_bytes" ? ex.wb_stage_ptr() : ex.wb_commit_ptr();   // wb.js's probe instance
        const value = new Uint8Array(ex.memory.buffer, p, ex.wb_stage_size()).slice();
        this.port.postMessage({ type: "result", id: m.id, value });
        return;
      }
      const f = /^wb_/.test(m.fn) ? ex[m.fn] : null;
      let value = f ? f(...(m.args || [])) : undefined;
      if (STRINGS.has(m.fn)) value = JSON.parse(this.cstr(value));
      this.port.postMessage({ type: "result", id: m.id, value, error: f ? undefined : `no export ${m.fn}` });
    }
  }

  cstr(p) {                                     // no TextDecoder in a worklet's scope
    const b = new Uint8Array(this.ex.memory.buffer, p);
    let s = "";
    for (let i = 0; b[i]; i++) s += String.fromCharCode(b[i]);
    return s;
  }

  process(inputs, outputs) {
    if (!this.ex) return true;
    const out = outputs[0], n = out[0].length, ex = this.ex;
    ex.wb_render(n);
    out[0].set(new Float32Array(ex.memory.buffer, ex.wb_out_l(), n));   // fresh views: the memory may have grown
    if (out[1]) out[1].set(new Float32Array(ex.memory.buffer, ex.wb_out_r(), n));
    if (currentTime - this.lastPos >= 0.1) {
      this.lastPos = currentTime;
      this.port.postMessage({ type: "pos", bar: ex.wb_bar(), bars: ex.wb_bars(), seconds: ex.wb_seconds(), gap: ex.wb_gap() });
    }
    return true;
  }
}

registerProcessor("workbench", Workbench);

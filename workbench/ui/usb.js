// SPDX-License-Identifier: GPL-3.0-or-later
// The FM-1 over USB as the device mock's controls (Web MIDI with SysEx; Chrome and Edge, not Safari). The FM-1 runs
// VNL-1 and goes silent while the browser has its controls; the browser plays. The protocol is app.c's ("the browser's
// remote"), prefix F0 7D 56 4E:
//   browser -> FM-1   01 remote on (and the keepalive, once a second)   02 remote off
//   FM-1 -> browser   11 ss 01   the state (ss 1 on, the answer to each 01; 0 off)
//                     20 bb ss   button bb (plat.h's B_*) down (1) or up (0)
//                     21 ee dd   encoder ee (SELECT ALGO PRESET K1..K4) turned dd detents (7-bit two's complement)
//                     22 kk ss   key kk (0..26, plat.h's: 0 = F3) down (1) or up (0): the mock's KEY0..KEY26
// The FM-1 lets go after 3 s without a 01 (a closed tab, the cable out) or at "remote off".
import { BUTTONS, ENCODERS, KNOBS } from "./device.js";

// the FM-1's USB-MIDI ports: any named "FM-1" (the device reports "AMB-1 FM-1", fm1-base's, not fm1.toml's
// usb_product); only VNL-1 answers the handshake, so another firmware just stays "waiting"
export const PORT_NAME = "FM-1";
export const PREFIX = [0xf0, 0x7d, 0x56, 0x4e];
export const ON = [...PREFIX, 0x01, 0xf7], OFF = [...PREFIX, 0x02, 0xf7];
const ENC = [...ENCODERS, ...KNOBS];       // plat.h's EN_* order
export const KEEPALIVE_MS = 1000, SILENT_MS = 3500;
const NKEYS = 27;                          // plat.h's keys, F3..G5

// a message from the FM-1: { ack: on } | { control, down } (a button or a key) | { control, detents } (an encoder) |
// null
export function decode(d) {
  if (d.length !== 8 || PREFIX.some((b, i) => d[i] !== b) || d[7] !== 0xf7) return null;
  const [op, a, b] = [d[4], d[5], d[6]];
  if (op === 0x11) return { ack: a === 1 };
  if (op === 0x20 && BUTTONS[a]) return { control: BUTTONS[a], down: b === 1 };
  if (op === 0x21 && ENC[a]) return { control: ENC[a], detents: b >= 64 ? b - 128 : b };
  if (op === 0x22 && a < NKEYS) return { control: `KEY${a}`, down: b === 1 };
  return null;
}

// Connect: find the FM-1's ports, take its controls (act(control, detents): the mock's own) and keep them;
// onState(state, text) each change. state: unsupported, denied, searching (not plugged in), waiting (asked, no
// answer yet), connected, off. Returns { disconnect() }. request: navigator.requestMIDIAccess by default;
// timers and now for the tests.
export async function connect({ act, key, onState = () => {}, request, timers = globalThis, now = () => Date.now() }) {
  let state = "", access, input = null, output = null, heard = 0, timer = null, closed = false;
  const set = (s, text) => { if (s !== state || text) { state = s; onState(s, text ?? TEXT[s]); } };
  request ??= typeof navigator !== "undefined" && navigator.requestMIDIAccess
    ? (o) => navigator.requestMIDIAccess(o) : null;
  if (!request) { set("unsupported"); return { disconnect() {} }; }
  try {
    access = await request({ sysex: true });
  } catch (e) {
    set("denied", `MIDI access was refused (${e.name ?? e}): allow MIDI with SysEx for this page in the browser's site settings.`);
    return { disconnect() {} };
  }
  const send = (m) => { try { output?.send(m); } catch { /* the port went away: statechange follows */ } };
  const onMessage = (e) => {
    const m = decode(e.data);
    if (!m) return;
    if ("ack" in m) {
      if (m.ack) { heard = now(); set("connected"); } else if (!closed) set("waiting");
    } else if (m.detents) act(m.control, m.detents);
    else if (key) key(m.control, m.down);   // a key or a button: down and up (a long press)
    else if (m.down) act(m.control, 1);   // (no key handler: a button acts as it goes down)
  };
  let lit = "";   // the lights last sent (03)
  const find = (ports) => [...ports.values()].find((p) => p.name?.includes(PORT_NAME) && p.state !== "disconnected") ?? null;
  const bind = () => {
    const i = find(access.inputs), o = find(access.outputs);
    if (i !== input) {
      if (input) input.onmidimessage = null;
      input = i;
      if (input) input.onmidimessage = onMessage;
    }
    if (o !== output) {
      output = o;
      lit = "";   // a new port: the lights again
      if (output) send(ON);
    }
    if (!input || !output) set("searching");
    else if (state !== "connected") set("waiting");
  };
  access.onstatechange = () => { if (!closed) bind(); };
  bind();
  timer = timers.setInterval(() => {
    if (!output) return;
    send(ON);
    if (state === "connected" && now() - heard > SILENT_MS) set("waiting");
  }, KEEPALIVE_MS);
  return {
    // the keys to light and to blink (Sets of key numbers), sent when they change (03; 7 bits a byte)
    lights(on, blink) {
      const pack = (s) => [0, 1, 2, 3].map((i) => [...s].reduce((m, k) => (k >= 7 * i && k < 7 * i + 7 ? m | (1 << (k - 7 * i)) : m), 0));
      const msg = [0xf0, 0x7d, 0x56, 0x4e, 0x03, ...pack(on), ...pack(blink), 0xf7], key = msg.join(",");
      if (key === lit || !output) return;
      lit = key;
      send(msg);
    },
    disconnect() {
      if (closed) return;
      closed = true;
      timers.clearInterval(timer);
      send(OFF);
      access.onstatechange = null;
      if (input) input.onmidimessage = null;
      input = output = null;
      set("off");
    },
  };
}

const TEXT = {
  unsupported: "Web MIDI isn't available in this browser (Safari has none): open the workbench in Chrome or Edge.",
  searching: "Looking for an FM-1: plug it in by USB (VNL-1 on it), and close other apps using its MIDI.",
  waiting: "Found the FM-1; waiting for it to answer (VNL-1 with the remote?).",
  connected: "FM-1 connected: its controls drive the mock, and it stays silent.",
  off: "FM-1 disconnected: it's stopped, ready for PLAY.",
};

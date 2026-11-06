// SPDX-License-Identifier: GPL-3.0-or-later
// The device mock's panel on the page: the FM-1's screen (240 x 240 at 2x, ui/device.js drawing ui/screens.js), its
// controls by printed label (clickable, and keys), what the screen reader says, and the flow between the screens with
// the breadcrumb. It plays the workbench's song: a knob's change goes through the rows' own calls (Desk.set), PLAY is
// the page's Play. Served from workbench/ui/ as it is on disk: edit screens.js, then "Reload screens" (or reload).
import { Lcd, Device, W, H, BUTTONS, ENCODERS, KNOBS, actionOf, transitions, problems } from "./device.js";
import { connect } from "./usb.js";

// the panel's keys (the page's, not the device's): a knob or encoder clockwise, with Shift counter-clockwise
const KEYS = { Digit1: "K1", Digit2: "K2", Digit3: "K3", Digit4: "K4", Digit5: "SELECT", Digit6: "ALGO", Digit7: "PRESET",
               KeyF: "FX", KeyS: "SEL", KeyN: "ENV", KeyL: "LFO", KeyE: "EDIT", KeyG: "GLO", KeyH: "HOME", KeyV: "SAVE",
               KeyA: "ARP", KeyQ: "SEQ", KeyP: "PLAY", KeyR: "REC", BracketLeft: "OCT-", BracketRight: "OCT+" };
const KEY_OF = Object.fromEntries(Object.entries(KEYS).map(([k, c]) => [c, k.replace(/^(Digit|Key)/, "")
  .replace("BracketLeft", "[").replace("BracketRight", "]")]));
// a rough FM-1 panel: the encoders, the knobs, then the buttons in two rows
const ROWS = [["FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME"], ["SAVE", "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+"]];

const CSS = `
  .dev { display: grid; grid-template-columns: auto minmax(0, 1fr); gap: 20px; margin-top: 10px; align-items: start; }
  @media (max-width: 860px) { .dev { grid-template-columns: 1fr; } }
  .dev canvas { width: ${2 * W}px; height: ${2 * H}px; max-width: 100%; image-rendering: pixelated; display: block;
                background: #000; border: 10px solid #111; border-radius: 8px; box-sizing: content-box; }
  .dev .say { font-size: 13px; color: var(--muted); margin: 6px 0 0; max-width: ${2 * W + 20}px; min-height: 2.8em; }
  .dev .say b { color: var(--fg); font-weight: 600; }
  .dev .ctl { display: flex; flex-wrap: wrap; gap: 6px; margin-top: 10px; max-width: ${2 * W + 20}px; }
  .dev .ctl button { font-size: 11px; font-weight: 600; padding: 4px 0; width: 64px; letter-spacing: 0.04em; }
  .dev .ctl button i, .dev .enc i { font-style: normal; font-weight: 400; color: var(--muted); margin-left: 4px; }
  .dev .ctl button.live { border-color: var(--accent); }
  .dev .keys27 { position: relative; height: 64px; margin-top: 12px; max-width: ${2 * W + 20}px; }
  .dev .keys27 button { position: absolute; top: 0; padding: 0; border-radius: 0 0 4px 4px; font-size: 9px; }
  .dev .keys27 button.w { height: 64px; background: #f4f2ec; color: #555; border: 1px solid #999; z-index: 1;
                          display: flex; align-items: flex-end; justify-content: center; padding-bottom: 3px; }
  .dev .keys27 button.b { height: 38px; background: #222; color: #ddd; border: 1px solid #000; z-index: 2; }
  .dev .keys27 button.live { box-shadow: inset 0 -4px 0 var(--accent); }
  .dev .keys27 button.lit { background: #ffb846; }
  .dev .keys27 button.b.lit { background: #c98a1e; }
  .dev .keys27 button.blink { animation: keyblink 0.25s steps(1) infinite; }
  @keyframes keyblink { 50% { background: #ffb846; } }
  .dev .encs { display: flex; gap: 10px; margin-top: 10px; flex-wrap: wrap; }
  .dev .enc { display: flex; flex-direction: column; align-items: center; gap: 3px; font-size: 11px; width: 78px; }
  .dev .enc .dial { width: 34px; height: 34px; border-radius: 50%; border: 2px solid var(--line); background: var(--panel);
                    cursor: ns-resize; }
  .dev .enc.live .dial { border-color: var(--accent); }
  .dev .enc .what { color: var(--muted); font-size: 10px; min-height: 1.2em; }
  .dev .enc span.b { display: flex; gap: 2px; }
  .dev .enc span.b button { padding: 0 6px; font-size: 11px; }
  .dev .crumb { font-size: 13px; margin: 0 0 8px; }
  .dev .crumb b { color: var(--accent); }
  .dev .flow { display: flex; flex-wrap: wrap; gap: 10px; }
  .dev .card { border: 1px solid var(--line); border-radius: 6px; padding: 8px 10px; min-width: 12em; background: var(--panel); }
  .dev .card.now { border-color: var(--accent); box-shadow: 0 0 0 1px var(--accent); }
  .dev .card h3 { font-size: 13px; margin: 0 0 4px; }
  .dev .card .ph { font-size: 10px; font-weight: 600; color: #fff; background: var(--muted); border-radius: 3px;
                   padding: 1px 4px; margin-left: 6px; vertical-align: 1px; }
  .dev .card ul { list-style: none; margin: 0; padding: 0; font-size: 12px; }
  .dev .card li { padding: 1px 0; }
  .dev .card li button { padding: 0 4px; font-size: 11px; font-weight: 600; margin-right: 4px; }
  .dev .card li.took { color: var(--accent); font-weight: 600; }
  .dev .card .keys { font-size: 11px; color: var(--muted); margin-top: 4px; }
  .dev .bad { color: var(--accent); font-size: 13px; white-space: pre-wrap; }
  .dev .tools { display: flex; gap: 8px; margin-top: 10px; flex-wrap: wrap; }
  .dev .tools button { font-size: 12px; padding: 3px 10px; }
  .dev .usb { font-size: 12px; color: var(--muted); margin: 6px 0 0; }
`;

const el = (tag, props = {}, ...kids) => { const e = Object.assign(document.createElement(tag), props); e.append(...kids); return e; };

// box: the section to fill; call: wb.call; C: wb.C; engine: { set(row, value), play() }
export function mountDevice(box, { call, C, engine, labels = () => ({}), rows: rowsOf = () => ({}), activity = () => null, levels = () => ({}) }) {
  document.head.append(el("style", { textContent: CSS }));
  const canvas = el("canvas", { width: W, height: H, role: "img", ariaLabel: "The device's screen (mock)" });
  const ctx = canvas.getContext("2d"), img = ctx.createImageData(W, H), lcd = new Lcd();
  const say = el("p", { className: "say", ariaLive: "polite" });
  const crumb = el("nav", { className: "crumb", ariaLabel: "Breadcrumb" }), flow = el("div", { className: "flow" });
  const bad = el("p", { className: "bad" });
  const ctl = new Map();
  let dev = null, shownId = null, took = null, pos = { bar: 0, bars: 0, gap: 0 }, playing = false, key = "", fetching = false, again = false;

  const act = (control, detents = 1) => {
    if (!dev) return;
    took = dev.press(control, detents) ?? took;
    draw();
    panel();
  };
  const down = new Set();   // keys held (a long press: Device.tick)
  const keyAct = (control, isDown) => {
    if (!dev || isDown === down.has(control)) return;
    if (isDown) down.add(control); else down.delete(control);
    took = (isDown ? dev.keyDown(control) : dev.keyUp(control)) ?? took;
    draw();
    panel();
  };
  const knob = (id) => {
    const what = el("span", { className: "what" }), dial = el("div", { className: "dial", title: `${id}: scroll, or the arrows` });
    dial.onwheel = (e) => { e.preventDefault(); act(id, e.deltaY < 0 ? 1 : -1); };
    const b = el("span", { className: "b" }, el("button", { textContent: "◀", title: `${id} counter-clockwise (Shift+${KEY_OF[id]})`,
                                                             onclick: () => act(id, -1) }),
                 el("button", { textContent: "▶", title: `${id} clockwise (${KEY_OF[id]})`, onclick: () => act(id, 1) }));
    const e = el("div", { className: "enc" }, dial, el("span", {}, id, el("i", { textContent: KEY_OF[id] })), b, what);
    ctl.set(id, Object.assign(e, { what }));
    return e;
  };
  const encs = el("div", { className: "encs" }, ...ENCODERS.map(knob)), knobs = el("div", { className: "encs" }, ...KNOBS.map(knob));
  const rows = ROWS.map((r) => el("div", { className: "ctl" }, ...r.map((id) => {
    const b = el("button", { onclick: () => act(id) }, id, el("i", { textContent: KEY_OF[id] }));
    ctl.set(id, b);
    return b;
  })));
  const tools = el("div", { className: "tools" },
                   el("button", { textContent: "Reload screens", title: "Read ui/screens.js again (the song plays on)", onclick: () => load() }),
                   el("button", { textContent: "Speak screen", title: "What a screen reader hears asking for the whole screen", onclick: () => { dev?.speak(); draw(); } }));
  // the real FM-1's controls over USB (ui/usb.js): the mock acts on them; the FM-1 stays silent meanwhile
  const usbState = el("p", { className: "usb", ariaLive: "polite" });
  let usb = null, usbIs = "";
  const usbBtn = el("button", { textContent: "Connect FM-1", title: "Use the FM-1's buttons, encoders and knobs on this mock (Web MIDI)",
    onclick: async () => {
      if (usb) { usb.disconnect(); usb = null; usbBtn.textContent = "Connect FM-1"; return; }
      usbBtn.disabled = true;
      usb = await connect({ act, key: keyAct, onState: (s, text) => { usbIs = s; usbState.textContent = text; } });
      usbBtn.disabled = false;
      if (usbIs === "unsupported" || usbIs === "denied") usb = null;   // nothing to disconnect
      usbBtn.textContent = usb ? "Disconnect FM-1" : "Connect FM-1";
    } });
  tools.append(usbBtn);
  addEventListener("pagehide", () => usb?.disconnect());
  // the 27 keys, F3 up (plat.h's keys 0..26): KEY0..KEY26; a screen maps them like any control (the song map's
  // white keys jump, Sam 2026-11-06). The OP1-OP5 black keys are printed on the FM-1's lower black keys.
  const BLACK = new Set([1, 3, 5, 8, 10]), white = [], keyRow = el("div", { className: "keys27" });
  for (let k = 0; k < 27; k++) if (!BLACK.has(((k % 12) + 12) % 12)) white.push(k);
  const kw = Math.floor((2 * W + 20) / white.length);
  for (let k = 0; k < 27; k++) {
    const id = `KEY${k}`, isB = BLACK.has(k % 12), wi = white.filter((w) => w < k).length;
    const b = el("button", { className: isB ? "b" : "w", title: `key ${k}`,
                             onpointerdown: (e) => { e.preventDefault(); keyAct(id, true); }, onpointerup: () => keyAct(id, false),
                             onpointerleave: () => keyAct(id, false),
                             textContent: isB ? "" : "FGABCDE"[wi % 7] });
    Object.assign(b.style, isB ? { left: `${wi * kw - kw * 0.3}px`, width: `${kw * 0.6}px` } : { left: `${wi * kw}px`, width: `${kw}px` });
    ctl.set(id, b);
    keyRow.append(b);
  }
  box.append(el("div", { className: "dev" },
                el("div", {}, canvas, say, encs, knobs, ...rows, keyRow),
                el("div", {}, crumb, flow, bad, tools, usbState)));

  // ui/screens.js, fresh from disk (a new URL each time, past the module cache); a mistake shows here and the last
  // good screens stay
  async function load() {
    try {
      const data = (await import(`./screens.js?t=${Date.now()}`)).default, bad_ = problems(data);
      bad.textContent = bad_.length ? `screens.js:\n${bad_.join("\n")}` : "";
      if (bad_.length && dev) return;
      if (dev) dev.load(data); else dev = new Device(data, engine);
      took = null;
      panel();
      refresh(true);
    } catch (e) {
      bad.textContent = `screens.js didn't load: ${e.message}`;
    }
  }

  // the screen and what the reader said
  let lightsKey = "";
  function draw() {
    if (!dev) return;
    const L = dev.keyLights(), lk = [...L.on].join() + "|" + [...L.blink].join();   // the keys' lights, here and on the FM-1
    if (lk !== lightsKey) {
      lightsKey = lk;
      for (let k = 0; k < 27; k++) { const b = ctl.get(`KEY${k}`); b?.classList.toggle("lit", L.on.has(k)); b?.classList.toggle("blink", L.blink.has(k)); }
    }
    usb?.lights?.(L.on, L.blink);
    if (dev.id !== shownId) { lcd.fill(0); shownId = dev.id; }   // a new screen: cleared first, as an app would (lcd_fill)
    dev.draw(lcd);
    ctx.putImageData(new ImageData(lcd.rgba(img.data), W, H), 0, 0);
    say.replaceChildren(el("b", { textContent: "Screen reader: " }), dev.said || "–");
  }

  // the panel around it: the breadcrumb, what each control does here, the flow between the screens
  function panel() {
    if (!dev) return;
    const d = dev.data;
    crumb.replaceChildren("Screens: ", ...dev.stack.flatMap((id, i) => [i ? " › " : "", i === dev.stack.length - 1
      ? el("b", { textContent: d.screens[id].title ?? id }) : d.screens[id].title ?? id]));
    // what control c does on screen id, in a few words ("": nothing; HOME on the home screen is nothing)
    const does = (id, c) => {
      const a = actionOf(d, id, c);
      return !a || (a.go === "home" && id === d.home) ? "" : a.go ? `→ ${a.go === "back" || a.go === "home" ? a.go : a.go === "cursor" ? "the highlighted part" : d.screens[a.go]?.title ?? a.go}`
        : a.do === "play" ? "play / stop" : a.do === "cursor" ? "move the highlight" : a.do === "part" ? "next part"
        : a.do === "page" ? "next page" : a.do === "jump" ? (a.i !== undefined ? `go to section ${a.i + 1}` : "go to the highlighted section")
        : a.do === "reroll" ? (a.part ? "re-roll this part" : "new song") : `${d.fields[a.field]?.name ?? a.field}${a.do === "nudge" ? ` ${a.by > 0 ? "+" : ""}${a.by}` : ""}`;
    };
    for (const [id, e] of ctl) {   // what each control does here
      const what = does(dev.id, id);
      e.classList.toggle("live", !!what);
      e.title = what ? `${id}: ${what}` : `${id}: nothing on this screen`;
      if (e.what) e.what.textContent = what;
    }
    const tr = transitions(d);
    flow.replaceChildren(...Object.entries(d.screens).map(([id, s]) => el("div", { className: `card${id === dev.id ? " now" : ""}` },
      el("h3", { title: "Show this screen (a workbench shortcut, not a device move)", style: "cursor: pointer",
                 onclick: () => { dev.stack = id === d.home ? [d.home] : [d.home, id]; took = null; panel(); draw(); } }, s.title ?? id, ...(s.placeholder ? [el("span", { className: "ph", textContent: "PLACEHOLDER" })] : [])),
      el("ul", {}, ...tr.filter((t) => t.from === id).map((t) => el("li", {
        className: took && took.from === id && took.control === t.control ? "took" : "" },
        el("button", { textContent: t.control, title: id === dev.id ? `Press ${t.control}` : `On ${s.title ?? id}`,
                       disabled: id !== dev.id, onclick: () => act(t.control) }),
        `→ ${t.to === "back" || t.to === "home" ? `${t.to} (${t.to === "home" ? d.screens[d.home].title ?? d.home : "the one before"})`
                                                : d.screens[t.to]?.title ?? t.to}`))),
      el("div", { className: "keys", textContent: `Knobs: ${(s.knobs ?? []).map((k, i) => `K${i + 1} ${k.label ?? d.fields[k.field]?.name}`).join(", ") || "none"}` }),
      el("div", { className: "keys", textContent: `Also: ${[...BUTTONS, ...ENCODERS].filter((c) => !actionOf(d, id, c)?.go && does(id, c))
        .map((c) => `${c} ${does(id, c)}`).join(", ") || "nothing"}` }))));
  }

  // what the engine plays, read when the bar, the transport or the song changes (force: after a change)
  async function refresh(force = false) {
    const k = `${pos.bar}/${pos.gap}/${playing}`;
    if (!dev || (!force && k === key)) return draw();
    if (fetching) { again = true; return; }
    key = k;
    fetching = true;
    try {
      const s = await call("wb_live_state"), facts = pos.bar > 0 ? await call("wb_screen", pos.bar - 1) : {};
      const rolled = await call("wb_rolled").catch(() => null);   // what the "as rolled" rows play
      const sections = await call("wb_sections", 1).catch(() => []);   // the song's map, as it plays
      const r = (f) => s.roll[C.RF[f]];
      dev.update({ playing, gap: pos.gap, bar: pos.bar, bars: pos.bars || r("BARS"), seed: s.seed, facts, labels: labels(rolled), rows: rowsOf(rolled, s), sections, activity: activity(), levels: levels(),
                   roll: { key: r("KEY"), tempo: r("TEMPO"), motion: r("MOTION"), mood: r("MOOD"), mode: r("MODE") } });
    } finally {
      fetching = false;
      if (again) { again = false; key = ""; refresh(); }
    }
    draw();
  }

  addEventListener("keydown", (e) => {
    const c = KEYS[e.code];
    if (!c || box.hidden || e.ctrlKey || e.metaKey || e.altKey || /^(INPUT|SELECT|TEXTAREA)$/.test(e.target.tagName)) return;
    e.preventDefault();
    act(c, e.shiftKey ? -1 : 1);
  });
  setInterval(() => { dev?.tick(); draw(); }, 100);   // a message's 1.3 s runs out; a long press; the playhead moves
  load();
  return {
    position(p, on) { pos = p; playing = on; refresh(); },   // each position the engine reports, and the transport
    changed() { refresh(true); },                            // after a change on the page
    redraw() { refresh(true); },                             // (the loop switch)
  };
}

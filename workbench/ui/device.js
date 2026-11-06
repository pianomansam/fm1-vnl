// SPDX-License-Identifier: GPL-3.0-or-later
// The device mock's core, no DOM (Node runs it too: tools/screen_check.mjs, smoke.mjs): the FM-1's LCD as gfx.c draws
// it, today's screen's text as app.c writes it, and the screens of ui/screens.js driven by the panel's controls.
import { FONTS } from "./fonts.js";
import { ROWS } from "../rows.js";

// ------------------------------------------------------------------------------------------------ the LCD (gfx.c) ---
export const W = 240, H = 240;
const CV_MAX = 240 * 144;   // gfx.c's APP_CANVAS_PX default: a band is at most this many pixels (cv_begin cuts its height)
export const RGB = (r, g, b) => ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
const NEUTRALS = [RGB(34, 34, 40), RGB(74, 74, 82), RGB(150, 150, 158)];
export const PALETTES = {
  GREEN: [...NEUTRALS, RGB(80, 220, 120), RGB(150, 255, 175)],
  AMBER: [...NEUTRALS, RGB(232, 136, 30), RGB(255, 184, 70)],
  CYAN: [...NEUTRALS, RGB(60, 180, 235), RGB(150, 225, 255)],
  RED: [...NEUTRALS, RGB(232, 76, 60), RGB(255, 132, 112)],
  MONO: [...NEUTRALS, RGB(196, 196, 202), RGB(236, 236, 240)],
};
export const THEME = "AMBER";   // fm1b_config.h's APP_PALETTE 1 (VNL-1 doesn't set its own)
// the theme's colors, plus the screen data's own (`colors`: { NAME: [r, g, b] }, e.g. a color per part)
export const colors = (theme = THEME, extra = {}) => {
  const p = PALETTES[theme];
  return { BLACK: 0x0000, WHITE: 0xffff, LINE: p[0], DIM: p[1], GRAY: p[2], AMB: p[3], HI: p[4],
           ...Object.fromEntries(Object.entries(extra).map(([k, [r, g, b]]) => [k, RGB(r, g, b)])) };
};

const decoded = new Map();
export function font(name) {   // a face of ui/fonts.js, its packed glyphs as bytes
  if (!decoded.has(name)) {
    const f = FONTS[name];
    if (!f) throw new Error(`no font ${name}`);
    decoded.set(name, { ...f, bytes: Uint8Array.from(f.data.match(/../g) ?? [], (h) => parseInt(h, 16)) });
  }
  return decoded.get(name);
}

const utf8 = (s) => new TextEncoder().encode(s);   // the device draws a string's bytes
function glyph(f, ch) {   // gfx.c's glyph(): lower case folds when the face has none; anything missing draws as '?'
  if (ch >= 97 && ch <= 122 && f.last < 97) ch -= 32;
  if (ch < f.first || ch > f.last || (ch >= 127 && ch < 160)) ch = 63;
  return ch - f.first;
}

export function textW(f, s) {
  let w = 0;
  for (const ch of utf8(s)) w += f.adv[glyph(f, ch)];
  return w;
}

// The screen's pixels (RGB565, as plat_host's fbdump has them) and the canvas a band is drawn in
export class Lcd {
  constructor() { this.fb = new Uint16Array(W * H); }
  fill(c = 0) { this.fb.fill(c); }
  begin(w, h, bg) {
    if (w * h > CV_MAX) h = Math.floor(CV_MAX / w);
    Object.assign(this, { w, h, px: new Uint16Array(w * h).fill(bg) });
  }
  rect(x, y, w, h, c) {
    for (let j = Math.max(y, 0); j < Math.min(y + h, this.h); j++)
      for (let i = Math.max(x, 0); i < Math.min(x + w, this.w); i++) this.px[j * this.w + i] = c;
  }
  // cv_text: each pixel's coverage (0..15) blended over the canvas; the pen's end
  text(x, y, f, s, c) {
    const r = c >> 11, g = (c >> 5) & 63, b = c & 31, d = f.bytes;
    for (const ch of utf8(s)) {
      const gi = glyph(f, ch), w = f.bw[gi], base = f.off[gi];
      let ni = 0, run = 0, a = 0;
      const nib = () => { const v = (ni & 1) ? d[base + (ni >> 1)] & 15 : d[base + (ni >> 1)] >> 4; ni++; return v; };
      for (let gy = 0; gy < f.h; gy++)
        for (let gx = 0; gx < w; gx++) {
          const px = x - f.pad + gx, py = y + gy;
          if (!run) {
            a = nib();
            run = 1;
            if (a === 0 || a === 15) run += nib();
          }
          run--;
          if (!a || px < 0 || px >= this.w || py < 0 || py >= this.h) continue;
          const at = py * this.w + px;
          if (a === 15) { this.px[at] = c; continue; }
          const bg = this.px[at], br = bg >> 11, bgg = (bg >> 5) & 63, bb = bg & 31;
          const nr = Math.floor((r * a + br * (15 - a)) / 15), ng = Math.floor((g * a + bgg * (15 - a)) / 15),
                nb = Math.floor((b * a + bb * (15 - a)) / 15);
          this.px[at] = (nr << 11) | (ng << 5) | nb;
        }
      x += f.adv[gi];
    }
    return x;
  }
  blit(x, y) {   // lcd_blit: the canvas onto the screen, clipped
    for (let j = 0; j < this.h; j++)
      for (let i = 0; i < this.w; i++)
        if (x + i < W && y + j < H) this.fb[(y + j) * W + x + i] = this.px[j * this.w + i];
  }
  // the screen as 8-bit RGBA (the panel's canvas)
  rgba(out = new Uint8ClampedArray(W * H * 4)) {
    for (let i = 0; i < W * H; i++) {
      const v = this.fb[i], r = v >> 11, g = (v >> 5) & 63, b = v & 31;
      out.set([(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2), 255], 4 * i);
    }
    return out;
  }
}

// ------------------------------------------------------------------------------------- the words (app.c, app.h) ---
export const APP_NAME = "VNL-1";
export const KEY_NAME = [["C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"],
                         ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"]];
export const MODE_NAME = ["MAJOR", "MINOR"];
export const MOOD_NAME = ["SAD", "WISTFUL", "WARM", "SUNNY"];
export const SEC_NAME = ["INTRO", "GROOVE", "LEAD", "BREAK", "OUTRO"];

// app.c's field_step: the value d steps from v (wrapping or stopping at its ends)
export function fieldStep(def, v, d) {
  if (def.wrap) return (((v + d) % def.wrap) + def.wrap) % def.wrap;
  return Math.min(Math.max(v + d, def.min ?? -Infinity), def.max ?? Infinity);
}

// app.c's value_text
export function valueText(def, v, mode) {
  if (def.format === "key") return KEY_NAME[mode % 2][v % 12];
  if (def.format === "mood") return MOOD_NAME[v & 3];
  if (def.format === "mode") return MODE_NAME[v % 2];
  return String(v);
}

const fill = (t, bag) => t.replace(/\{(\w+)\}/g, (_, k) => String(bag[k] ?? ""));
const holds = (when, bag) => [].concat(when ?? []).every((w) => (w[0] === "!" ? !bag[w.slice(1)] : !!bag[w]));

// The values a screen's templates read, from what the screen shows (Device.shown(): app.c's snap_t, as the UI holds
// it). app.c's line_pending is `pending`
const MENU_MAX = 10;
// a field's value as a share of its range (0..1): a knob's fill bar
const fracOf = (def, v) => def.wrap ? v / (def.wrap - 1) : (v - (def.min ?? 0)) / ((def.max ?? 100) - (def.min ?? 0));
// A field's value now: the roll's (key, tempo, ...), else its workbench row's (rows: the page's { values, options })
export const fieldValue = (def, k, roll, rows) => (roll?.[k] !== undefined ? roll[k] : rows?.values?.[def.row]);
// a field's text: an options field (`options: true`) names its value from its row's options (an instrument, a
// rhythm); `format: "variant"` a re-roll (0 as rolled, else variant n); else app.c's value_text
export function fieldText(def, v, mode, rows) {
  if (v === undefined) return "";
  if (def.options) return rows?.options?.[def.row]?.find((o) => String(o.value) === String(v))?.name ?? String(v);
  if (def.format === "variant") return v ? `VARIANT ${v}` : "AS ROLLED";
  return valueText(def, v, mode);
}
const fieldFrac = (def, v, rows) => {
  if (v === undefined) return 0;
  if (!def.options) return fracOf(def, v);
  const o = rows?.options?.[def.row] ?? [], i = o.findIndex((x) => String(x.value) === String(v));
  return o.length > 1 && i >= 0 ? i / (o.length - 1) : 0;
};
// a step of d from v: an options field steps through its row's options (no wrap), else fieldStep
export function fieldMove(def, v, d, rows) {
  if (!def.options) return fieldStep(def, v, d);
  const o = rows?.options?.[def.row] ?? [], i = o.findIndex((x) => String(x.value) === String(v));
  return o.length ? o[Math.min(Math.max((i < 0 ? 0 : i) + d, 0), o.length - 1)].value : v;
}
export function valuesOf(s, data) {
  const r = s.roll ?? {}, f = data.fields, song = !!s.hasSong;
  const bag = {
    APP: APP_NAME, song, stopped: !!s.stopped, idle: s.stopped ? "STOPPED" : "NEXT SONG",
    msg: !!s.msg, msg0: s.msg?.[0] ?? "", msg1: s.msg?.[1] ?? "",
    map: (s.map ?? "").toUpperCase(), section: SEC_NAME[s.section] ?? "", bar: (s.bar ?? 0) + 1, bars: r.bars,
    chord: s.chord ?? "", pending: "",
  };
  for (const [k, def] of Object.entries(f)) {
    const v = fieldValue(def, k, r, s.rows);
    bag[k] = fieldText(def, v, r.mode ?? 0, s.rows);
    bag[`${k}Frac`] = fieldFrac(def, v, s.rows);
  }
  bag.page = s.page ?? "";
  bag.progress = song && r.bars ? Math.min(1, ((s.bar ?? 0) + 1) / r.bars) : 0;
  if (song && s.pend) {
    const def = f[s.pend.field];
    bag.pending = `${def?.name ?? "CHANGE"}${s.pend.val !== undefined ? ` ${fieldText(def, s.pend.val, r.mode ?? 0, s.rows)}` : ""}` +
                  ` IN ${s.pend.bars}${s.pend.bars === 1 ? " BAR" : " BARS"}`;
  }
  for (const [k, v] of Object.entries(s.labels ?? {})) if (!(k in bag)) bag[k] = v;   // the rolls, by row: {kit}, {lead}
  for (let i = 0; i < MENU_MAX; i++) bag[`sel${i}`] = s.cursor === i;                  // a menu's highlighted line
  for (const [k, t] of Object.entries(data.lines ?? {})) bag[k] = fill(t, bag);
  return bag;
}

// a screen's bands onto lcd, from its values (app.c's draw(), as data)
export function drawScreen(lcd, screen, bag, data, C = colors(THEME, data.colors)) {
  for (const band of screen.bands) {
    lcd.begin(W, band.h, C[band.bg ?? "BLACK"]);
    let pen = 0;
    for (const it of band.draw) {
      if (!holds(it.when, bag)) continue;
      if (it.rect) lcd.rect(...it.rect, C[it.color]);
      else if (it.icon) {   // a 1-bit icon from the data's `icons` (rows of "#" and "."): { icon, x, y, color }
        (data.icons?.[it.icon] ?? []).forEach((row, j) => [...row].forEach((c, i) => { if (c === "#") lcd.rect(it.x + i, it.y + j, 1, 1, C[it.color]); }));
      }
      else if (it.meter) {   // a bar filled to a value's share (0..1): { meter: [x, y, w, h], value, color, back }
        const [x, y, w, h] = it.meter, v = Math.max(0, Math.min(1, Number(bag[it.value]) || 0));
        if (it.back) lcd.rect(x, y, w, h, C[it.back]);
        lcd.rect(x, y, Math.round(w * v), h, C[it.color]);
      }
      else if (it.knobs) {
        const k = it.knobs;
        (screen.knobs ?? []).forEach((kn, i) => {
          const x = k.x + i * k.dx;
          lcd.text(x, k.label.y, font(k.label.font), kn.label ?? data.fields[kn.field].name, C[k.label.color]);
          if (holds(k.value.when, bag)) lcd.text(x, k.value.y, font(k.value.font), String(bag[kn.field] ?? ""), C[k.value.color]);
          if (k.bar && holds(k.value.when, bag)) {   // { y, w, h, color, back }: the knob's place in its range
            lcd.rect(x, k.bar.y, k.bar.w, k.bar.h, C[k.bar.back]);
            lcd.rect(x, k.bar.y, Math.round(k.bar.w * Math.max(0, Math.min(1, bag[`${kn.field}Frac`] ?? 0))), k.bar.h, C[k.bar.color]);
          }
        });
      } else {
        const f = font(it.font), s = fill(it.text, bag);
        const x = it.right !== undefined ? it.right - textW(f, s) : it.after !== undefined ? pen + it.after : it.x;
        pen = lcd.text(x, it.y, f, s, C[it.color]);
      }
    }
    lcd.blit(0, band.y);
  }
}

// what the screen reader says for a screen (app.c's speak())
export const sayOf = (screen, bag) =>
  (screen.say ?? []).map((p) => (typeof p === "string" ? fill(p, bag) : holds(p.when, bag) ? fill(p.text, bag) : "")).join("");

// The controls, by printed label (plat.h's B_* and EN_*), and the four knobs
export const BUTTONS = ["FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+"];
export const ENCODERS = ["SELECT", "ALGO", "PRESET"];
export const KNOBS = ["K1", "K2", "K3", "K4"];

// A control's action on a screen (the screen's own, else the defaults'; null: none)
export function actionOf(data, id, control) {
  const s = data.screens[id], own = s?.controls ?? {};
  if (control in own) return own[control];
  const k = KNOBS.indexOf(control);
  if (k >= 0) { const kn = s?.knobs?.[k]; return kn ? { do: "turn", field: kn.field, step: kn.step ?? 1 } : null; }
  return data.defaults?.[control] ?? null;
}

// Every transition the data has: [{ from, control, to }] (to: a screen id, "back" or "home"), for the flow view
export function transitions(data) {
  const out = [];
  for (const id of Object.keys(data.screens))
    for (const c of [...BUTTONS, ...ENCODERS]) {
      const a = actionOf(data, id, c);
      if (a?.go && !(a.go === "home" && id === data.home)) out.push({ from: id, control: c, to: a.go });
    }
  return out;
}

// What's wrong with the data (an empty list: nothing): the panel shows these, smoke.mjs checks there are none
export function problems(data) {
  const out = [], C = colors(THEME, data.colors), ids = Object.keys(data.screens ?? {});
  if (!data.screens?.[data.home]) out.push(`home "${data.home}" is not a screen`);
  for (const p of data.parts ?? []) if (!data.screens?.[p]) out.push(`parts: no screen "${p}"`);
  const known = new Set(["APP", "song", "stopped", "idle", "msg", "msg0", "msg1", "map", "section", "bar", "bars", "chord",
                         "pending", "rhythm", "progress", "page", ...Object.keys(data.fields ?? {}).map((k) => `${k}Frac`), ...ROWS.map((r) => r.id), ...Array.from({ length: MENU_MAX }, (_, i) => `sel${i}`),
                         ...Object.keys(data.fields ?? {}), ...Object.keys(data.lines ?? {})]);
  const tpl = (where, t) => { for (const [, k] of String(t).matchAll(/\{(\w+)\}/g)) if (!known.has(k)) out.push(`${where}: no value {${k}}`); };
  const cond = (where, w) => { for (const x of [].concat(w ?? [])) if (!known.has(x.replace(/^!/, ""))) out.push(`${where}: no value "${x}"`); };
  const act = (where, a) => {
    if (a === null) return;
    if (a.go) { if (!["back", "home", "cursor"].includes(a.go) && !ids.includes(a.go)) out.push(`${where}: no screen "${a.go}"`); }
    else if (["play", "cursor", "part", "page"].includes(a.do)) { /* */ }
    else if (a.do === "nudge" || a.do === "turn") { if (!data.fields?.[a.field]) out.push(`${where}: no field "${a.field}"`); }
    else out.push(`${where}: unknown action ${JSON.stringify(a)}`);
  };
  for (const [k, t] of Object.entries(data.lines ?? {})) tpl(`lines.${k}`, t);
  for (const [c, a] of Object.entries(data.defaults ?? {})) {
    if (![...BUTTONS, ...ENCODERS].includes(c)) out.push(`defaults: no control ${c}`);
    act(`defaults.${c}`, a);
  }
  for (const [id, s] of Object.entries(data.screens ?? {})) {
    for (const m of [...(s.menu ?? []), ...(s.pages ?? [])]) if (!ids.includes(m)) out.push(`${id}: no screen "${m}"`);
    for (const [i, b] of (s.bands ?? []).entries()) {
      const w = `${id}.bands[${i}]`;
      if (!(b.y >= 0 && b.h > 0 && b.y + b.h <= H)) out.push(`${w}: outside the screen (y ${b.y}, h ${b.h})`);
      if (b.bg && !(b.bg in C)) out.push(`${w}: no color ${b.bg}`);
      for (const it of b.draw ?? []) {
        cond(w, it.when);
        const faces = it.knobs ? [it.knobs.label.font, it.knobs.value.font] : it.text !== undefined ? [it.font] : [];
        for (const f of faces) if (!FONTS[f]) out.push(`${w}: no font ${f}`);
        if (it.meter) cond(w, it.value);
        if (it.icon && !data.icons?.[it.icon]) out.push(`${w}: no icon ${it.icon}`);
        for (const c of it.knobs ? [it.knobs.label.color, it.knobs.value.color, ...(it.knobs.bar ? [it.knobs.bar.color, it.knobs.bar.back] : [])]
                        : [it.color, ...(it.back ? [it.back] : [])])
          if (!(c in C)) out.push(`${w}: no color ${c}`);
        if (it.text !== undefined) tpl(w, it.text);
        if (it.knobs) cond(w, it.knobs.value.when);
      }
    }
    if ((s.knobs ?? []).length > 4) out.push(`${id}: more than four knobs`);
    for (const k of s.knobs ?? []) if (!data.fields?.[k.field]) out.push(`${id}.knobs: no field "${k.field}"`);
    for (const [c, a] of Object.entries(s.controls ?? {})) {
      if (![...BUTTONS, ...ENCODERS].includes(c)) out.push(`${id}.controls: no control ${c} (K1-K4 are knobs)`);
      act(`${id}.controls.${c}`, a);
    }
    for (const p of s.say ?? []) typeof p === "string" ? tpl(`${id}.say`, p) : (tpl(`${id}.say`, p.text), cond(`${id}.say`, p.when));
  }
  return out;
}

// --------------------------------------------------------------------------------------------- the device's UI ---
// The screens driven by the controls, on what the engine plays. engine: { set(row, value), play() } (the page's: the
// rows' own calls). update(live) each position: { playing, gap, bar (from 1: wb_bar), bars, seed, roll: { key, tempo,
// ... }, facts: wb_screen's { section, map, chord } }. A knob's change waits for the next 4-bar boundary, shown
// pending, then goes to the engine in the bar before it (the workbench lands a change at the next bar line).
export class Device {
  constructor(data, engine, now = () => Date.now()) {
    Object.assign(this, { data, engine, now, stack: [data.home], cursor: {}, req: {}, msg: null, said: "", live: null });
  }
  get id() { return this.stack[this.stack.length - 1]; }
  get screen() { return this.data.screens[this.id]; }
  hasSong(l = this.live) { return !!l && l.playing && !l.gap && l.bar > 0; }

  // new screen data (a reload): the breadcrumb kept where its screens still exist
  load(data) {
    this.data = data;
    this.stack = this.stack.filter((id) => data.screens[id]);
    if (this.stack[0] !== data.home) this.stack = [data.home, ...this.stack.filter((id) => id !== data.home)];
  }

  update(live) {
    const was = this.live;
    this.live = live;
    if (!this.hasSong() || (was && was.seed !== live.seed)) this.req = {};   // a new song or none: nothing waits
    const bar = live.bar - 1;
    for (const [f, q] of Object.entries(this.req)) {
      if (bar >= q.at) delete this.req[f];
      else if (!q.sent && bar >= q.at - 1) {
        q.sent = true;
        this.engine.set(this.data.fields[f].row, q.val);
      }
    }
  }

  // what the screen shows now (app.c's snap_t and ui.msg)
  shown() {
    const l = this.live ?? {}, song = this.hasSong(), bar = (l.bar ?? 1) - 1;
    const roll = { ...(l.roll ?? {}), bars: l.bars };
    let pend = null;
    for (const [f, q] of Object.entries(this.req))   // the earliest; a tie: the first field (app.c: the lowest RF_*)
      if (!pend || q.at < pend.at) pend = { field: f, val: q.val, at: q.at, bars: q.at - bar };
    return { hasSong: song, stopped: !l.playing, roll, bar, section: l.facts?.section ?? -1, map: l.facts?.map,
             chord: l.facts?.chord, labels: l.labels, rows: l.rows,
             page: this.screen?.pages?.length > 1 ? `${this.screen.pages.indexOf(this.id) + 1}/${this.screen.pages.length}` : "",
             cursor: this.screen?.menu ? this.cursor[this.id] ?? 0 : -1, pend, msg: this.msg && this.now() < this.msg.until ? this.msg.text : null };
  }
  values() { return valuesOf(this.shown(), this.data); }
  draw(lcd) { drawScreen(lcd, this.screen, this.values(), this.data); }

  say(a, b = "") {   // app_say: the header for 1.3 s, and the screen reader
    this.msg = { text: [a.slice(0, 23), b.slice(0, 23)], until: this.now() + 1300 };
    this.said = b ? `${a} ${b}` : a;
  }
  speak() { this.said = sayOf(this.screen, this.values()); return this.said; }   // the reader asks for the screen

  // a control pressed (a button) or turned (an encoder or knob: detents, + clockwise); what it did, for the flow view
  press(control, detents = 1) {
    const a = actionOf(this.data, this.id, control);
    if (!a) return null;
    if (a.do === "part" || a.do === "page") {   // ALGO: the next part (data.parts, at its first page); SELECT: the next page
      const list = a.do === "part" ? this.data.parts ?? [] : this.screen.pages ?? [], from = this.id;
      const here = a.do === "part" ? list.findIndex((id) => id === from || this.data.screens[id]?.pages?.includes(from)) : list.indexOf(from);
      if (here < 0 || list.length < 2) return null;
      this.stack[this.stack.length - 1] = list[(((here + detents) % list.length) + list.length) % list.length];
      if (this.id !== from) this.speak();
      return { from, control, to: this.id };
    }
    if (a.do === "cursor") {   // a menu: the highlight moves (wraps)
      const n = this.screen.menu?.length ?? 0;
      if (n) this.cursor[this.id] = (((this.cursor[this.id] ?? 0) + detents) % n + n) % n;
      return null;
    }
    if (a.go) {
      const from = this.id, to = a.go === "cursor" ? this.screen.menu?.[this.cursor[this.id] ?? 0] : a.go;
      if (!to) return null;
      if (to === "back") { if (this.stack.length > 1) this.stack.pop(); }
      else if (to === "home") this.stack = [this.data.home];
      else if (to !== this.id) this.stack.push(to);
      if (this.id !== from) this.speak();
      return { from, control, to: this.id };
    }
    if (a.do === "play") {
      this.say(this.live?.playing ? "STOP" : "PLAY");
      this.engine.play();
    } else if (a.do === "turn" || a.do === "nudge") this.request(a.field, a.do === "turn" ? detents * (a.step ?? 1) : a.by);
    return null;
  }

  // a knob's request (app.c's input() and command()): from the value waiting, else the song's; on the next 4-bar
  // boundary (unless the song ends first)
  request(f, d) {
    const def = this.data.fields[f];
    if (!this.hasSong()) return;
    const l = this.live, bar = l.bar - 1, from = this.req[f]?.val ?? fieldValue(def, f, l.roll, l.rows), v = fieldMove(def, from, d, l.rows);
    if (v !== from) {
      const at = (Math.floor(bar / 4) + 1) * 4;
      if (at < l.bars) {
        this.req[f] = { val: v, at, sent: false };
        if (bar >= at - 1) this.update(l);
      }
    }
    this.say(def.name, fieldText(def, v, l.roll.mode ?? 0, l.rows));
  }
}

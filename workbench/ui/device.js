// SPDX-License-Identifier: GPL-3.0-or-later
// The device mock's core, no DOM (Node runs it too: tools/screen_check.mjs, smoke.mjs): the FM-1's LCD as gfx.c draws
// it, today's screen's text as app.c writes it, and the screens of ui/screens.js driven by the panel's controls.
import { FONTS } from "./fonts.js";
import { ROWS } from "../rows.js";
import VOCAB from "./vocab.json" with { type: "json" };

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
// each mode's keys spelled by its parent major, the simplest way (app.c's KEY_NAME: major, Dorian, Phrygian, Lydian,
// Mixolydian, minor)
export const KEY_NAME = [["C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"],
                         ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"],
                         ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "Bb", "B"],
                         ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"],
                         ["C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"],
                         ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"]];
export const MODE_NAME = ["MAJOR", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN", "MINOR"];
export const MOOD_NAME = ["SAD", "WISTFUL", "WARM", "SUNNY"];
export const SEC_NAME = ["INTRO", "GROOVE", "LEAD", "BREAK", "OUTRO"];

// app.c's field_step: the value d steps from v (wrapping or stopping at its ends)
export function fieldStep(def, v, d) {
  if (def.wrap) return (((v + d) % def.wrap) + def.wrap) % def.wrap;
  return Math.min(Math.max(v + d, def.min ?? -Infinity), def.max ?? Infinity);
}

// app.c's value_text
export function valueText(def, v, mode) {
  if (def.format === "key") return KEY_NAME[mode % 6][v % 12];
  if (def.format === "mood") return MOOD_NAME[v & 3];
  if (def.format === "mode") return MODE_NAME[v % 6];
  return String(v);
}

const fill = (t, bag) => t.replace(/\{(\w+)\}/g, (_, k) => String(bag[k] ?? ""));
const holds = (when, bag) => [].concat(when ?? []).every((w) => (w[0] === "!" ? !bag[w.slice(1)] : !!bag[w]));

// The values a screen's templates read, from what the screen shows (Device.shown(): app.c's snap_t, as the UI holds
// it). app.c's line_pending is `pending`
const MENU_MAX = 10;
const SEC_KEYS = "FGABCDEFGA";   // the white keys from F3, a section each (the Sections screen's jumps)
// a field's value as a share of its range (0..1): a knob's fill bar. Only a number has one (Sam, 2026-11-06: tempo,
// motion); a choice from a list (a key, a mood, an instrument) doesn't: its place in the list means little
export const hasBar = (def) => !!def && !def.options && !def.format && !def.wrap;
const fracOf = (def, v) => def.wrap ? v / (def.wrap - 1) : (v - (def.min ?? 0)) / ((def.max ?? 100) - (def.min ?? 0));
// A field's value now: the roll's (key, tempo, ...), else its workbench row's (rows: the page's { values, options })
export const fieldValue = (def, k, roll, rows) => (roll?.[k] !== undefined ? roll[k] : rows?.values?.[def.row]);
// a field's text: an options field (`options: true`) names its value from its row's options (an instrument, a
// rhythm); `format: "variant"` a re-roll (0 as rolled, else variant n); else app.c's value_text
export function fieldText(def, v, mode, rows) {
  if (v === undefined) return "";
  if (def.options) return rows?.options?.[def.row]?.find((o) => String(o.value) === String(v))?.name ?? String(v);
  if (def.format === "variant") {   // a re-roll: what it plays, where the engine names it (the bass line's style)
    const what = String(v) === String(rows?.values?.[def.row]) ? rows?.rolled?.[def.row] : undefined;
    return `${v ? `VARIANT ${v}` : what ? "AS ROLLED" : "ORIGINAL"}${what ? ` (${what})` : ""}`;   // nothing to name: ORIGINAL
  }
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
  if (!o.length) return v;
  if (def.wrap) return o[((((i < 0 ? 0 : i) + d) % o.length) + o.length) % o.length].value;   // a switch: either way flips it
  return o[Math.min(Math.max((i < 0 ? 0 : i) + d, 0), o.length - 1)].value;
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
  bag.ready = r.key !== undefined && !!s.stopped;   // stopped with a song ready (its key and tempo are known)
  bag.known = song || bag.ready;                     // a song playing or ready: its values can show
  // the song's map (wb_sections, heard): each section's name and length, the one playing; looping (the workbench's
  // Loop section)
  const secs = s.sections ?? [], bar1 = (s.bar ?? 0) + 1, off = s.secOff ?? 0, shownN = s.secRows ?? MENU_MAX;
  bag.secCount = secs.length; bag.secList = secs;
  secs.slice(off, off + Math.min(shownN, MENU_MAX)).forEach((x, i) => {   // line i shows section off + i (Sam: it scrolls)
    bag[`sec${i}`] = String(x.name ?? "").toUpperCase(); bag[`sec${i}Bars`] = x.bars;
    bag[`sec${i}Now`] = song && bar1 > x.start && bar1 <= x.start + x.bars;
    bag[`sec${i}Key`] = SEC_KEYS[off + i] ?? "";
  });
  bag.secRange = secs.length > shownN ? `${off + 1}-${Math.min(off + shownN, secs.length)} OF ${secs.length}` : "";
  bag.looping = String(s.rows?.values?.loop) === "1";
  bag.activity = s.activity ?? null;
  bag.levels = s.levels ?? {};
  bag.barPos = s.barPos ?? (s.bar ?? 0);   // bars from 0, with the fraction of the bar playing
  // a list's ends (Sam, 2026-11-06: arrows, not a bar): {field}First / {field}Last where the value (or the change
  // waiting) can't move further that way; a number without ends (a wrap) has neither
  for (const [k, def] of Object.entries(f)) {
    const v = k in (s.reqs ?? {}) ? s.reqs[k] : fieldValue(def, k, r, s.rows);
    if (v === undefined || def.wrap) continue;   // (a wrapping number or switch has no ends)
    if (def.options) {
      const o = s.rows?.options?.[def.row] ?? [], i = o.findIndex((x) => String(x.value) === String(v));
      bag[`${k}First`] = i <= 0; bag[`${k}Last`] = i < 0 || i >= o.length - 1;
    } else { bag[`${k}First`] = v <= (def.min ?? -Infinity); bag[`${k}Last`] = v >= (def.max ?? Infinity); }
  }
  // a change waiting for its bar shows at once, marked (Sam, 2026-11-06: "*", in the pending color): the field's
  // value and its row's label (a tile's {kit}); {field}Pend / {row}Pend true. Not on the reference screen (today's
  // device draws the old value until it lands)
  const waiting = s.mark ? Object.entries(s.reqs ?? {}) : [];
  // and a change made elsewhere (the workbench's rows) staged but not playing yet: its row's value differs from the
  // song's as it plays (rows.live)
  if (s.mark && s.rows?.live)
    for (const [k, def] of Object.entries(f)) {
      if (!def.row || k in (s.reqs ?? {})) continue;
      const staged = s.rows.values?.[def.row], now = r[k] !== undefined ? r[k] : s.rows.live[def.row];
      if (staged !== undefined && now !== undefined && String(staged) !== String(now)) waiting.push([k, staged]);
    }
  for (const [k, v] of waiting) {
    const def = f[k], t = fieldText(def, v, r.mode ?? 0, s.rows);
    bag[k] = `*${t}`; bag[`${k}Pend`] = true; bag[`${k}Frac`] = fieldFrac(def, v, s.rows);
    if (def.row) { bag[`${def.row}Pend`] = `*${t.replace(/^AS ROLLED \((.*)\)$/, "$1")}`; }
  }
  bag.progress = song && r.bars ? Math.min(1, ((s.bar ?? 0) + 1) / r.bars) : 0;
  if (song && s.pend) {
    const def = f[s.pend.field];
    bag.pending = `${def?.name ?? "CHANGE"}${s.pend.val !== undefined ? ` ${fieldText(def, s.pend.val, r.mode ?? 0, s.rows)}` : ""}` +
                  ` IN ${s.pend.bars}${s.pend.bars === 1 ? " BAR" : " BARS"}`;
  }
  const pr = s.partRoll;   // a part's re-roll waiting (SEQ on its screen)
  if (pr && (!s.pend || pr.at < s.pend.at) && (!s.jump || pr.at <= s.jump.at))
    bag.pending = `${pr.name}: NEW ROLL IN ${pr.bars}${pr.bars === 1 ? " BAR" : " BARS"}`;
  if (s.jump) {
    if ((!s.pend || s.jump.at < s.pend.at) && (!pr || s.jump.at < pr.at))
      bag.pending = `GO TO ${String(s.jump.name).toUpperCase()} IN ${s.jump.bars}${s.jump.bars === 1 ? " BAR" : " BARS"}`;
    const ji = s.jump.i - (s.secOff ?? 0);   // its line, when shown
    if (bag[`sec${ji}`] !== undefined) { bag[`sec${ji}`] = `*${bag[`sec${ji}`]}`; bag[`sec${ji}Pend`] = true; }
  }
  for (const [k, v] of Object.entries(s.labels ?? {})) if (!(k in bag)) bag[k] = bag[`${k}Pend`] || v;   // the rolls, by row: {kit}, {lead}
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
      else if (it.lanes) {   // { lanes: [x, y, w, h], parts, label, rowH, window }: the workbench's piano roll, small: a
        // lane a part, its notes by time (x) and pitch (y; a drum lane one row a drum), brighter for louder; `window`
        // bars across, scrolling with the song (the playhead a quarter in); bar lines, section starts; played notes in
        // the part's color, those to come gray; a muted (or soloed-out) part dim
        const [x, y, w] = it.lanes, act = bag.activity, rowH = it.rowH ?? 22, lx = x + (it.label ?? 52), lw = w - (lx - x);
        const bars = act?.bars || 1, span = Math.min(it.window ?? 8, bars), pos = Math.max(0, bag.barPos ?? 0), f = font("T");
        // the window steps a bar at a time (Sam, 2026-11-06: live scrolling was too much motion; `live: true` scrolls
        // smoothly, for a future "more movement" setting)
        const at = it.live ? pos : Math.floor(pos);
        const from = Math.max(0, Math.min(bars - span, at - Math.floor(span / 4))), ppb = lw / span, X = (b) => lx + (b - from) * ppb;
        const dim = (c, k) => { const r = c >> 11, g = (c >> 5) & 63, b = c & 31; return (Math.round(r * k) << 11) | (Math.round(g * k) << 5) | Math.round(b * k); };
        const height = it.parts.length * rowH - 2;
        if (bag.song) {   // the bar playing, highlighted whole (Sam, 2026-11-06: not a line in front of it)
          const b0 = Math.floor(pos), xa = Math.max(lx, Math.round(X(b0))), xb = Math.min(lx + lw, Math.round(X(b0 + 1)));
          lcd.rect(xa, y, xb - xa, height, C.LINE);
        }
        for (let b = Math.ceil(from); b <= from + span; b++) lcd.rect(Math.round(X(b)), y, 1, height, C.LINE);   // bar lines
        for (const sb of act?.sections ?? []) if (sb > from && sb < from + span) lcd.rect(Math.round(X(sb)), y, 1, height, C.DIM);
        it.parts.forEach((p, i) => {
          const ry = y + i * rowH, part = p === "keys" ? "comp" : p, col = C[part.toUpperCase()] ?? C.WHITE;
          const off = act?.solo ? act.solo !== p : act?.mute?.includes(p), sel = bag[`sel${i}`];
          if (sel) lcd.rect(x - 4, ry, lx - x, rowH - 2, C.LINE);
          if (data.icons?.[part]) data.icons[part].forEach((row, j) => [...row].forEach((c, k) => { if (c === "#") lcd.rect(x + k, ry + 4 + j, 1, 1, off ? C.DIM : col); }));
          lcd.text(x, ry + 13, f, part.toUpperCase().slice(0, 7), off ? C.DIM : col);
          if (it.keys?.[i]) lcd.text(x + 32, ry + 2, f, it.keys[i], C.DIM);   // its key (press mutes, hold solos)
          if (act?.mute?.includes(p)) lcd.text(x + 12, ry + 2, f, "M", C.HI);
          if (act?.solo === p) lcd.text(x + 12, ry + 2, f, "S", C.HI);
          lcd.rect(lx, ry + rowH - 2, lw, 1, C.LINE);
          for (const [nx, nw, ny, nh, shade] of act?.notes?.[p] ?? []) {
            if (nx + nw < from || nx > from + span) continue;
            const x0 = Math.max(lx, Math.round(X(nx))), x1 = Math.min(lx + lw, Math.round(X(nx + nw)));
            // reduced motion (the default, Sam 2026-11-06): no note lights as it sounds, and played vs to come changes a
            // bar at a time; `live` (more movement) lights the sounding notes and colors them as they pass
            const now = it.live && bag.song && nx <= pos && pos < nx + Math.max(nw, 0.06);
            const base = off ? C.DIM : now ? C.WHITE : nx < (it.live ? pos : Math.floor(pos) + 1) ? col : C.GRAY;   // played or this bar: its color (Sam: only bars to come gray)
            lcd.rect(x0, ry + 1 + Math.round(ny * (rowH - 4)), Math.max(1, x1 - x0 - 1), Math.max(1, Math.round(nh * (rowH - 4))),
                     now && !off ? base : dim(base, 0.45 + 0.55 * shade));
          }
        });
      }
      else if (it.faders) {   // { faders: [x, y, w, h], parts }: the mixer, a column a part (levels -12..+6 dB)
        const [x, y, w, h] = it.faders, n = it.parts.length, cw = Math.floor(w / n), act = bag.activity, f = font("T");
        const top = y + 14, tH = h - 34, at = (db) => top + Math.round(((6 - db) / 18) * tH);
        it.parts.forEach((p, i) => {
          const cx = x + i * cw, part = p === "keys" ? "comp" : p, col = C[part.toUpperCase()] ?? C.WHITE;
          const off = act?.solo ? act.solo !== p : act?.mute?.includes(p), db = bag.levels?.[p] ?? 0, mid = cx + Math.floor(cw / 2);
          if (bag[`sel${i}`]) lcd.rect(cx + 1, y, cw - 2, h, C.LINE);
          if (data.icons?.[part]) data.icons[part].forEach((row, j) => [...row].forEach((c, k) => { if (c === "#") lcd.rect(mid - 4 + k, y + 2 + j, 1, 1, off ? C.DIM : col); }));
          lcd.rect(mid - 2, top, 4, tH, C.DIM);                          // the track
          lcd.rect(mid - 2, at(db), 4, top + tH - at(db), off ? C.GRAY : col);   // the level, from the bottom
          lcd.rect(mid - 6, at(0), 12, 1, C.WHITE);                       // 0 dB
          lcd.rect(mid - 7, at(db) - 1, 14, 3, off ? C.GRAY : C.WHITE);   // the fader's cap
          const t = `${db > 0 ? "+" : ""}${db}`;
          lcd.text(mid - Math.floor(textW(f, t) / 2), y + h - 18, f, t, off ? C.DIM : C.WHITE);
          const ms = act?.solo === p ? "S" : act?.mute?.includes(p) ? "M" : "";
          if (ms) lcd.text(mid - Math.floor(textW(f, ms) / 2), y + h - 8, f, ms, C.HI);
        });
      }
      else if (it.sections) {   // the song's map: a segment per section (by its bars), the one playing filled to
        // where the song is in it, those before it in `past`, those after in `back`
        const [x, y, w, h] = it.sections, secs = bag.secList ?? [], total = secs.reduce((a, s) => a + s.bars, 0);
        let at = x;
        secs.forEach((sec, i) => {
          const sw = Math.max(2, Math.round((w * sec.bars) / (total || 1)) - 2), bar = bag.bar ?? 0;
          const now = bar > sec.start && bar <= sec.start + sec.bars, done = bar > sec.start + sec.bars;
          lcd.rect(at, y, sw, h, C[done ? it.past : it.back]);
          if (now && bag.song) lcd.rect(at, y, Math.max(1, Math.round((sw * (bar - sec.start)) / sec.bars)), h, C[it.color]);
          at += sw + 2;
        });
      }
      else if (it.meter) {   // a bar filled to a value's share (0..1): { meter: [x, y, w, h], value, color, back }
        const [x, y, w, h] = it.meter, v = Math.max(0, Math.min(1, Number(bag[it.value]) || 0));
        const fd = data.fields?.[String(it.value).replace(/Frac$/, "")];
        if (fd && !hasBar(fd)) continue;   // a field's bar: numbers only
        if (it.back) lcd.rect(x, y, w, h, C[it.back]);
        lcd.rect(x, y, Math.round(w * v), h, C[it.color]);
      }
      else if (it.knobs) {
        const k = it.knobs;
        (screen.knobs ?? []).forEach((kn, i) => {
          const x = k.x + i * k.dx;
          lcd.text(x, k.label.y, font(k.label.font), kn.label ?? data.fields[kn.field].name, C[k.label.color]);
          if (holds(k.value.when, bag))
            lcd.text(x, k.value.y, font(k.value.font), String(bag[kn.field] ?? ""), C[bag[`${kn.field}Pend`] && C.PENDING !== undefined ? "PENDING" : k.value.color]);
          if (k.bar && hasBar(data.fields[kn.field]) && holds(k.value.when, bag)) {   // { y, w, h, color, back }: the knob's place in its range
            lcd.rect(x, k.bar.y, k.bar.w, k.bar.h, C[k.bar.back]);
            lcd.rect(x, k.bar.y, Math.round(k.bar.w * Math.max(0, Math.min(1, bag[`${kn.field}Frac`] ?? 0))), k.bar.h, C[k.bar.color]);
          }
        });
      } else {
        const f = font(it.font), s = fill(it.text, bag);
        const x = it.right !== undefined ? it.right - textW(f, s) : it.after !== undefined ? pen + it.after : it.x;
        const lone = /^\{(\w+)\}$/.exec(it.text)?.[1];   // a value alone: the pending color while it waits
        const col = C[lone && bag[`${lone}Pend`] && C.PENDING !== undefined ? "PENDING" : it.color];
        pen = lcd.text(x, it.y, f, s, col);
        if (it.arrows && lone) {   // "<  >" at the line's right end (Sam, 2026-11-06): an arrow only where the list goes on
          const ac = C[it.arrows === true ? "DIM" : it.arrows], r = it.arrowsRight ?? 232;
          if (!bag[`${lone}Last`]) lcd.text(r - textW(f, ">"), it.y, f, ">", ac);
          if (!bag[`${lone}First`]) lcd.text(r - textW(f, ">") - textW(f, "  <"), it.y, f, "<", ac);
        }
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
export const LONG_MS = 500;   // a long press

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

// The value names a vocabulary's `values` allows for this data (its fixed names, its patterns expanded): a Set
export function valueNames(data, vocab = VOCAB) {
  const sub = { field: Object.keys(data.fields ?? {}), row: ROWS.map((r) => r.id), line: Object.keys(data.lines ?? {}) };
  const expand = (p) => {
    const m = /\{(\d+)-(\d+)\}|\{(field|row|line)\}/.exec(p);
    if (!m) return [p];
    const xs = m[3] ? sub[m[3]] : Array.from({ length: +m[2] - +m[1] + 1 }, (_, i) => String(+m[1] + i));
    return xs.flatMap((x) => expand(p.slice(0, m.index) + x + p.slice(m.index + m[0].length)));
  };
  return new Set([...vocab.values.fixed, ...vocab.values.patterns.flatMap(expand)]);
}
// a draw item's kind: the first of the vocabulary's item kinds it carries (undefined: none)
export const itemKind = (it, vocab = VOCAB) => Object.keys(vocab.items).find((k) => it[k] !== undefined);

// What's wrong with the data (an empty list: nothing): the panel shows these, smoke.mjs and tools/export_screens.mjs
// check there are none. Draw items, actions and values must be ui/vocab.json's (the device implements those only)
export function problems(data, vocab = VOCAB) {
  const out = [], C = colors(THEME, data.colors), ids = Object.keys(data.screens ?? {});
  if (!data.screens?.[data.home]) out.push(`home "${data.home}" is not a screen`);
  for (const p of data.parts ?? []) if (!data.screens?.[p]) out.push(`parts: no screen "${p}"`);
  const known = valueNames(data, vocab);
  const tpl = (where, t) => { for (const [, k] of String(t).matchAll(/\{(\w+)\}/g)) if (!known.has(k)) out.push(`${where}: no value {${k}}`); };
  const cond = (where, w) => { for (const x of [].concat(w ?? [])) if (!known.has(x.replace(/^!/, ""))) out.push(`${where}: no value "${x}"`); };
  const act = (where, a) => {
    if (a === null) return;
    const kind = a.go !== undefined ? "go" : a.do, keys = vocab.actions[kind];
    if (!keys) { out.push(`${where}: unknown action ${kind ?? JSON.stringify(a)}`); return; }
    for (const k of Object.keys(a)) if (k !== "do" && !keys.includes(k)) out.push(`${where}: ${kind} has no key ${k}`);
    if (kind === "go") { if (!["back", "home", "cursor"].includes(a.go) && !ids.includes(a.go)) out.push(`${where}: no screen "${a.go}"`); }
    else if ((kind === "turn" || kind === "nudge") && a.byCursor) {
      if (!Array.isArray(a.byCursor) || a.field !== undefined) out.push(`${where}: byCursor is a list of fields, without field`);
      else for (const f of a.byCursor) if (f !== null && !data.fields?.[f]) out.push(`${where}: no field "${f}"`);
    }
    else if (kind === "nudge" || kind === "turn") { if (!data.fields?.[a.field]) out.push(`${where}: no field "${a.field}"`); }
    if (a.long) act(`${where}.long`, a.long);
  };
  for (const [k, t] of Object.entries(data.lines ?? {})) tpl(`lines.${k}`, t);
  for (const [c, a] of Object.entries(data.defaults ?? {})) {
    if (![...BUTTONS, ...ENCODERS].includes(c) && !/^KEY([0-9]|1[0-9]|2[0-6])$/.test(c)) out.push(`defaults: no control ${c}`);
    act(`defaults.${c}`, a);
  }
  for (const [id, s] of Object.entries(data.screens ?? {})) {
    for (const m of [...(s.menu ?? []), ...(s.pages ?? [])]) if (!ids.includes(m)) out.push(`${id}: no screen "${m}"`);
    for (const [i, b] of (s.bands ?? []).entries()) {
      const w = `${id}.bands[${i}]`;
      if (!(b.y >= 0 && b.h > 0 && b.y + b.h <= H)) out.push(`${w}: outside the screen (y ${b.y}, h ${b.h})`);
      if (b.bg && !(b.bg in C)) out.push(`${w}: no color ${b.bg}`);
      for (const it of b.draw ?? []) {
        const kind = itemKind(it, vocab);
        if (!kind) { out.push(`${w}: no draw item ${Object.keys(it).find((k) => k !== "when") ?? "(empty)"}`); continue; }
        for (const k of Object.keys(it)) if (!vocab.items[kind].includes(k)) out.push(`${w}: ${kind} has no key ${k}`);
        cond(w, it.when);
        const faces = it.knobs ? [it.knobs.label.font, it.knobs.value.font] : it.text !== undefined ? [it.font] : [];
        for (const f of faces) if (!FONTS[f]) out.push(`${w}: no font ${f}`);
        if (it.meter) cond(w, it.value);
        if (it.sections) for (const c of [it.color, it.back, it.past]) if (!(c in C)) out.push(`${w}: no color ${c}`);
        if (it.icon && !data.icons?.[it.icon]) out.push(`${w}: no icon ${it.icon}`);
        for (const c of it.lanes || it.faders ? [] : it.knobs ? [it.knobs.label.color, it.knobs.value.color, ...(it.knobs.bar ? [it.knobs.bar.color, it.knobs.bar.back] : [])]
                        : [it.color, ...(it.back ? [it.back] : [])])
          if (!(c in C)) out.push(`${w}: no color ${c}`);
        if (it.text !== undefined) tpl(w, it.text);
        if (it.knobs) cond(w, it.knobs.value.when);
      }
    }
    if ((s.knobs ?? []).length > 4) out.push(`${id}: more than four knobs`);
    for (const k of s.knobs ?? []) if (!data.fields?.[k.field]) out.push(`${id}.knobs: no field "${k.field}"`);
    for (const [c, a] of Object.entries(s.controls ?? {})) {
      if (![...BUTTONS, ...ENCODERS].includes(c) && !/^KEY([0-9]|1[0-9]|2[0-6])$/.test(c) && !(KNOBS.includes(c) && ["mute", "solo", "level"].includes(a?.do))) out.push(`${id}.controls: no control ${c} (K1-K4 are knobs; keys KEY0-KEY26)`);
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
    Object.assign(this, { data, engine, now, stack: [data.home], cursor: {}, held: {}, req: {}, msg: null, said: "", live: null });
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
    if (!was || was.bar !== live.bar || was.seed !== live.seed) this.barT = this.now();   // the bar began (as far as we know)
    this.live = live;
    if (!this.hasSong() || (was && was.seed !== live.seed)) { this.req = {}; this.jumpReq = null; this.rollReq = null; }   // a new song or none: nothing waits
    const rr = this.rollReq;
    if (rr) {   // a part's re-roll: sent in the bar before its boundary
      const bar = live.bar - 1;
      if (bar >= rr.at) this.rollReq = null;
      else if (!rr.sent && bar >= rr.at - 1) { rr.sent = true; this.engine.rerollPart?.(rr.part); }
    }
    const jr = this.jumpReq;
    if (jr) {   // a jump: sent in the bar before its boundary (the workbench lands it at the next bar line)
      const bar = live.bar - 1;
      // done once the song has moved on from the bar it was sent in: forward past the boundary, or back (a jump to an
      // earlier section, the intro: Sam saw its key keep blinking)
      if (bar >= jr.at || (jr.sent && bar !== jr.sentBar)) this.jumpReq = null;
      else if (!jr.sent && bar >= jr.at - 1) { jr.sent = true; jr.sentBar = bar; this.engine.jump?.(jr.start, jr.name); }
    }
    if (was && was.seed !== live.seed && this.stack.length > 1) {   // a new song (rolled or the next one): back to the
      this.stack = [this.data.home];                                  // main screen (Sam, 2026-11-06)
      this.speak();
    }
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
             chord: l.facts?.chord, labels: l.labels, rows: l.rows, mark: !this.screen?.reference,
             reqs: Object.fromEntries(Object.entries(this.req).map(([f, q]) => [f, q.val])),
             partRoll: this.rollReq && song ? { name: this.rollReq.name, at: this.rollReq.at, bars: this.rollReq.at - bar } : null,
             jump: this.jumpReq && song ? { name: this.jumpReq.name, i: this.jumpReq.i, at: this.jumpReq.at, bars: this.jumpReq.at - bar } : null,
             page: this.screen?.pages?.length > 1 ? `${this.screen.pages.indexOf(this.id) + 1}/${this.screen.pages.length}` : "",
             cursor: this.screen?.menu || this.screen?.list || this.screen?.lanes ? (this.cursor[this.id] ?? 0) - this.listOff() : -1,
             secOff: this.listOff(), secRows: this.screen?.rows, sections: l.sections,
             activity: l.activity, levels: l.levels,
             barPos: song ? bar + Math.min(0.999, (this.now() - (this.barT ?? this.now())) / (240000 / Math.max(40, l.roll?.tempo ?? 80))) : bar, pend, msg: this.msg && this.now() < this.msg.until ? this.msg.text : null };
  }
  values() { return valuesOf(this.shown(), this.data); }
  draw(lcd) { drawScreen(lcd, this.screen, this.values(), this.data); }

  say(a, b = "") {   // app_say: the header for 1.3 s, and the screen reader
    this.msg = { text: [a.slice(0, 23), b.slice(0, 23)], until: this.now() + 1300 };
    this.said = b ? `${a} ${b}` : a;
  }
  speak() { this.said = sayOf(this.screen, this.values()); return this.said; }   // the reader asks for the screen

  // a control pressed (a button) or turned (an encoder or knob: detents, + clockwise); what it did, for the flow view
  // a key down / up (the mock's keys, the FM-1's over USB): a key whose action has `long` acts on release when let go
  // soon (a press) or once held LONG_MS (its long action: Sam, 2026-11-06: press = mute, long press = solo); others act
  // as they go down
  keyDown(control) {
    const a = actionOf(this.data, this.id, control);
    if (!a?.long) return this.press(control);
    this.held[control] = { t: this.now(), fired: false, a };
    return null;
  }
  keyUp(control) {
    const h = this.held[control];
    delete this.held[control];
    return h && !h.fired ? this.press(control, 1, h.a) : null;
  }
  // the live state for Claude (docs/superpowers/plans/2026-11-06-device-screens.md, "Live state"): what plays and what
  // the screen shows, as plain data (the page posts it to the server about once a second)
  state() {
    const l = this.live ?? {}, v = this.values(), drop = new Set(["activity", "secList"]);
    const values = Object.fromEntries(Object.entries(v).filter(([k, x]) => !drop.has(k) && x !== "" && x !== false && x !== undefined));
    return {
      source: "workbench", at: new Date(this.now()).toISOString(), seed: l.seed, playing: !!l.playing, bar: l.bar, bars: l.bars,
      section: v.section, chord: v.chord, screen: this.id, title: this.screen?.title, stack: [...this.stack],
      cursor: this.cursor[this.id] ?? null, values, rows: l.rows?.values ?? {}, rolled: l.rows?.rolled ?? {},
      levels: l.levels ?? {}, mute: l.activity?.mute ?? [], solo: l.activity?.solo ?? null, sections: l.sections ?? [],
      waiting: { changes: Object.fromEntries(Object.entries(this.req).map(([f, q]) => [f, { value: q.val, at: q.at }])),
                 jump: this.jumpReq ? { section: this.jumpReq.name, at: this.jumpReq.at } : null,
                 reroll: this.rollReq ? { part: this.rollReq.part, at: this.rollReq.at } : null },
    };
  }
  // a scrolling list's first line shown (a screen's `rows`: how many lines fit): it follows the highlight
  listOff() {
    const rows = this.screen?.rows;
    if (!rows) return 0;
    const cur = this.cursor[this.id] ?? 0, n = this.live?.sections?.length ?? 0;
    let off = this.off?.[this.id] ?? 0;
    if (cur < off) off = cur;
    if (cur >= off + rows) off = cur - rows + 1;
    off = Math.max(0, Math.min(off, Math.max(0, n - rows)));
    (this.off ??= {})[this.id] = off;
    return off;
  }
  // the keys lit and blinking on this screen (Sam, 2026-11-06), from its `keyLeds`: { lanes: [key a lane] } a part on
  // (not muted, not soloed out) lit, soloed blinking; { sections: [key a section] } the playing section lit, a jump
  // waiting blinking. -> { on: Set, blink: Set } of key numbers (the mock's keys, and the FM-1's over USB)
  keyLights() {
    const on = new Set(), blink = new Set(), k = this.screen?.keyLeds ?? this.data.keyLeds, act = this.live?.activity;
    if (k?.lanes)
      (k.parts ?? this.screen.lanes ?? []).forEach((p, i) => {
        if (k.lanes[i] === undefined) return;
        if (act?.solo === p) blink.add(k.lanes[i]);
        else if (!(act?.solo || act?.mute?.includes(p))) on.add(k.lanes[i]);
      });
    if (k?.sections && this.hasSong()) {
      const secs = this.live?.sections ?? [], bar1 = this.live.bar;
      k.sections.forEach((key, i) => {
        const x = secs[i];
        if (x && bar1 > x.start && bar1 <= x.start + x.bars) on.add(key);
        if (this.jumpReq?.i === i) blink.add(key);
      });
    }
    return { on, blink };
  }
  tick() {   // a held key past LONG_MS: its long action, once
    for (const [c, h] of Object.entries(this.held))
      if (!h.fired && this.now() - h.t >= LONG_MS) { h.fired = true; this.press(c, 1, h.a.long); }
  }

  press(control, detents = 1, a = actionOf(this.data, this.id, control)) {
    if (!a) return null;
    if (a.do === "part" || a.do === "page") {   // ALGO: the next part (data.parts, at its first page); SELECT: the next page
      const list = a.do === "part" ? this.data.parts ?? [] : this.screen.pages ?? [], from = this.id;
      const here = a.do === "part" ? list.findIndex((id) => id === from || this.data.screens[id]?.pages?.includes(from)) : list.indexOf(from);
      if (here < 0 || list.length < 2) return null;
      this.stack[this.stack.length - 1] = list[(((here + detents) % list.length) + list.length) % list.length];
      if (this.id !== from) this.speak();
      return { from, control, to: this.id };
    }
    if (a.do === "jump") {   // the highlighted section: the song goes there (at the next bar line, as the workbench's strip)
      const i = a.i ?? this.cursor[this.id] ?? 0, sec = this.live?.sections?.[i];   // a.i: a key's own section
      if (sec && this.hasSong()) {   // it lands on the next 4-bar boundary, as a knob's change does (Sam, 2026-11-06)
        const l = this.live, bar = l.bar - 1, at = (Math.floor(bar / 4) + 1) * 4;
        if (at < l.bars) {
          this.jumpReq = { start: sec.start, name: sec.name, i, at, sent: false };
          if (bar >= at - 1) this.update(l);
        }
        this.say("GO TO", String(sec.name).toUpperCase());
      }
      return null;
    }
    if (a.do === "reroll" && a.part) {   // SEQ on a part's screen: that part re-rolls (its 🎲), on the 4-bar boundary
      if (this.hasSong()) {
        const l = this.live, bar = l.bar - 1, at = (Math.floor(bar / 4) + 1) * 4, name = this.screen.title?.split(":")[0] ?? a.part;
        if (at < l.bars) {
          this.rollReq = { part: a.part, name: String(name).toUpperCase(), at, sent: false };
          if (bar >= at - 1) this.update(l);
        }
        this.say(String(name).toUpperCase(), "NEW ROLL");
      }
      return null;
    }
    if (a.do === "reroll") {   // SEQ: a new song (Sam, 2026-11-06), as the device's
      this.engine.reroll?.();
      this.say("NEW SONG");
      return null;
    }
    if (a.do === "level") {   // the mixer: a part's level, 1 dB a detent, -12..+6 dB, at once (levels kept across songs)
      const part = a.part ?? this.screen.lanes?.[this.cursor[this.id] ?? 0];
      if (!part) return null;
      const db = Math.max(-12, Math.min(6, (this.live?.levels?.[part] ?? 0) + detents));
      this.engine.level?.(part, db);
      if (this.live) this.live.levels = { ...(this.live.levels ?? {}), [part]: db };
      this.say(`${(part === "keys" ? "comp" : part).toUpperCase()}`, `${db > 0 ? "+" : ""}${db} DB`);
      return null;
    }
    if (a.do === "mute" || a.do === "solo") {   // the highlighted lane's part: muted, or soloed (again: neither); at once
      const part = a.part ?? this.screen.lanes?.[this.cursor[this.id] ?? 0], act = this.live?.activity;
      if (!part || part === "texture") return null;   // (the texture: a level only)
      if (a.do === "mute" && act?.solo === part) { this.engine.solo?.(null); this.say(part.toUpperCase(), "SOLO OFF"); }   // a press on the soloed part: un-solo (Sam, 2026-11-06: not mute)
      else if (a.do === "mute") { const on = !act?.mute?.includes(part); this.engine.mute?.(part, on); this.say(part.toUpperCase(), on ? "MUTED" : "ON"); }
      else { const on = act?.solo !== part; this.engine.solo?.(on ? part : null); this.say(part.toUpperCase(), on ? "SOLO" : "SOLO OFF"); }
      return null;
    }
    if (a.do === "cursor") {   // a menu: the highlight moves (wraps)
      const n = this.screen.menu?.length ?? (this.screen.list === "sections" ? Math.min(this.live?.sections?.length ?? 0, MENU_MAX)
        : this.screen.lanes?.length ?? 0);
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
    } else if (a.do === "turn" || a.do === "nudge") {
      const field = a.byCursor ? a.byCursor[this.cursor[this.id] ?? 0] : a.field;   // byCursor: the highlighted line's
      if (field) this.request(field, a.do === "turn" ? detents * (a.step ?? 1) : a.by);
    }
    return null;
  }

  // a knob's request (app.c's input() and command()): from the value waiting, else the song's; on the next 4-bar
  // boundary (unless the song ends first)
  request(f, d) {
    const def = this.data.fields[f];
    if (!this.hasSong()) return;
    const l = this.live, bar = l.bar - 1, from = this.req[f]?.val ?? fieldValue(def, f, l.roll, l.rows), v = fieldMove(def, from, d, l.rows);
    if (def.now) {   // a switch that acts at once (the loop): no waiting for a 4-bar boundary
      if (v !== from) { this.engine.set(def.row, v); if (l.rows?.values) l.rows.values[def.row] = v; }
      this.say(def.name, fieldText(def, v, l.roll.mode ?? 0, l.rows));
      return;
    }
    // what the song plays now (not a change staged in the workbench): turning back to it undoes the change (Sam,
    // 2026-11-06): no mark, no "IN n BARS"; one already sent to the engine is set back
    const playing = l.roll?.[f] !== undefined ? l.roll[f] : l.rows?.live?.[def.row] ?? l.rows?.values?.[def.row];
    if (v !== from && String(v) === String(playing)) {
      const was = this.req[f];
      delete this.req[f];
      if (was?.sent) this.engine.set(def.row, v);
    } else if (v !== from) {
      const at = (Math.floor(bar / 4) + 1) * 4;
      if (at < l.bars) {
        this.req[f] = { val: v, at, sent: false };
        if (bar >= at - 1) this.update(l);
      }
    }
    this.say(def.name, fieldText(def, v, l.roll.mode ?? 0, l.rows));
  }
}

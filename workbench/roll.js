// SPDX-License-Identifier: GPL-3.0-or-later
// The piano roll (view only): the song as it plays, one lane per part, from wb_events. layout() is pure (tested in
// Node); draw() paints a layout on a canvas.

export const LANES = ["keys", "bass", "lead", "pad", "drums"];
export const BAR_PX = 40, LANE_PX = { drums: 90 }, LANE_DEFAULT_PX = 56;

// events (wb.js readEvents), sections (wb_sections), bars; V: the engine's role numbers (C.V: CHORD, BASS, LEAD, PAD,
// KICK..RIDE); chords (wb.js readChords). -> { bars, sections: [x of each section's start, in bars], chords: [x of
// each chord change], lanes: [{ role: part name, notes: [{ x, w, y,
// h, shade }] }] }. x, w in bars (from 0); y, h as fractions of the lane, 0 at the top (a pitched lane spans its own
// lowest to highest note; a drum lane is one row of the grid); shade 0..1 by velocity. An empty lane has no notes.
export function layout(events, sections, bars, V, chords = []) {
  const part = (r) => (r === V.CHORD ? "keys" : r === V.BASS ? "bass" : r === V.LEAD ? "lead" : r === V.PAD ? "pad" : r >= V.KICK && r <= V.RIDE ? "drums" : null);
  const by = Object.fromEntries(LANES.map((p) => [p, []]));
  for (const e of events) { const p = part(e.role); if (p) by[p].push(e); }
  const rows = V.RIDE - V.KICK + 1;
  return {
    bars,
    sections: sections.map((s) => s.start),
    chords: chords.map((c) => c.bar + c.step / 16),
    lanes: LANES.map((role) => {
      const evs = by[role];
      let lo = 127, hi = 0;
      if (role !== "drums") for (const e of evs) { lo = Math.min(lo, e.note); hi = Math.max(hi, e.note); }
        const span = Math.max(hi - lo, 11) + 1;   // at least an octave tall
      const notes = evs.map((e) => ({
        x: e.bar + e.step / 16, w: Math.max(e.len, 1) / 16,
        y: role === "drums" ? e.note / rows : (hi - e.note) / span, h: role === "drums" ? 1 / rows : 1 / span,
        shade: e.vel / 127,
      }));
      return { role, notes };
    }),
  };
}

const css = (el, v) => getComputedStyle(el).getPropertyValue(v).trim() || "#888";

// canvas: sized here (bars * BAR_PX wide); playhead: the bar sounding (1-based, 0 stopped); listen: { mute:Set, solo }.
// -> the playhead's x in px (null if stopped), for the page to scroll to
export function draw(canvas, lay, playhead, listen) {
  const lanes = lay.lanes, px = (l) => LANE_PX[l.role] || LANE_DEFAULT_PX, W = Math.max(lay.bars, 1) * BAR_PX;
  const H = lanes.reduce((n, l) => n + px(l), 0);
  if (canvas.width !== W) canvas.width = W;
  if (canvas.height !== H) canvas.height = H;
  const g = canvas.getContext("2d"), accent = css(canvas, "--accent"), muted = css(canvas, "--muted"), line = css(canvas, "--line");
  g.clearRect(0, 0, W, H);
  g.fillStyle = css(canvas, "--panel");
  g.fillRect(0, 0, W, H);
  g.strokeStyle = line;
  g.lineWidth = 1;
  g.beginPath();
  for (let b = 0; b <= lay.bars; b++) { g.moveTo(b * BAR_PX + 0.5, 0); g.lineTo(b * BAR_PX + 0.5, H); }
  g.stroke();
  g.strokeStyle = muted;   // a section's start
  g.beginPath();
  for (const s of lay.sections) { g.moveTo(s * BAR_PX + 0.5, 0); g.lineTo(s * BAR_PX + 0.5, H); }
  g.stroke();
  g.strokeStyle = accent;   // a chord change: dotted
  g.setLineDash([2, 3]);
  g.beginPath();
  for (const c of lay.chords) { g.moveTo(c * BAR_PX + 0.5, 0); g.lineTo(c * BAR_PX + 0.5, H); }
  g.stroke();
  g.setLineDash([]);
  let top = 0;
  for (const l of lanes) {
    const h = px(l), off = listen.solo ? listen.solo !== l.role : listen.mute.has(l.role);   // heard or greyed
    g.strokeStyle = line;
    g.beginPath(); g.moveTo(0, top + h - 0.5); g.lineTo(W, top + h - 0.5); g.stroke();
    g.fillStyle = off ? "#9a9a9a" : accent;
    for (const n of l.notes) {
      g.globalAlpha = (off ? 0.3 : 0.35) + 0.65 * n.shade * (off ? 0.5 : 1);
      g.fillRect(n.x * BAR_PX, top + n.y * (h - 2) + 1, Math.max(n.w * BAR_PX - 1, 1.5), Math.max(n.h * (h - 2) - 1, 2));
    }
    g.globalAlpha = 1;
    g.fillStyle = muted;
    g.font = "11px system-ui, sans-serif";
    g.fillText(l.notes.length ? l.role + (off ? " (muted)" : "") : `${l.role} (none)`, 4, top + 12);
    top += h;
  }
  if (!playhead) return null;
  const x = (playhead - 1) * BAR_PX;
  g.fillStyle = accent;
  g.globalAlpha = 0.18;
  g.fillRect(x, 0, BAR_PX, H);
  g.globalAlpha = 1;
  g.fillRect(x, 0, 2, H);
  return x;
}

/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The screens' drawing (render.h): workbench/ui/device.js's drawScreen in C, item for item, on fm1-base's gfx.c (its
   static cv_* and the fonts: this file is part of the unity build, after the platform). A band is drawn into the
   canvas and blitted; what is drawn outside a band is clipped by the canvas. */
#include "render.h"

/* the faces the screens use (screens_gen.h's UI_FONT_TABLE: the others not linked in) */
static const felucca_font_t *const UI_FONTS[] = UI_FONT_TABLE;
#define UI_FONT(i) (UI_FONTS[i] ? UI_FONTS[i] : &FONT_T)

/* JS's Math.round(w * num / den) (floor(x + 0.5)), exactly, num / den clamped to 0..1 (the drawing's widths) */
static int32_t ui_round_frac(int32_t w, int32_t num, int32_t den)
{
    if (den <= 0 || num <= 0) return 0;
    if (num > den) num = den;
    return (2 * w * num + den) / (2 * den);
}
static int32_t ui_frac_of(const ui_bag_t *b, int id, int32_t w)
{
    int den, num = ui_value_num(b, id, &den);
    return ui_round_frac(w, num, den);
}

/* every condition of the item's when holds */
static int ui_holds(const ui_bag_t *b, int when)
{
    int i;
    for (i = 0; i < 3 && UI_WHENS[when][i]; i++)
        if (!ui_true(b, UI_WHENS[when][i])) return 0;
    return 1;
}

/* a template filled: each value's text in place */
static void ui_fill(const ui_bag_t *b, const char *p, char *out, int max)
{
    char v[UI_VALUE_MAX];
    int n = 0;
    for (; *p && n < max - 1; p++) {
        if (*p == UI_TPL_VAL) {
            int id = UI_TPL_ID(p + 1);
            const char *x;
            if (ui_value_kind(id) == UI_VK_NUM) {   /* (the mock writes a number as JS does: integers only here) */
                int den;
                int32_t k = (int32_t)ui_value_num(b, id, &den), d = 1000000000, started = 0;
                if (k < 0) { out[n++] = '-'; k = -k; }
                for (; d && n < max - 1; d /= 10)
                    if (k / d % 10 || started || d == 1) { out[n++] = (char)('0' + k / d % 10); started = 1; }
                p += 2;
                continue;
            }
            for (x = ui_value(b, id, 0, v); *x && n < max - 1; x++) out[n++] = *x;
            p += 2;
        } else
            out[n++] = *p;
    }
    out[n] = 0;
}

static void ui_icon(int icon, int32_t x, int32_t y, uint16_t c)
{
    int j, i;
    for (j = 0; j < UI_ICON_ROWS[icon]; j++)
        for (i = 0; i < 9; i++)
            if (UI_ICONS[icon][j] >> i & 1u) cv_rect(x + i, y + j, 1, 1, c);
}

static void ui_knobs(const ui_bag_t *b, const ui_screen_t *sc, const ui_knob_style_t *k)
{
    char v[UI_VALUE_MAX];
    int i;
    for (i = 0; i < 4; i++) {
        const ui_knob_t *kn = &sc->knobs[i];
        int32_t x = k->x + i * k->dx;
        int f = kn->field, on;
        if (f == UI_NONE) continue;
        cv_text(x, k->label_y, UI_FONT(k->label_font), UI_STR(UI_FIELDS[f].name), UI_COLORS[k->label_color]);
        on = ui_holds(b, k->value_when);
        if (on) {
            int pend = UI_PENDING_COLOR >= 0 && ui_true(b, UV_FIELD(f, UVK_FIELD_PEND));
            cv_text(x, k->value_y, UI_FONT(k->value_font), ui_value(b, UV_FIELD(f, UVK_FIELD_VALUE), 0, v),
                    UI_COLORS[pend ? UI_PENDING_COLOR : k->value_color]);
        }
        if (k->bar_w && on && !UI_FIELDS[f].wrap && UI_FIELDS[f].kind == UI_FK_ROLL && UI_FIELDS[f].format == UI_FMT_NONE) {
            cv_rect(x, k->bar_y, k->bar_w, k->bar_h, UI_COLORS[k->bar_back]);
            cv_rect(x, k->bar_y, ui_frac_of(b, UV_FIELD(f, UVK_FIELD_FRAC), k->bar_w), k->bar_h,
                    UI_COLORS[k->bar_color]);
        }
    }
}

/* the song's map: a segment a section (by its bars), the playing one filled to where the song is */
static void ui_sections(const ui_bag_t *b, const ui_item_t *it)
{
    const snap_t *s = b->s;
    int den;
    int32_t at = it->x, total = 0, bar = (int32_t)ui_value_num(b, UV_BAR, &den), i;
    for (i = 0; i < s->n_secs; i++) total += s->secs[i].bars;
    for (i = 0; i < s->n_secs; i++) {
        int32_t start = s->secs[i].start, bars = s->secs[i].bars;
        int32_t sw = (2 * it->w * bars + (total ? total : 1)) / (2 * (total ? total : 1)) - 2;   /* Math.round */
        int now = bar > start && bar <= start + bars, done = bar > start + bars;
        if (sw < 2) sw = 2;
        cv_rect(at, it->y, sw, it->h, UI_COLORS[done ? it->past : it->back]);
        if (now && ui_true(b, UV_SONG)) {
            int32_t w = ui_round_frac(sw, bar - start, bars);
            cv_rect(at, it->y, w < 1 ? 1 : w, it->h, UI_COLORS[it->color]);
        }
        at += sw + 2;
    }
}

/* JS's Math.round(num / den) for any sign (floor(x + 0.5)), den > 0 */
static int32_t ui_round_q(int32_t num, int32_t den)
{
    int32_t a = 2 * num + den, d = 2 * den;
    return a >= 0 ? a / d : -((-a + d - 1) / d);
}

/* the i-th entry of a |-separated list (UI_STR), into t (at most n - 1 characters): "" past its end */
static const char *ui_nth(const char *list, int i, char *t, int n)
{
    int k = 0;
    for (; *list && i; list++)
        if (*list == '|') i--;
    for (; *list && *list != '|' && k < n - 1; list++) t[k++] = *list;
    t[k] = 0;
    return t;
}

/* a part's look, dimmed when it is muted or soloed out */
static int ui_part_off(const ui_activity_t *act, int p)
{
    if (!act) return 0;
    return act->solo >= 0 ? act->solo != p : (int)(act->mute >> p & 1u);
}

static void ui_part_icon(int p, int32_t x, int32_t y, uint16_t c)
{
    if (UI_PART_LOOK[p].icon != UI_NONE) ui_icon(UI_PART_LOOK[p].icon, x, y, c);
}

/* dim(c, k) with k = 0.45 + 0.55 * vel / 127 (the mock's: brighter for louder), each channel rounded */
static uint16_t ui_dim(uint16_t c, int vel)
{
    int32_t k = 5715 + 55 * vel, r = c >> 11, g = (c >> 5) & 63, bl = c & 31;
    return (uint16_t)(ui_round_q(r * k, 12700) << 11 | ui_round_q(g * k, 12700) << 5 | ui_round_q(bl * k, 12700));
}

/* the Parts roll (the mock's lanes): a lane a part, its notes by time and pitch (a drum lane a row a drum), `window`
   bars across, stepping a bar at a time (the window's first bar a quarter of it before the bar playing); the bar
   playing highlighted, bar lines, section starts; notes played or in this bar in the part's color, those to come
   gray, a muted or soloed-out part dim. Positions are exact fractions (the mock's doubles round alike) */
static void ui_lanes(const ui_bag_t *b, const ui_item_t *it)
{
    const ui_lanes_t *L = &UI_LANE_ITEMS[it->ref];
    const ui_activity_t *act = b->act;
    const ui_list_t *parts = &UI_LISTS[L->parts];
    int32_t x = it->x, y = it->y, w = it->w, rowH = L->row_h, lx = x + L->label, lw = w - (lx - x), i, k;
    int32_t bars = b->s->roll.v[RF_BARS] ? b->s->roll.v[RF_BARS] : 1, span = L->window < bars ? L->window : bars;
    int32_t pos = b->s->bar < 0 ? 0 : b->s->bar, from = pos - span / 4, height = parts->n * rowH - 2;
    const char *keys = UI_STR(L->keys);
    if (from > bars - span) from = bars - span;
    if (from < 0) from = 0;
#define UI_X16(t16) (lx + ui_round_q(((t16) - 16 * from) * lw, 16 * span))   /* X(b) for b in 16ths of a bar */
    if (ui_true(b, UV_SONG)) {                                   /* the bar playing, whole */
        int32_t xa = UI_X16(16 * pos), xb = UI_X16(16 * pos + 16);
        if (xa < lx) xa = lx;
        if (xb > lx + lw) xb = lx + lw;
        cv_rect(xa, y, xb - xa, height, UI_COLORS[UC_LINE]);
    }
    for (k = from; k <= from + span; k++) cv_rect(UI_X16(16 * k), y, 1, height, UI_COLORS[UC_LINE]);   /* bar lines */
    for (k = 0; k < b->s->n_secs; k++) {                         /* section starts */
        int32_t sb = b->s->secs[k].start;
        if (sb > from && sb < from + span) cv_rect(UI_X16(16 * sb), y, 1, height, UI_COLORS[UC_DIM]);
    }
    {
        /* each lane's part, its row, its colors, a pitched lane's lowest and highest note: one pass over the notes for
           all lanes, then one drawing them (a lane's row its own: the same pixels as lane by lane) */
        int lane_of[16], lo[8], hi[8], rspan[8], n;
        int32_t ry0[8];
        uint16_t col[8], dimc = UI_COLORS[UC_DIM];
        int8_t offp[8];
        for (k = 0; k < 16; k++) lane_of[k] = -1;
        for (i = 0; i < parts->n && i < 8; i++) {
            int p = UI_LIST_DATA[parts->at + i];
            char key[8];
            lane_of[p & 15] = i;
            offp[i] = (int8_t)ui_part_off(act, p);
            ry0[i] = y + i * rowH;
            col[i] = UI_COLORS[UI_PART_LOOK[p].color];
            lo[i] = 127;
            hi[i] = 0;
            if (ui_true(b, UV_I(i, UVK_I_SEL))) cv_rect(x - 4, ry0[i], lx - x, rowH - 2, UI_COLORS[UC_LINE]);
            ui_part_icon(p, x, ry0[i] + 4, offp[i] ? dimc : col[i]);
            cv_text(x, ry0[i] + 13, &FONT_T, UI_STR(UI_PART_LOOK[p].name), offp[i] ? dimc : col[i]);
            if (ui_nth(keys, i, key, sizeof key)[0]) cv_text(x + 32, ry0[i] + 2, &FONT_T, key, dimc);   /* its key's letter */
            if (act && act->mute >> p & 1u) cv_text(x + 12, ry0[i] + 2, &FONT_T, "M", UI_COLORS[UC_HI]);
            if (act && act->solo == p) cv_text(x + 12, ry0[i] + 2, &FONT_T, "S", UI_COLORS[UC_HI]);
            cv_rect(lx, ry0[i] + rowH - 2, lw, 1, UI_COLORS[UC_LINE]);
        }
        if (act) {
            for (n = 0; n < act->n; n++) {                       /* a pitched lane spans its lowest to highest note */
                const ui_note_t *e = &act->notes[n];
                int l = e->part < 16 ? lane_of[e->part] : -1;
                if (l < 0 || e->part == UI_PART_DRUMS) continue;
                if (e->note < lo[l]) lo[l] = e->note;
                if (e->note > hi[l]) hi[l] = e->note;
            }
            for (i = 0; i < parts->n && i < 8; i++)
                rspan[i] = (hi[i] - lo[i] > 11 ? hi[i] - lo[i] : 11) + 1;
            for (n = 0; n < act->n; n++) {
                const ui_note_t *e = &act->notes[n];
                int l = e->part < 16 ? lane_of[e->part] : -1;
                int32_t t = 16 * e->bar + e->step, len = e->len ? e->len : 1, x0, x1, ny, nh;
                uint16_t base;
                if (l < 0 || t + len < 16 * from || t > 16 * (from + span)) continue;
                x0 = UI_X16(t);
                x1 = UI_X16(t + len);
                if (x0 < lx) x0 = lx;
                if (x1 > lx + lw) x1 = lx + lw;
                if (e->part == UI_PART_DRUMS) {                  /* a row a drum (V_KICK .. V_RIDE) */
                    ny = ui_round_q(e->note * (rowH - 4), V_RIDE - V_KICK + 1);
                    nh = ui_round_q(rowH - 4, V_RIDE - V_KICK + 1);
                } else {
                    ny = ui_round_q((hi[l] - e->note) * (rowH - 4), rspan[l]);
                    nh = ui_round_q(rowH - 4, rspan[l]);
                }
                base = offp[l] ? dimc : t < 16 * (pos + 1) ? col[l] : UI_COLORS[UC_GRAY];   /* played or this bar */
                cv_rect(x0, ry0[l] + 1 + ny, x1 - x0 - 1 > 1 ? x1 - x0 - 1 : 1, nh > 1 ? nh : 1, ui_dim(base, e->vel));
            }
        }
    }
#undef UI_X16
}

/* the Mixer (the mock's faders): a column a part, its level -12..+6 dB from the bottom, 0 dB marked, the cap, the
   number, M or S under it */
static void ui_faders(const ui_bag_t *b, const ui_item_t *it)
{
    const ui_list_t *parts = &UI_LISTS[UI_LANE_ITEMS[it->ref].parts];
    const ui_activity_t *act = b->act;
    const char *labels = UI_STR(UI_LANE_ITEMS[it->ref].keys), *tags = UI_STR(UI_LANE_ITEMS[it->ref].tags);
    int32_t x = it->x, y = it->y, w = it->w, h = it->h, n = parts->n, cw = w / (n ? n : 1);
    int32_t top = y + (*labels ? 25 : 14), tH = h - (*labels ? 45 : 34);
    int i;
#define UI_AT(db) (top + ui_round_q((6 - (db)) * tH, 18))
    for (i = 0; i < n; i++) {
        int p = UI_LIST_DATA[parts->at + i], off = ui_part_off(act, p), db = act ? act->level[p] : 0, k = 0, m;
        int32_t cx = x + i * cw, mid = cx + cw / 2;
        uint16_t col = UI_COLORS[UI_PART_LOOK[p].color], gray = UI_COLORS[UC_GRAY];
        char t[6], name[12], tag[8];
        const char *ms;
        ui_nth(tags, i, tag, sizeof tag);
        /* highlighted: the picked column, or (the knobs layout) the columns this page's knobs drive */
        if (ui_true(b, UV_I(i, UVK_I_SEL)) || tag[0]) cv_rect(cx + 1, y, cw - 2, h, UI_COLORS[UC_LINE]);
        ui_part_icon(p, mid - 4, y + 2, off ? UI_COLORS[UC_DIM] : col);
        if (ui_nth(labels, i, name, sizeof name)[0])           /* its name under the icon */
            cv_text(mid - text_w(&FONT_T, name) / 2, y + 11, &FONT_T, name, off ? UI_COLORS[UC_DIM] : col);
        cv_rect(mid - 2, top, 4, tH, UI_COLORS[UC_DIM]);                              /* the track */
        cv_rect(mid - 2, UI_AT(db), 4, top + tH - UI_AT(db), off ? gray : col);    /* the level */
        cv_rect(mid - 6, UI_AT(0), 12, 1, UI_COLORS[UC_WHITE]);                    /* 0 dB */
        cv_rect(mid - 7, UI_AT(db) - 1, 14, 3, off ? gray : UI_COLORS[UC_WHITE]);  /* the cap */
        if (db > 0) t[k++] = '+';
        if (db < 0) t[k++] = '-';
        m = db < 0 ? -db : db;
        if (m >= 10) t[k++] = (char)('0' + m / 10);
        t[k++] = (char)('0' + m % 10);
        t[k] = 0;
        cv_text(mid - text_w(&FONT_T, t) / 2, y + h - 18, &FONT_T, t, off ? UI_COLORS[UC_DIM] : UI_COLORS[UC_WHITE]);
        ms = act && act->solo == p ? "S" : act && act->mute >> p & 1u ? "M" : "";
        {                                                       /* under it: its knob, and M or S */
            char both[16];
            int32_t x0, pen;
            both[0] = 0;
            k = 0;
            while (tag[k]) { both[k] = tag[k]; k++; }
            if (tag[0] && *ms) both[k++] = ' ';
            if (*ms) both[k++] = *ms;
            both[k] = 0;
            x0 = mid - text_w(&FONT_T, both) / 2;
            pen = tag[0] ? cv_text(x0, y + h - 8, &FONT_T, tag, gray) : x0;
            if (*ms) cv_text(tag[0] ? pen + text_w(&FONT_T, " ") : x0, y + h - 8, &FONT_T, ms, UI_COLORS[UC_HI]);
        }
    }
#undef UI_AT
}

static int32_t ui_text_item(const ui_bag_t *b, const ui_item_t *it, int32_t pen)
{
    const felucca_font_t *f = UI_FONT(it->font);
    char s[UI_VALUE_MAX * 2];
    int32_t x;
    int col = it->color;
    const ui_lone_t *lone = it->past ? &UI_LONES[it->past - 1] : 0;
    ui_fill(b, UI_STR(it->ref), s, sizeof s);
    x = it->pos == UI_POS_RIGHT ? it->x - text_w(f, s) : it->pos == UI_POS_AFTER ? pen + it->x : it->x;
    if (lone && UI_PENDING_COLOR >= 0 && lone->pend && ui_true(b, lone->pend)) col = UI_PENDING_COLOR;
    pen = cv_text(x, it->y, f, s, UI_COLORS[col]);
    if (lone && lone->color) {            /* "<  >" at the line's right end: an arrow only where the list goes on */
        uint16_t ac = UI_COLORS[lone->color - 1];
        int32_t r = lone->right;
        if (!(lone->last && ui_true(b, lone->last))) cv_text(r - text_w(f, ">"), it->y, f, ">", ac);
        if (!(lone->first && ui_true(b, lone->first))) cv_text(r - text_w(f, ">") - text_w(f, "  <"), it->y, f, "<", ac);
    }
    return pen;
}

/* the panel's pixels as last sent, a sum each strip of UI_STRIP rows (by the screen row it starts on): only the
   strips drawn differently are sent again, a run of them at once (the panel's transfer is a redraw's cost; one late
   audio block in ten was a redraw's, the FM-1, 2026-11-07; the Parts roll's lanes, one tall band, changed a few
   strips a bar but sent them all, 2026-11-08) */
#define UI_STRIP 8u
static uint32_t ui_sum[240];
static uint8_t ui_sum_ok[240];
void ui_draw_all(void) { memset(ui_sum_ok, 0, sizeof ui_sum_ok); }

static uint32_t canvas_sum(uint32_t r0, uint32_t rows)
{
    const uint32_t *p = (const uint32_t *)(cv_px + r0 * cv_w);
    uint32_t h = 2166136261u, i;
    for (i = 0; i < rows * cv_w / 2u; i++)
        h = (h ^ p[i]) * 16777619u;
    return h;
}

/* the canvas's strips that changed, to screen row y */
static void ui_send(uint32_t y)
{
    uint32_t r0, at = cv_h;
    for (r0 = 0; r0 < cv_h; r0 += UI_STRIP) {
        uint32_t k = y + r0, h = canvas_sum(r0, cv_h - r0 < UI_STRIP ? cv_h - r0 : UI_STRIP);
        int same = k < 240u && ui_sum_ok[k] && ui_sum[k] == h;
        if (k < 240u) {
            ui_sum[k] = h;
            ui_sum_ok[k] = 1;
        }
        if (!same && at == cv_h)
            at = r0;
        else if (same && at < cv_h) {
            lcd_blit(0, y + at, cv_w, r0 - at, cv_px + at * cv_w);
            at = cv_h;
        }
    }
    if (at < cv_h)
        lcd_blit(0, y + at, cv_w, cv_h - at, cv_px + at * cv_w);
}

void ui_draw(const ui_screen_t *sc, const ui_bag_t *b, uint32_t band_mask)
{
    int bi, ii;
    for (bi = 0; bi < sc->n_bands; bi++) {
        const ui_band_t *band = &UI_BANDS[sc->bands + bi];
        int32_t pen = 0;
        if (!(band_mask >> bi & 1u)) continue;
        cv_begin(240, band->h, UI_COLORS[band->bg]);
        for (ii = 0; ii < band->n_items; ii++) {
            const ui_item_t *it = &UI_ITEMS[band->first_item + ii];
            if (it->when && !ui_holds(b, it->when)) continue;
            switch (it->kind) {
            case UI_TEXT: pen = ui_text_item(b, it, pen); break;
            case UI_RECT: cv_rect(it->x, it->y, it->w, it->h, UI_COLORS[it->color]); break;
            case UI_ICON: ui_icon(it->ref, it->x, it->y, UI_COLORS[it->color]); break;
            case UI_METER:
                if (it->past) break;              /* a choice's field: no bar */
                if (it->back) cv_rect(it->x, it->y, it->w, it->h, UI_COLORS[it->back]);
                cv_rect(it->x, it->y, ui_frac_of(b, it->ref, it->w), it->h, UI_COLORS[it->color]);
                break;
            case UI_KNOBS: ui_knobs(b, sc, &UI_KNOB_STYLES[it->ref]); break;
            case UI_SECTIONS: ui_sections(b, it); break;
            case UI_LANES: ui_lanes(b, it); break;
            case UI_FADERS: ui_faders(b, it); break;
            default: break;
            }
        }
        ui_send((uint32_t)band->y);
    }
}

/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The screens' drawing (render.h): workbench/ui/device.js's drawScreen in C, item for item, on fm1-base's gfx.c (its
   static cv_* and the fonts: this file is part of the unity build, after the platform). A band is drawn into the
   canvas and blitted; what is drawn outside a band is clipped by the canvas. */
#include "render.h"

static const felucca_font_t *const UI_FONTS[] = {&FONT_T, &FONT_XS, &FONT_S, &FONT_B, &FONT_M, &FONT_L};
#define UI_FONT(i) UI_FONTS[(i) < sizeof UI_FONTS / sizeof UI_FONTS[0] ? (i) : 0]

/* JS's Math.round for the positive and the negative alike: floor(x + 0.5) */
static int32_t ui_round(double x)
{
    double f = (double)(int32_t)x;
    if (f > x) f -= 1.0;                  /* floor */
    return x - f >= 0.5 ? (int32_t)f + 1 : (int32_t)f;
}

static double clamp01(double v) { return v < 0.0 ? 0.0 : v > 1.0 ? 1.0 : v; }

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
                int32_t k = (int32_t)ui_value_num(b, id), d = 1000000000, started = 0;
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
            cv_rect(x, k->bar_y, ui_round(k->bar_w * clamp01(ui_value_num(b, UV_FIELD(f, UVK_FIELD_FRAC)))), k->bar_h,
                    UI_COLORS[k->bar_color]);
        }
    }
}

/* the song's map: a segment a section (by its bars), the playing one filled to where the song is */
static void ui_sections(const ui_bag_t *b, const ui_item_t *it)
{
    const snap_t *s = b->s;
    int32_t at = it->x, total = 0, bar = (int32_t)ui_value_num(b, UV_BAR), i;
    for (i = 0; i < s->n_secs; i++) total += s->secs[i].bars;
    for (i = 0; i < s->n_secs; i++) {
        int32_t start = s->secs[i].start, bars = s->secs[i].bars;
        int32_t sw = ui_round((double)it->w * bars / (total ? total : 1)) - 2;
        int now = bar > start && bar <= start + bars, done = bar > start + bars;
        if (sw < 2) sw = 2;
        cv_rect(at, it->y, sw, it->h, UI_COLORS[done ? it->past : it->back]);
        if (now && ui_true(b, UV_SONG)) {
            int32_t w = ui_round((double)sw * (bar - start) / bars);
            cv_rect(at, it->y, w < 1 ? 1 : w, it->h, UI_COLORS[it->color]);
        }
        at += sw + 2;
    }
}

static int32_t ui_text_item(const ui_bag_t *b, const ui_item_t *it, int32_t pen)
{
    const felucca_font_t *f = UI_FONT(it->font);
    char s[UI_VALUE_MAX * 2];
    int32_t x;
    int col = it->color;
    const uint16_t *lone = it->past ? UI_LONES[it->past - 1] : 0;
    ui_fill(b, UI_STR(it->ref), s, sizeof s);
    x = it->pos == UI_POS_RIGHT ? it->x - text_w(f, s) : it->pos == UI_POS_AFTER ? pen + it->x : it->x;
    if (lone && UI_PENDING_COLOR >= 0 && lone[0] && ui_true(b, lone[0])) col = UI_PENDING_COLOR;
    pen = cv_text(x, it->y, f, s, UI_COLORS[col]);
    if (it->arrows && lone) {             /* "<  >" at the line's right end: an arrow only where the list goes on */
        uint16_t ac = UI_COLORS[it->arrows - 1];
        int32_t r = it->arrows_right;
        if (!(lone[2] && ui_true(b, lone[2]))) cv_text(r - text_w(f, ">"), it->y, f, ">", ac);
        if (!(lone[1] && ui_true(b, lone[1]))) cv_text(r - text_w(f, ">") - text_w(f, "  <"), it->y, f, "<", ac);
    }
    return pen;
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
                cv_rect(it->x, it->y, ui_round(it->w * clamp01(ui_value_num(b, it->ref))), it->h, UI_COLORS[it->color]);
                break;
            case UI_KNOBS: ui_knobs(b, sc, &UI_KNOB_STYLES[it->ref]); break;
            case UI_SECTIONS: ui_sections(b, it); break;
            default:                              /* lanes, faders: a later phase */
                cv_text(it->x + 4, it->y + 4, &FONT_T, "NOT YET", UI_COLORS[UC_DIM]);
                break;
            }
        }
        cv_blit(0, band->y);
    }
}

/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The screens' values (values.h): workbench/ui/device.js's valuesOf, with the page's rows and labels (index.html's
   rows() and labels(), rows.js's optionsFor, ui/rolled.js's rolledNames) as the device has them. No libc beyond
   <string.h>'s three (the firmware's toolchain is freestanding). */
#include <string.h>
#include "values.h"
#include "screens_gen.h"
#include "choices_gen.h"
#include "harmony.h"
#include "library.h"
#include "patterns_gen.h"
#include "plan.h"
#include "theory.h"
#include "override.h"

/* app.c's words (the mock's KEY_NAME ...): each mode's keys spelled by its parent major */
static const char UI_KEY_NAME[MODE_N][12][3] = {   /* (characters, not pointers: half the flash) */
    {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"},
    {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"},
    {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "Bb", "B"},
    {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"},
    {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"},
    {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"},
};
static const char *const UI_MODE_NAME[MODE_N] = {"MAJOR", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN", "MINOR"};
static const char *const UI_MOOD_NAME[4] = {"SAD", "WISTFUL", "WARM", "SUNNY"};
static const char *const UI_SEC_NAME[5] = {"INTRO", "GROOVE", "LEAD", "BREAK", "OUTRO"};
static const char UI_SEC_KEYS[] = "FGABCDEFGA";   /* the white keys from F3, a section each (the Sections screen) */
enum { MENU_MAX = 10, OPTS_MAX = UI_OPTS_MAX };

/* ------------------------------------------------------------------------------------------------- text --- */
static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static char *cat(char *t, const char *s)        /* appends, bounded by UI_VALUE_MAX */
{
    int n = slen(t);
    while (*s && n < UI_VALUE_MAX - 1) t[n++] = *s++;
    t[n] = 0;
    return t;
}
static char *cat_int(char *t, int v)
{
    char d[12];
    int i = 11;
    unsigned u = v < 0 ? (unsigned)-v : (unsigned)v;
    d[i] = 0;
    do d[--i] = (char)('0' + u % 10); while ((u /= 10) && i > 1);
    if (v < 0) d[--i] = '-';
    return cat(t, d + i);
}
static char *upper_cat(char *t, const char *s)
{
    int n = slen(t);
    for (; *s && n < UI_VALUE_MAX - 1; s++)
        t[n++] = *s >= 'a' && *s <= 'z' ? (char)(*s - 32) : *s;
    t[n] = 0;
    return t;
}

/* ------------------------------------------------------------------------------------------- the rows --- */
static int row_of_field(int f) { return UI_FIELDS[f].row; }
static int value_of_row(const ui_bag_t *b, int row) { return b->s->rows[row]; }
static int is_rhythm(int row) { return row == ROW_RHYTHM; }
/* the row a field's value lives in: the rhythm row is the comp's or the strum patterns' (rows.js's rhythmRow) */
static int real_row(const ui_bag_t *b, int row)
{
    return is_rhythm(row) ? ch_rhythm_row(b->s->rows[ROW_KEYS], b->s->rows[ROW_COMP]) : row;
}

/* a library row's choices the tables don't hold: the arrangement maps, the progressions (rows.js's maps and progs) */
static int lib_count(int row)
{
    if (row == ROW_MAP) return LIB_NMAPS;
    if (row == ROW_PROG) return LIB_NPROGS;
    return 0;
}
static int lib_value(int row, int i) { return row == ROW_MAP ? LIB_MAPS[i].id : LIB_PROGS[i].id; }
static int lib_status(int row, int i) { return row == ROW_MAP ? 0 : LIB_PROGS_STATUS[i] == 1 ? 0 : LIB_PROGS_STATUS[i] == 2 ? 2 : 1; }

/* a choice's name as the device writes it, without "as rolled"'s: the tables', a map's name, a progression's id;
   NULL for none */
static const char *choice_name(int row, int v, char *buf)
{
    const char *n = ch_name(row, v);
    if (n) return n;
    if (row == ROW_MAP) {
        const lib_map_t *m = lib_map_by_id((uint8_t)v);
        if (!m) return 0;
        buf[0] = 0;
        return upper_cat(buf, m->name);
    }
    if (row == ROW_PROG) {                         /* rows.js's: "#17 " and its source, shortened */
        const lib_prog_t *p = lib_prog_by_id((uint8_t)v);
        const char *src, *a;
        char w[UI_VALUE_MAX];
        int i = 0;
        if (!p) return 0;
        src = p->src ? p->src : "";
        if (!memcmp(src, "imported from ", 14)) {
            for (a = src; *a; a++)
                if (*a == '/') src = a + 1;
        } else {
            for (a = src; *a && *a != ':' && *a != ','; a++) {}
            if (*a) {
                const char *d;
                src = a + 1;
                while (*src == ' ') src++;
                for (d = src; *d >= '0' && *d <= '9'; d++) {}
                if (d > src && *d == ',') {         /* rows.js's optional "17, " */
                    src = d + 1;
                    while (*src == ' ') src++;
                }
            }
        }
        for (; src[i] && src[i] != ';' && i < UI_VALUE_MAX - 1; i++) w[i] = src[i];
        w[i] = 0;
        buf[0] = 0;
        cat(buf, "#");
        cat_int(buf, v);
        cat(buf, " ");
        return upper_cat(buf, w);
    }
    return 0;
}

/* a kit row value the device plays: an engine kit, or a shipped one (song.h SONG_KIT_PRESET..; a sampled one only
   with the sample bank, as index.html leaves out an unavailable one) */
static int kit_ok(int v)
{
    return (v >= 0 && v < KIT_N) || (v >= SONG_KIT_PRESET && v < SONG_KIT_PRESET + SONG_KIT_PRESETS &&
                                     (!SONG_KIT_IS_SAMPLED(v - SONG_KIT_PRESET) || lib_samples()));
}

/* rows.js's optionsFor, filtered as index.html's rows() does: the choices the device offers, in order (retired,
   removed and hidden ones left out, the comp's by what the comp instrument plays, the kits the device can't play
   left out), the value playing appended when it isn't among them. -> the count; values into out */
static int options(const ui_bag_t *b, int row, int *out)
{
    int n = 0, i, cur = value_of_row(b, real_row(b, row)), have = 0, count;
    row = real_row(b, row);
    count = lib_count(row) ? lib_count(row) : ch_count(row);
    for (i = 0; i < count && n < OPTS_MAX - 1; i++) {
        int v, st, f;
        if (lib_count(row)) {
            v = lib_value(row, i);
            st = lib_status(row, i);
            f = 0;
        } else {
            v = ch_value(row, i);
            f = ch_flags(row, v);
            st = CH_STATUS(f);
        }
        if (row == ROW_COMP && (st == 3 || !ch_fits(row, v, b->s->rows[ROW_KEYS]))) continue;
        if (row == ROW_KIT && !kit_ok(v)) continue;
        if (row == ROW_PROG) {                                       /* legal for the song (the mood, the mode) */
            song_roll_t t = b->s->roll;
            if (roll_set(&t, RF_PROG, v) != v) continue;
        }
        if (st >= 2 || (f & CH_HIDDEN)) continue;                    /* (the value playing: appended below) */
        if (v == cur) have = 1;
        out[n++] = v;
    }
    if (!have && !(row == ROW_KIT && !kit_ok(cur)))
        out[n++] = cur;
    return n;
}

static int index_of(const int *o, int n, int v)
{
    int i;
    for (i = 0; i < n; i++)
        if (o[i] == v) return i;
    return -1;
}

/* ui/rolled.js's rolledNames: what an "as rolled" row plays, by row; NULL where it names nothing */
static const char *note_name(int m, char *t)
{
    static const char *const NOTE[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    cat(t, NOTE[m % 12]);
    return cat_int(t, m / 12 - 1);
}
static const char *rolled_name(const ui_bag_t *b, int row, char *t)
{
    static const char *const OFF_ON[2] = {"OFF", "ON"};
    static const char *const PLACE[3] = {"BELOW THE LEAD", "WITH THE LEAD", "ABOVE THE LEAD"};
    static const char *const FILL[2] = {"STEP BACK", "DRIVE"};
    static const char *const BASS[3] = {"ROOTS", "WALKING", "PEDAL"};
    static const char *const RIDE[3] = {"NONE", "THROUGHOUT", "ONE SECTION"};
    const song_rolled_t *x = &b->rolled;
    if (is_rhythm(row))
        row = real_row(b, row) == ROW_COMP ? ROW_COMP : ROW_STRUM_PAT;
    t[0] = 0;
    switch (row) {
    case ROW_COMP: return ch_name(ROW_COMP, x->comp_pat >= 0 ? 100 + x->comp_pat : x->comp);
    case ROW_STRUM_PAT: return ch_name(ROW_STRUM_PAT, x->strum_pat);
    case ROW_LEAD_STYLE: return ch_name(ROW_LEAD_STYLE, x->lead_style + 2);
    case ROW_PAD_ON: return x->pad_on >= 0 && x->pad_on < 2 ? OFF_ON[x->pad_on] : 0;
    case ROW_PAD_PLACE: return x->pad_place >= 0 && x->pad_place < 3 ? PLACE[x->pad_place] : 0;
    case ROW_STRUM_FILL: return x->strum_fill >= 0 && x->strum_fill < 2 ? FILL[x->strum_fill] : 0;
    case ROW_LEAD_LOOP: return x->lead_loop >= 0 && x->lead_loop < 2 ? OFF_ON[x->lead_loop] : 0;
    case ROW_BASS_LINE: return x->bass_style >= 0 && x->bass_style < 3 ? BASS[x->bass_style] : 0;
    case ROW_RIDE: return x->ride >= 0 && x->ride < 3 ? RIDE[x->ride] : 0;
    case ROW_KEYS_OCT: return x->keys_high ? "HIGH" : "UNDER THE LEAD";
    case ROW_LEAD_OCT:
        if (x->lead_lo <= 0 || x->lead_hi <= 0) return 0;
        note_name(x->lead_lo, t);
        cat(t, "-");
        return note_name(x->lead_hi, t);
    default: return 0;
    }
}

/* a field's text for value v (device.js's fieldText): an options field the choice's name ("AS ROLLED (X)" where it
   plays X), a re-roll ORIGINAL / AS ROLLED (X) / VARIANT n, else app.c's value_text */
static const char *field_text(const ui_bag_t *b, int f, int v, char *t)
{
    const ui_field_t *d = &UI_FIELDS[f];
    char rn[UI_VALUE_MAX], nb[UI_VALUE_MAX];
    t[0] = 0;
    if (d->kind == UI_FK_OPTIONS) {
        int row = real_row(b, d->row);
        const char *n = choice_name(row, v, nb), *r;
        int fl = ch_flags(row, v);
        if (!n) return cat_int(t, v);                    /* a stale or unknown value: its number */
        cat(t, n);
        if (fl >= 0 && (fl & CH_ROLLED) && (r = rolled_name(b, d->row, rn)) != 0) {
            cat(t, " (");
            cat(t, r);
            cat(t, ")");
        }
        return t;
    }
    if (d->format == UI_FMT_VARIANT) {
        const char *what = v == value_of_row(b, d->row) ? rolled_name(b, d->row, rn) : 0;
        if (v) {
            cat(t, "VARIANT ");
            cat_int(t, v);
        } else
            cat(t, what ? "AS ROLLED" : "ORIGINAL");
        if (what) {
            cat(t, " (");
            cat(t, what);
            cat(t, ")");
        }
        return t;
    }
    if (d->format == UI_FMT_KEY) return cat(t, UI_KEY_NAME[b->s->roll.v[RF_MODE] % MODE_N][((v % 12) + 12) % 12]);
    if (d->format == UI_FMT_MOOD) return cat(t, UI_MOOD_NAME[v & 3]);
    if (d->format == UI_FMT_MODE) return cat(t, UI_MODE_NAME[((v % MODE_N) + MODE_N) % MODE_N]);
    return cat_int(t, v);
}

static int field_value(const ui_bag_t *b, int f) { return value_of_row(b, real_row(b, row_of_field(f))); }
static int waiting(const ui_bag_t *b, int f) { return b->n && b->n->req_at[f] >= 0; }
static int field_shown(const ui_bag_t *b, int f) { return waiting(b, f) ? b->n->req_val[f] : field_value(b, f); }

/* a field's share of its range, as a fraction num / *den (the device has no double: the drawing rounds it exactly) */
static int field_frac(const ui_bag_t *b, int f, int v, int *den)
{
    const ui_field_t *d = &UI_FIELDS[f];
    *den = 1;
    if (d->kind == UI_FK_OPTIONS) {
        int o[OPTS_MAX], n = options(b, d->row, o), i = index_of(o, n, v);
        if (n > 1 && i >= 0) *den = n - 1;
        return n > 1 && i >= 0 ? i : 0;
    }
    if (d->wrap) {
        *den = d->wrap - 1;
        return v;
    }
    *den = (d->max ? d->max : 100) - d->min;
    return v - d->min;
}

/* a list's ends (device.js): {field}First / {field}Last; a wrap has neither */
static int field_end(const ui_bag_t *b, int f, int last)
{
    const ui_field_t *d = &UI_FIELDS[f];
    int v = field_shown(b, f);
    if (d->wrap) return 0;
    if (d->kind == UI_FK_OPTIONS) {
        int o[OPTS_MAX], n = options(b, d->row, o), i = index_of(o, n, v);
        return last ? i < 0 || i >= n - 1 : i <= 0;
    }
    return last ? v >= d->max : v <= d->min;
}

/* index.html's labels(): a row's shown name (a tile's {kit}): its choice's name ("as rolled": what plays) */
static const char *label(const ui_bag_t *b, int row, char *t)
{
    char nb[UI_VALUE_MAX], rn[UI_VALUE_MAX];
    int r = real_row(b, row), v = value_of_row(b, r), fl;
    const char *n = choice_name(r, v, nb), *x;
    t[0] = 0;
    if (row == ROW_PAD) {                                   /* the pad off (rolled or set): "OFF", not its instrument
                                                               (Sam, 2026-11-10) */
        const char *o = label(b, ROW_PAD_ON, rn);
        if (o[0] == 'O' && o[1] == 'F' && o[2] == 'F' && !o[3]) return cat(t, "OFF");
    }
    fl = ch_flags(r, v);
    if (fl >= 0 && (fl & CH_ROLLED) && (x = rolled_name(b, row, rn)) != 0) return cat(t, x);
    if (n) return cat(t, n);
    return cat_int(t, v);
}

/* ----------------------------------------------------------------------------------------- the bag --- */
static int find_field(int value_id, int *k)
{
    if (value_id < UV_FIELD0 || value_id >= UV_FIELD(UV_FIELD_COUNT, 0)) return -1;
    *k = (value_id - UV_FIELD0) % UVK_FIELD_N;
    return (value_id - UV_FIELD0) / UVK_FIELD_N;
}

void ui_values(const snap_t *s, const ui_nav_t *nav, ui_bag_t *b)
{
    int f, best = -1;
    b->s = s;
    b->n = nav;
    b->act = 0;
    b->screen = nav && nav->depth ? nav->stack[nav->depth - 1] : UI_HOME;
    song_rolled(&s->roll, &b->rolled);
    for (f = 0; nav && f < UI_N_FIELDS; f++)       /* the earliest change waiting; a tie: the first one made */
        if (nav->req_at[f] >= 0 && (best < 0 || nav->req_at[f] < nav->req_at[best] ||
                                    (nav->req_at[f] == nav->req_at[best] && nav->req_seq[f] < nav->req_seq[best])))
            best = f;
    b->pend_field = best;
    b->pend_bars = best >= 0 ? nav->req_at[best] - s->bar : 0;
}

int ui_value_kind(int id)
{
    int k;
    switch (id) {
    case UV_SONG: case UV_STOPPED: case UV_MSG: case UV_READY: case UV_KNOWN: case UV_LOOPING: return UI_VK_BOOL;
    case UV_BAR: case UV_BARS: case UV_SEC_COUNT: case UV_PROGRESS: case UV_SPIN: case UV_FADE: return UI_VK_NUM;
    default: break;
    }
    if (id >= UV_I0 && id < UV_I(UV_I_COUNT, 0)) {
        k = (id - UV_I0) % UVK_I_N;
        return k == UVK_I_SEL || k == UVK_I_SEC_NOW || k == UVK_I_SEC_PEND ? UI_VK_BOOL
               : k == UVK_I_SEC_BARS ? UI_VK_NUM : UI_VK_STR;
    }
    {
        int f = find_field(id, &k);
        if (f >= 0)
            return k == UVK_FIELD_FRAC ? UI_VK_NUM : k == UVK_FIELD_VALUE || (k == UVK_FIELD_PEND && UI_FIELDS[f].as_row)
                   ? UI_VK_STR : UI_VK_BOOL;
    }
    return UI_VK_STR;
}

static const char *pending_text(const ui_bag_t *b, char *t)
{
    const ui_nav_t *n = b->n;
    char v[UI_VALUE_MAX];
    int at = b->pend_field >= 0 ? n->req_at[b->pend_field] : 0, bars = 0, which = 0;
    t[0] = 0;
    if (!b->s->has_song || !n) return t;
    if (b->pend_field >= 0) which = 1, bars = b->pend_bars;
    if (n->roll_part >= 0 && (b->pend_field < 0 || n->roll_at < at) && (n->jump_i < 0 || n->roll_at <= n->jump_at))
        which = 2, bars = n->roll_at - b->s->bar;
    if (n->jump_i >= 0 && (b->pend_field < 0 || n->jump_at < at) && (n->roll_part < 0 || n->jump_at < n->roll_at))
        which = 3, bars = n->jump_at - b->s->bar;
    if (which == 1) {
        cat(t, UI_STR(UI_FIELDS[b->pend_field].name));
        cat(t, " ");
        cat(t, field_text(b, b->pend_field, n->req_val[b->pend_field], v));
    } else if (which == 2) {
        const char *title = UI_STR(UI_SCREENS[n->roll_screen].title);
        char w[UI_VALUE_MAX];
        int i;
        for (i = 0; title[i] && title[i] != ':' && i < UI_VALUE_MAX - 1; i++) w[i] = title[i];
        w[i] = 0;
        upper_cat(t, w);
        cat(t, ": NEW ROLL");
    } else if (which == 3) {
        cat(t, "GO TO ");
        if (n->jump_i < b->s->n_secs) upper_cat(t, plan_section_name(b->s->secs[n->jump_i].section));
    } else
        return t;
    cat(t, " IN ");
    cat_int(t, bars);
    return cat(t, bars == 1 ? " BAR" : " BARS");
}

static int list_off(const ui_bag_t *b) { return b->n ? b->n->off[b->screen] : 0; }   /* (ui_list_off keeps it) */
/* a list's lines shown: the screen's rows, at most MENU_MAX */
static int list_rows(const ui_bag_t *b)
{
    int r = UI_SCREENS[b->screen].rows;
    return r && r < MENU_MAX ? r : MENU_MAX;
}

static const char *fill_line(const ui_bag_t *b, int line, char *t);

const char *ui_value(const ui_bag_t *b, int id, int index, char *t)
{
    const snap_t *s = b->s;
    const ui_nav_t *n = b->n;
    int song = s->has_song, known = song || s->ready, k, f;
    char v[UI_VALUE_MAX];
    (void)index;
    t[0] = 0;
    switch (id) {
    case UV_APP: return "VNL-1";
    case UV_SONG: return song ? "1" : "";
    case UV_STOPPED: return s->stopped ? "1" : "";
    case UV_IDLE: return s->stopped ? "STOPPED" : "NEXT SONG";
    case UV_MSG: return n && n->msg_until && n->now < n->msg_until ? "1" : "";
    case UV_MSG0: case UV_MSG1:
        return n && n->msg_until && n->now < n->msg_until ? n->msg[id == UV_MSG1] : "";
    case UV_MAP: {
        const lib_map_t *m = lib_map_by_id((uint8_t)s->roll.v[RF_MAP]);
        return m ? upper_cat(t, m->name) : "";
    }
    case UV_SECTION: return s->section >= 0 && s->section < 5 ? UI_SEC_NAME[s->section] : "";
    case UV_BAR: return cat_int(t, s->bar + 1);
    case UV_BARS: return cat_int(t, s->roll.v[RF_BARS]);
    case UV_CHORD: {
        const lib_prog_t *p = lib_prog_by_id((uint8_t)s->roll.v[RF_PROG]);
        int pcs[5], np, loop = p && p->n * p->bars_per_chord ? p->n * p->bars_per_chord : 4;
        if (!known && !s->held) return "";
        np = harmony_chord_at(&s->roll, s->bar, s->bar / loop + 1, (s->flags & PB_DARK) != 0, pcs);
        if (np > 0) th_chord_name(pcs, np, s->roll.v[RF_KEY], s->roll.v[RF_MODE], t);
        return t;
    }
    case UV_PENDING: return pending_text(b, t);
    case UV_RHYTHM: break;                                   /* a row's label: below */
    case UV_PROGRESS: return "";                             /* (a number: ui_value_num) */
    case UV_PAGE: {
        const ui_screen_t *sc = &UI_SCREENS[b->screen];
        const ui_list_t *L = &UI_LISTS[sc->pages];
        int i;
        if (L->n < 2) return "";
        for (i = 0; i < L->n && UI_LIST_DATA[L->at + i] != b->screen; i++) {}
        cat_int(t, i + 1);
        cat(t, "/");
        return cat_int(t, L->n);
    }
    case UV_READY: return s->ready ? "1" : "";
    case UV_KNOWN: return known ? "1" : "";
    case UV_SEC_COUNT: return cat_int(t, s->n_secs);
    case UV_SPIN: return cat_int(t, n ? n->spin : 0);
    case UV_FADE: return cat_int(t, n ? n->fade : 0);
    case UV_LOOPING: return s->rows[ROW_LOOP] == 1 ? "1" : "";
    case UV_SEC_RANGE: {                                    /* "1-7 OF 9": a list longer than its lines */
        int rows = UI_SCREENS[b->screen].rows ? UI_SCREENS[b->screen].rows : MENU_MAX, off = list_off(b);
        if (s->n_secs <= rows) return "";
        cat_int(t, off + 1);
        cat(t, "-");
        cat_int(t, off + rows < s->n_secs ? off + rows : s->n_secs);
        cat(t, " OF ");
        return cat_int(t, s->n_secs);
    }
    default: break;
    }
    if (id >= UV_I0 && id < UV_I(UV_I_COUNT, 0)) {        /* sel{i}, sec{i}... */
        const ui_screen_t *sc = &UI_SCREENS[b->screen];
        int i = (id - UV_I0) / UVK_I_N, off = list_off(b), at = off + i;
        k = (id - UV_I0) % UVK_I_N;
        if (k == UVK_I_SEL) {
            int has = UI_LISTS[sc->menu].n || sc->list || UI_LISTS[sc->lanes].n;
            return has && n && n->cursor[b->screen] - off == i ? "1" : "";
        }
        if (at >= s->n_secs || i >= list_rows(b)) return "";
        switch (k) {
        case UVK_I_SEC:
            if (n && n->jump_i == at) cat(t, "*");
            return upper_cat(t, plan_section_name(s->secs[at].section));
        case UVK_I_SEC_BARS: return cat_int(t, s->secs[at].bars);
        case UVK_I_SEC_NOW:
            return song && s->bar + 1 > s->secs[at].start && s->bar + 1 <= s->secs[at].start + s->secs[at].bars ? "1" : "";
        case UVK_I_SEC_PEND: return n && n->jump_i == at ? "1" : "";
        default: {                                          /* sec{i}Key */
            char c[2] = {at < 10 ? UI_SEC_KEYS[at] : 0, 0};
            return cat(t, c);
        }
        }
    }
    if ((f = find_field(id, &k)) >= 0) {
        int w = waiting(b, f), val = field_shown(b, f);
        switch (k) {
        case UVK_FIELD_VALUE:
            if (w) cat(t, "*");
            return cat(t, field_text(b, f, val, v));
        case UVK_FIELD_FRAC: return "";                      /* (a number: ui_value_num) */
        case UVK_FIELD_PEND:                                 /* (named as its row: the row's "*X") */
            return !w ? "" : UI_FIELDS[f].as_row ? ui_value(b, UV_ROW(UI_FIELDS[f].row, UVK_ROW_PEND), 0, t) : "1";
        case UVK_FIELD_FIRST: return field_end(b, f, 0) ? "1" : "";
        default: return field_end(b, f, 1) ? "1" : "";
        }
    }
    if (id >= UV_ROW0 && id < UV_ROW(UV_ROW_COUNT, 0)) {   /* {row}, {row}Pend */
        int row = (id - UV_ROW0) / UVK_ROW_N;
        k = (id - UV_ROW0) % UVK_ROW_N;
        for (f = 0; f < UI_N_FIELDS; f++)                   /* a field of that row waiting: its value, marked */
            if (UI_FIELDS[f].row == row && waiting(b, f)) {
                const char *x = field_text(b, f, n->req_val[f], v);
                int L = slen(x);
                cat(t, "*");
                if (L > 11 && !memcmp(x, "AS ROLLED (", 11) && x[L - 1] == ')') {   /* "AS ROLLED (X)": X */
                    v[L - 1] = 0;
                    return cat(t, x + 11);
                }
                return cat(t, x);
            }
        if (k == UVK_ROW_PEND) return "";
        return label(b, row, t);
    }
    if (id == UV_RHYTHM) {
        for (f = 0; f < UI_N_FIELDS; f++)
            if (UI_FIELDS[f].row == ROW_RHYTHM && waiting(b, f))
                return ui_value(b, UV_ROW(ROW_RHYTHM, UVK_ROW_PEND), 0, t);
        return label(b, ROW_RHYTHM, t);
    }
    if (id >= UV_LINE0 && id < UV_LINE(UV_LINE_COUNT, 0)) return fill_line(b, (id - UV_LINE0) / UVK_LINE_N, t);
    return "";
}

/* a template filled (the lines: keyline, place) */
static const char *fill_line(const ui_bag_t *b, int line, char *t)
{
    const char *p = UI_STR(UI_LINES[line]);
    char v[UI_VALUE_MAX];
    t[0] = 0;
    for (; *p; p++) {
        if (*p == UI_TPL_VAL) {
            int id = UI_TPL_ID(p + 1);
            cat(t, ui_value(b, id, 0, v));
            p += 2;
        } else {
            char c[2] = {*p, 0};
            cat(t, c);
        }
    }
    return t;
}

int ui_value_num(const ui_bag_t *b, int id, int *den)
{
    const snap_t *s = b->s;
    int f, k;
    char t[UI_VALUE_MAX];
    *den = 1;
    if (id == UV_SPIN) return b->n ? b->n->spin : 0;
    if (id == UV_FADE) return b->n ? b->n->fade : 0;
    if (id == UV_PROGRESS) {
        if (!s->has_song || !s->roll.v[RF_BARS]) return 0;
        *den = s->roll.v[RF_BARS];
        return s->bar + 1 < *den ? s->bar + 1 : *den;
    }
    if ((f = find_field(id, &k)) >= 0 && k == UVK_FIELD_FRAC)
        return field_frac(b, f, field_shown(b, f), den);
    {
        const char *x = ui_value(b, id, 0, t);
        int v = 0, neg = *x == '-';
        for (x += neg; *x >= '0' && *x <= '9'; x++) v = v * 10 + (*x - '0');
        return neg ? -v : v;
    }
}

int ui_true(const ui_bag_t *b, int cond)
{
    int id = cond < 0 ? -cond : cond, on;
    char t[UI_VALUE_MAX];
    if (!id) return 1;
    if (ui_value_kind(id) == UI_VK_NUM)
    {
        int den;
        on = ui_value_num(b, id, &den) != 0;
    }
    else
        on = ui_value(b, id, 0, t)[0] != 0;
    return cond < 0 ? !on : on;
}

/* ------------------------------------------------------------------------------------- the render side --- */
void ui_song_rows(const song_roll_t *r, int16_t rows[ROW_N])
{
    const lib_set_t *set = lib_set_by_id((uint8_t)r->v[RF_SET]);
    const song_ov_t *o = song_ov();
    int i;
    for (i = 0; i < ROW_N; i++) {
        int sr = CH_SONG_ROW[i], v = 0;
        if (sr >= SR_ROLL && sr < SR_ROLL + RF_N)
            v = r->v[sr - SR_ROLL];
        else if (sr >= SR_OV && sr < SR_OV + OV_N)
            v = song_ov_get(o, sr - SR_OV);
        else if (sr == SR_KIT)
            v = lib_song_kit(r);
        else if (sr >= SR_ROLE && sr < SR_OV && set) {
            switch (sr - SR_ROLE) {
            case V_CHORD: v = set->chords; break;
            case V_BASS: v = set->bass; break;
            case V_LEAD: v = set->lead; break;
            case V_PAD: v = set->id == LIB_SET_WB ? set->pad : lib_song_pad(r); break;
            default: break;
            }
        }
        rows[i] = (int16_t)v;
    }
    if (o->comp_pat > 0) rows[ROW_COMP] = (int16_t)(100 + o->comp_pat - 1);   /* a comp rhythm: the comp row's 100 + */
    if (rows[ROW_KICK] < 0) rows[ROW_KICK] = (int16_t)(lib_song_kick(r) + 1);   /* rolled: the phrase it plays, as
                                                                                   the workbench shows it */
    rows[ROW_RHYTHM] = rows[ch_rhythm_row(rows[ROW_KEYS], rows[ROW_COMP]) == ROW_COMP ? ROW_COMP : ROW_STRUM_PAT];
}

void ui_snap_sections(snap_t *s)
{
    song_plan_t p;
    int i;
    if (s->n_secs || !s->ready) return;
    plan_build(&s->roll, &p);
    for (i = 0; i < p.n && i < SNAP_SECS; i++) {
        s->secs[i].start = p.b[i].start;
        s->secs[i].bars = p.b[i].bars;
        s->secs[i].section = p.b[i].section;
    }
    s->n_secs = (uint8_t)i;
}

/* for the navigator: a field's text for v, its value playing, its choices */
const char *ui_field_text(const ui_bag_t *b, int f, int v, char *t) { return field_text(b, f, v, t); }
int ui_field_now(const ui_bag_t *b, int f) { return field_value(b, f); }
int ui_field_options(const ui_bag_t *b, int f, int *out) { return options(b, UI_FIELDS[f].row, out); }

/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The navigator (nav.h): workbench/ui/device.js's Device class, rule for rule (update, press, keyDown / keyUp /
   tick, request, keyLights, listOff, say / speak). Comments name the mock's rule where it isn't plain. */
#include <string.h>
#include "nav.h"
#include "values.h"
#include "choices_gen.h"

enum { NAV_MENU_MAX = 10 };

static int nav_id(const ui_nav_t *n) { return n->stack[n->depth - 1]; }
static const ui_screen_t *nav_screen(const ui_nav_t *n) { return &UI_SCREENS[nav_id(n)]; }
static int has_song(const ui_nav_t *n) { return n->live && n->live->has_song; }
static int bar_now(const ui_nav_t *n) { return n->live ? n->live->bar : 0; }
static int bars_of(const ui_nav_t *n) { return n->live ? n->live->roll.v[RF_BARS] : 0; }
static int boundary(int bar) { return (bar / 4 + 1) * 4; }   /* the next 4-bar boundary */
static const ui_list_t *list(int i) { return &UI_LISTS[i]; }
static int list_at(int l, int i) { return UI_LIST_DATA[UI_LISTS[l].at + i]; }

static void copy(char *d, const char *s, int max)        /* bounded, NUL ended */
{
    int i;
    for (i = 0; s[i] && i < max - 1; i++) d[i] = s[i];
    d[i] = 0;
}
static void upper(char *d, const char *s, int max)
{
    int i;
    for (i = 0; s[i] && i < max - 1; i++) d[i] = s[i] >= 'a' && s[i] <= 'z' ? (char)(s[i] - 32) : s[i];
    d[i] = 0;
}
static int join(char *d, int at, const char *s, int max)
{
    while (*s && at < max - 1) d[at++] = *s++;
    d[at] = 0;
    return at;
}

/* app_say: the header for 1.3 s (each half at most 23 characters), and the screen reader */
static void say(ui_nav_t *n, const char *a, const char *b)
{
    int k;
    copy(n->msg[0], a, UI_MSG);
    copy(n->msg[1], b, UI_MSG);
    n->msg_until = n->now + 1300u;
    if (!n->msg_until) n->msg_until = 1;
    k = join(n->said, 0, a, UI_SAID);
    if (*b) {
        k = join(n->said, k, " ", UI_SAID);
        join(n->said, k, b, UI_SAID);
    }
}

/* the bag of the screen shown, for the text of a value (the navigator's own state read as the mock's shown() is) */
static void bag(ui_nav_t *n, ui_bag_t *b)
{
    static snap_t none;                  /* (no song seen yet: in RAM, zeros, not a table in flash) */
    ui_values(n->live ? n->live : &none, n, b);
}

void ui_speak(ui_nav_t *n)
{
    const ui_screen_t *sc = nav_screen(n);
    ui_bag_t b;
    int i, k = 0;
    char v[UI_VALUE_MAX];
    bag(n, &b);
    n->said[0] = 0;
    for (i = 0; i < sc->n_say; i++) {
        const ui_say_t *s = &UI_SAYS[sc->say + i];
        const char *p;
        int w;
        for (w = 0; w < 3 && UI_WHENS[s->when][w] && ui_true(&b, UI_WHENS[s->when][w]); w++) {}
        if (w < 3 && UI_WHENS[s->when][w]) continue;
        for (p = UI_STR(s->text); *p; p++) {
            if (*p == UI_TPL_VAL) {
                k = join(n->said, k, ui_value(&b, UI_TPL_ID(p + 1), 0, v), UI_SAID);
                p += 2;
            } else {
                char c[2] = {*p, 0};
                k = join(n->said, k, c, UI_SAID);
            }
        }
    }
}

int ui_list_off(ui_nav_t *n)
{
    int id = nav_id(n), rows = UI_SCREENS[id].rows, cur, len, off;
    if (!rows) return n->off[id] = 0;
    cur = n->cursor[id];
    len = UI_SCREENS[id].list == UI_LIST_SAVED ? UI_SLOTS : n->live ? n->live->n_secs : 0;
    off = n->off[id];
    if (cur < off) off = cur;
    if (cur >= off + rows) off = cur - rows + 1;
    if (off > len - rows) off = len - rows;
    if (off < 0) off = 0;
    return n->off[id] = (int8_t)off;
}

void ui_nav_init(ui_nav_t *n)
{
    int i;
    memset(n, 0, sizeof *n);
    n->stack[0] = n->home = (uint8_t)UI_HOME;
    n->depth = 1;
    for (i = 0; i < UI_N_FIELDS; i++)
        n->req_at[i] = -1;
    n->roll_part = n->jump_i = -1;
}

void ui_new_song(ui_nav_t *n)
{
    n->stack[0] = n->home;
    n->depth = 1;
    ui_speak(n);
}

static void clear_waiting(ui_nav_t *n)
{
    int f;
    for (f = 0; f < UI_N_FIELDS; f++)
        n->req_at[f] = -1;
    n->roll_part = n->jump_i = -1;
}

void ui_update(ui_nav_t *n, const snap_t *s, uint32_t ms)
{
    int was = n->seen, seed_moved = n->seen && s->roll.seed != n->seed, bar = s->bar, f;
    n->now = ms;
    n->live = s;
    n->seen = 1;
    n->seed = s->roll.seed;
    if (!s->has_song || seed_moved)                   /* a new song or none: nothing waits */
        clear_waiting(n);
    if (n->roll_part >= 0) {                          /* a part's re-roll: sent in the bar before its boundary */
        if (bar >= n->roll_at)
            n->roll_part = -1;
        else if (!n->roll_sent && bar >= n->roll_at - 1) {
            n->roll_sent = 1;
            if (n->eng && n->eng->reroll_part) n->eng->reroll_part(n->roll_part);
        }
    }
    if (n->jump_i >= 0) {                             /* a jump: likewise; done once the song moved on from the bar it
                                                         was sent in, forward or back */
        if (bar >= n->jump_at || (n->jump_sent && bar != n->jump_sent_bar))
            n->jump_i = -1;
        else if (!n->jump_sent && bar >= n->jump_at - 1) {
            n->jump_sent = 1;
            n->jump_sent_bar = (int16_t)bar;
            if (n->eng && n->eng->jump) n->eng->jump(n->jump_start, n->jump_i);
        }
    }
    if (was && seed_moved && (n->depth > 1 || n->stack[0] != n->home))   /* a new song: back to the home */
        ui_new_song(n);
    for (f = 0; f < UI_N_FIELDS; f++) {
        int row = UI_FIELDS[f].row;
        if (n->req_at[f] < 0) continue;
        if (row == ROW_RHYTHM)                        /* (the comp's row or the strum patterns', as values.c real_row) */
            row = ch_rhythm_row(s->rows[ROW_KEYS], s->rows[ROW_COMP]);
        /* done once the song plays it (its row shows the value), or a bar past its boundary (refused, or a value the
           row writes otherwise): the change lands a 16th after the bar line, and the old value never shows between;
           or, sent, once the song went back before the bar it was sent in (Loop, Stay on song, a jump: app.c lands it
           as the song restarts there) */
        if ((bar >= n->req_at[f] && (s->rows[row] == n->req_val[f] || bar >= n->req_at[f] + 1)) ||
            (n->req_sent[f] && bar < n->req_at[f] - 1))
            n->req_at[f] = -1;
        else if (!n->req_sent[f] && bar >= n->req_at[f] - 1) {
            n->req_sent[f] = 1;
            if (n->eng && n->eng->set) n->eng->set(UI_FIELDS[f].row, n->req_val[f]);
        }
    }
    ui_list_off(n);
}

/* fieldMove: an options field steps through its choices (a switch wraps, the rest stop at the ends; wrap: this
   control's turn wraps them too), else fieldStep */
static int field_move(ui_nav_t *n, int f, int v, int d, int wrap)
{
    const ui_field_t *def = &UI_FIELDS[f];
    if (def->kind == UI_FK_OPTIONS) {
        int o[UI_OPTS_MAX], len, i;
        ui_bag_t b;
        bag(n, &b);
        len = ui_field_options(&b, f, o);
        for (i = 0; i < len && o[i] != v; i++) {}
        if (i == len) i = 0;
        if (!len) return v;
        if (def->wrap || wrap) return o[(((i + d) % len) + len) % len];
        i += d;
        return o[i < 0 ? 0 : i >= len ? len - 1 : i];
    }
    if (def->wrap) return (((v + d) % def->wrap) + def->wrap) % def->wrap;
    v += d;
    return v < def->min ? def->min : v > def->max ? def->max : v;
}

/* a knob's request: from the value waiting, else the song's; on the next 4-bar boundary (unless the song ends first);
   turning back to what plays undoes it (one already sent is set back); a `now` switch acts at once */
static void request(ui_nav_t *n, int f, int d, int wrap)
{
    const ui_field_t *def = &UI_FIELDS[f];
    ui_bag_t b;
    char t[UI_VALUE_MAX];
    int from, v, playing, bar = bar_now(n);
    if (!has_song(n)) return;
    bag(n, &b);
    playing = ui_field_now(&b, f);
    from = n->req_at[f] >= 0 ? n->req_val[f] : playing;
    v = field_move(n, f, from, d, wrap);
    if (def->now) {
        if (v != from && n->eng && n->eng->set) n->eng->set(def->row, v);
        say(n, UI_STR(def->name), ui_field_text(&b, f, v, t));
        return;
    }
    if (v != from && v == playing) {
        int sent = n->req_at[f] >= 0 && n->req_sent[f];
        n->req_at[f] = -1;
        if (sent && n->eng && n->eng->set) n->eng->set(def->row, v);
    } else if (v != from) {
        int at = boundary(bar);
        if (at < bars_of(n)) {
            if (n->req_at[f] < 0) n->req_seq[f] = ++n->seq;   /* (a change already waiting keeps its place) */
            n->req_val[f] = (int16_t)v;
            n->req_at[f] = (int16_t)at;
            n->req_sent[f] = 0;
            if (bar >= at - 1) ui_update(n, n->live, n->now);
        }
    }
    bag(n, &b);
    say(n, UI_STR(def->name), ui_field_text(&b, f, v, t));
}

/* a part's name as the mock writes it (part.toUpperCase(); the mixer writes keys as ACCOMP) */
static const char *part_name(int p, int mixer)
{
    static const char *const NAME[] = {"DRUMS", "BASS", "KEYS", "PAD", "LEAD", "TEXTURE", "SONG"};
    if (mixer && p == UI_PART_KEYS) return "ACCOMP";
    return p >= 0 && p < (int)(sizeof NAME / sizeof NAME[0]) ? NAME[p] : "";
}

/* the highlighted lane's part (a lanes screen's), or the action's own */
static int lane_part(const ui_nav_t *n, const ui_action_t *a)
{
    const ui_screen_t *sc = nav_screen(n);
    int c = n->cursor[nav_id(n)];
    if (a->arg != UI_NONE) return a->arg;
    return c < list(sc->lanes)->n ? list_at(sc->lanes, c) : -1;
}

/* a part screen's name, as its title starts (up to a ':'), upper case: the reroll's and the lock's messages */
static void title_name(const ui_screen_t *sc, char *name)
{
    const char *title = UI_STR(sc->title);
    int k;
    for (k = 0; title[k] && title[k] != ':' && k < UI_MSG - 1; k++)
        name[k] = title[k] >= 'a' && title[k] <= 'z' ? (char)(title[k] - 32) : title[k];
    name[k] = 0;
}

static void act(ui_nav_t *n, int ai, int detents)
{
    const ui_action_t *a = &UI_ACTIONS[ai];
    const ui_screen_t *sc = nav_screen(n);
    int id = nav_id(n), bar = bar_now(n);
    char t[UI_VALUE_MAX];
    int kind = a->kind;
    if (!ai) return;
    if (kind == UA_SETTING)                           /* the highlighted line's action (Settings' knobs) */
        kind = n->cursor[id] < list(a->list)->n ? list_at(a->list, n->cursor[id]) : UA_NONE;
    switch (kind) {
    case UA_PART:                                     /* ALGO: the next part (at its first page); SELECT: next page */
    case UA_PAGE: {
        int l = a->kind == UA_PART ? UI_PART_SCREENS : sc->pages, len = list(l)->n, here = -1, i;
        for (i = 0; i < len && here < 0; i++) {
            int s = list_at(l, i), j;
            if (s == id) here = i;
            if (a->kind == UA_PART)
                for (j = 0; j < list(UI_SCREENS[s].pages)->n; j++)
                    if (list_at(UI_SCREENS[s].pages, j) == id) here = i;
        }
        if (here < 0 || len < 2) return;
        n->stack[n->depth - 1] = (uint8_t)list_at(l, (((here + detents) % len) + len) % len);
        if (nav_id(n) != id) ui_speak(n);
        return;
    }
    case UA_JUMP: {                                   /* the highlighted section (or the key's): at the next 4-bar
                                                         boundary, or the song's last bar where the song ends first */
        int i = a->arg != UI_NONE ? a->arg : n->cursor[id];
        if (has_song(n) && i < n->live->n_secs) {
            int at = boundary(bar), last = bars_of(n) - 1;
            if (last < bar + 1) last = bar + 1;
            if (at > last) at = last;
            n->jump_i = (int8_t)i;
            n->jump_at = (int16_t)at;
            n->jump_start = n->live->secs[i].start;
            n->jump_sent = 0;
            if (bar >= at - 1) ui_update(n, n->live, n->now);
            upper(t, plan_section_name(n->live->secs[i].section), sizeof t);
            say(n, "GO TO", t);
        }
        return;
    }
    case UA_LOCK: {                                   /* SEQ held on a part's screen (or the main screen, its
                                                         highlighted tile's): the part locked (again: unlocked),
                                                         at once; a new song keeps it (app.c) */
        char name[UI_MSG];
        int on, p = a->arg;
        if (a->list)                                  /* the main screen: the highlighted tile's part (or the song
                                                         line) */
            p = n->cursor[id] < list(a->list)->n ? list_at(a->list, n->cursor[id]) : UI_NONE;
        if (p == UI_NONE) return;
        on = !(n->locks >> p & 1u);
        n->locks = (uint8_t)(on ? n->locks | 1u << p : n->locks & ~(1u << p));
        if (a->list)
            copy(name, part_name(p, 1), UI_MSG);
        else
            title_name(sc, name);
        say(n, name, on ? "LOCKED" : "UNLOCKED");
        return;
    }
    case UA_REROLL:
        if (a->arg != UI_NONE) {                      /* SEQ on a part's screen: that part, on the 4-bar boundary */
            if (a->arg == UI_PART_TEXTURE) return;    /* (nothing to re-roll: SEQ held there locks it) */
            if (has_song(n)) {
                char name[UI_MSG];
                int at = boundary(bar);
                title_name(sc, name);
                if (n->locks >> a->arg & 1u) {        /* locked: no roll (desk.js's "is locked") */
                    say(n, name, "IS LOCKED");
                    return;
                }
                if (at < bars_of(n)) {
                    n->roll_part = (int8_t)a->arg;
                    n->roll_at = (int16_t)at;
                    n->roll_screen = (uint8_t)id;
                    n->roll_sent = 0;
                    if (bar >= at - 1) ui_update(n, n->live, n->now);
                }
                say(n, name, "NEW ROLL");
            }
        } else {                                      /* SEQ: a new song */
            if (n->eng && n->eng->reroll) n->eng->reroll();
            say(n, "NEW SONG", "");
        }
        return;
    case UA_LEVEL: {                                  /* the mixer: 1 dB a detent, -12..+6 dB, at once */
        int p = lane_part(n, a), db;
        char v[8];
        if (p < 0) return;
        db = (n->eng && n->eng->level_get ? n->eng->level_get(p) : 0) + detents;
        db = db < -12 ? -12 : db > 6 ? 6 : db;
        if (n->eng && n->eng->level) n->eng->level(p, db);
        {
            int k = 0, m = db < 0 ? -db : db;
            if (db > 0) v[k++] = '+';
            if (db < 0) v[k++] = '-';
            if (m >= 10) v[k++] = (char)('0' + m / 10);
            v[k++] = (char)('0' + m % 10);
            v[k] = 0;
        }
        join(t, join(t, 0, v, sizeof t), " DB", sizeof t);
        say(n, part_name(p, 1), t);
        return;
    }
    case UA_MUTE:
    case UA_SOLO: {                                   /* the lane's part muted or soloed (again: neither); at once */
        int p = lane_part(n, a), solo = n->eng && n->eng->soloed ? n->eng->soloed() : -1;
        if (p < 0 || p == UI_PART_TEXTURE) return;
        if (a->kind == UA_MUTE && solo == p) {        /* a press on the soloed part: un-solo, not mute */
            if (n->eng && n->eng->solo) n->eng->solo(-1);
            say(n, part_name(p, 0), "SOLO OFF");
        } else if (a->kind == UA_MUTE) {
            int on = !(n->eng && n->eng->muted && n->eng->muted(p));
            if (n->eng && n->eng->mute) n->eng->mute(p, on);
            say(n, part_name(p, 0), on ? "MUTED" : "ON");
        } else {
            int on = solo != p;
            if (n->eng && n->eng->solo) n->eng->solo(on ? p : -1);
            say(n, part_name(p, 0), on ? "SOLO" : "SOLO OFF");
        }
        return;
    }
    case UA_CURSOR: {                                 /* a menu: the highlight moves (wraps) */
        int len = list(sc->menu)->n ? list(sc->menu)->n
                  : sc->list == UI_LIST_SAVED ? UI_SLOTS
                  : sc->list ? (n->live ? (n->live->n_secs < NAV_MENU_MAX ? n->live->n_secs : NAV_MENU_MAX) : 0)
                  : list(sc->lanes)->n ? list(sc->lanes)->n : sc->rows;   /* rows alone: a plain menu's lines */
        if (len) n->cursor[id] = (int8_t)((((n->cursor[id] + detents) % len) + len) % len);
        ui_list_off(n);
        return;
    }
    case UA_GO: {
        int to;
        if (a->arg == UI_GO_CURSOR)
            to = n->cursor[id] < list(sc->menu)->n ? list_at(sc->menu, n->cursor[id]) : -1;
        else
            to = a->arg;
        if (to < 0) return;
        if (to == UI_GO_BACK) {
            if (n->depth > 1) n->depth--;
        } else if (to == UI_GO_HOME) {
            n->stack[0] = n->home;
            n->depth = 1;
        } else if (a->by) {                           /* the screen alone on the breadcrumb (the home as it is) */
            n->stack[0] = (uint8_t)to;
            n->depth = 1;
        } else if (to != id && n->depth < UI_STACK)
            n->stack[n->depth++] = (uint8_t)to;
        if (nav_id(n) != id) ui_speak(n);
        ui_list_off(n);
        return;
    }
    case UA_SAVE:                                     /* the song saved: into the highlighted slot on the saved
                                                         songs' list, else the first free one */
    case UA_RECALL:                                   /* the highlighted slot's song: the next song */
    case UA_ERASE: {                                  /* the highlighted slot deleted */
        int slot = sc->list == UI_LIST_SAVED ? n->cursor[id] : -1, r;
        const ui_engine_t *e = n->eng;
        char num[4] = {0, 0, 0, 0};
        if (a->kind == UA_SAVE)
            r = e && e->save ? e->save(slot) : UI_SAVE_NONE;
        else if (slot < 0)
            return;
        else
            r = a->kind == UA_RECALL ? (e && e->recall ? e->recall(slot) : UI_SAVE_EMPTY)
                                     : (e && e->erase ? e->erase(slot) : UI_SAVE_EMPTY);
        if (a->kind == UA_SAVE && r >= 0) slot = r;
        if (slot >= 9) num[0] = '1', num[1] = (char)('0' + slot - 9);
        else if (slot >= 0) num[0] = (char)('1' + slot);
        if (r == UI_SAVE_NONE) say(n, "NO SONG", "TO SAVE");
        else if (r == UI_SAVE_FULL) say(n, "NO FREE SLOT", "DELETE ONE FIRST");
        else if (r == UI_SAVE_BUSY) say(n, "A SAVE WAITS", "STOP TO SAVE MORE");
        else {
            join(t, join(t, 0, a->kind == UA_SAVE && r >= 0 ? "SAVED TO " : "SLOT ", sizeof t), num, sizeof t);
            say(n, t, r == UI_SAVE_EMPTY ? "IS EMPTY" : a->kind == UA_RECALL ? "IS NEXT" : a->kind == UA_ERASE
                ? "DELETED" : "");
        }
        return;
    }
    case UA_READOUT:
        if (n->eng && n->eng->readout) n->eng->readout();
        return;
    case UA_THEME:                                    /* the next palette (a turn: detents of them), at once */
        if (n->eng && n->eng->theme) say(n, "THEME", n->eng->theme(detents));
        return;
    case UA_BASS_SAT:                                 /* the bass's harmonics on or off (each detent flips them) */
        if (n->eng && n->eng->bass_sat) say(n, "BASS SATURATION", n->eng->bass_sat(detents));
        return;
    case UA_HOME_SCREEN:                              /* the home: now playing or the main screen (each detent flips) */
        if (n->eng && n->eng->home_screen) say(n, "HOME SCREEN", n->eng->home_screen(detents));
        return;
    case UA_PLAY:
        say(n, n->live && n->live->playing ? "STOP" : "PLAY", "");
        if (n->eng && n->eng->play) n->eng->play();
        return;
    case UA_TURN:
    case UA_NUDGE: {                                  /* byCursor: the highlighted line's field */
        int f = a->list ? (n->cursor[id] < list(a->list)->n ? list_at(a->list, n->cursor[id]) : UI_NONE) : a->arg;
        if (f != UI_NONE) request(n, f, a->kind == UA_TURN ? detents * a->by : a->by, a->wrap);
        return;
    }
    default:
        return;
    }
}

void ui_press(ui_nav_t *n, int control, int detents)
{
    if (control < 0 || control >= UI_N_CONTROLS) return;
    act(n, ui_control(nav_screen(n), control), detents);
}

/* a control that may be held (slot: a key's number, or UI_KEYS + a button's) down or up: one whose action has a long
   press acts on release when let go soon, or once held its time (ui_tick: its long action); others act as they go
   down */
static void hold(ui_nav_t *n, int slot, int control, int down, uint32_t ms)
{
    int ai;
    n->now = ms;
    if (down) {
        ai = ui_control(nav_screen(n), control);
        if (!ai || !UI_ACTIONS[ai].lng) {
            act(n, ai, 1);
            return;
        }
        n->held_action[slot] = (uint8_t)ai;
        n->held_fired[slot] = 0;
        n->held_t[slot] = ms;
    } else {
        ai = n->held_action[slot];
        n->held_action[slot] = 0;
        if (ai && !n->held_fired[slot]) act(n, ai, 1);
    }
}

void ui_key(ui_nav_t *n, int key, int down, uint32_t ms)
{
    if (key >= 0 && key < UI_KEYS) hold(n, key, UI_CTL_KEY(key), down, ms);
}

void ui_button(ui_nav_t *n, int button, int down, uint32_t ms)
{
    if (button >= 0 && button < UI_BUTTONS) hold(n, UI_KEYS + button, UI_CTL_BUTTON(button), down, ms);
}

void ui_tick(ui_nav_t *n, uint32_t ms)
{
    int k;
    n->now = ms;
    for (k = 0; k < UI_HOLDS; k++)
        if (n->held_action[k] && !n->held_fired[k] && ms - n->held_t[k] >= (k < UI_KEYS ? UI_LONG_MS : UI_HOLD_MS)) {
            n->held_fired[k] = 1;
            act(n, UI_ACTIONS[n->held_action[k]].lng, 1);
        }
}

/* the screen's keyLeds (else the defaults'): lanes, a part's key lit while it plays, blinking soloed; sections, the
   playing section's key lit, a jump waiting blinking */
uint32_t ui_key_lights(const ui_nav_t *n, const snap_t *s, uint32_t *blink)
{
    const ui_screen_t *sc = nav_screen(n);
    const ui_key_leds_t *k;
    uint32_t on = 0, bl = 0;
    int i, kl = sc->key_leds;
    *blink = 0;
    if (!kl) return 0;
    k = &UI_KEY_LEDS[kl - 1];
    if (k->kind == UI_KL_LANES) {
        int parts = k->parts ? k->parts : sc->lanes, solo = n->eng && n->eng->soloed ? n->eng->soloed() : -1;
        for (i = 0; i < list(parts)->n && i < list(k->keys)->n; i++) {
            int p = list_at(parts, i), key = list_at(k->keys, i);
            if (solo == p) bl |= 1u << key;
            else if (!(solo >= 0 || (n->eng && n->eng->muted && n->eng->muted(p)))) on |= 1u << key;
        }
    } else if (k->kind == UI_KL_SECTIONS && s && s->has_song) {
        for (i = 0; i < list(k->keys)->n; i++) {
            int key = list_at(k->keys, i);
            if (i < s->n_secs && s->bar + 1 > s->secs[i].start && s->bar + 1 <= s->secs[i].start + s->secs[i].bars)
                on |= 1u << key;
            if (n->jump_i == i) bl |= 1u << key;
        }
    }
    *blink = bl;
    return on;
}

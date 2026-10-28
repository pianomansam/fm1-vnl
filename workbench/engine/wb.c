/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The workbench's engine glue (wb.h). The render runs the song as the app does (app.c's step: the plan's filter
   moves, the last block's fade, the step's events to the voices at its start sample), without the app's mailbox,
   requests or gaps between songs: one song, looped after a 2 s gap at its end. Exports for the worklet follow
   base/web/emu/emu_web.c's pattern. */
#include <stdio.h>
#include <string.h>
#include "wb.h"
#include "song.h"
#include "library.h"
#include "override.h"
#include "voices.h"

#define EXPORT __attribute__((used, visibility("default")))
#define SR 44100u
#define GAP (2u * SR)                   /* the silence between the song's end and its loop */

static song_t song;
static song_roll_t live, staged;        /* the roll playing; the one the next landing applies */
static song_ov_t ov_staged;
static lib_set_t set_live, set_staged;  /* the custom set (LIB_SET_WB): playing; staged */
static uint8_t base_live, base_staged;  /* the library set each was copied from (its id-keyed rules) */
static uint8_t playing, apply_pend, start_pend, restart_pend, in_gap;
static int cur_bar = -1;                /* the bar of the step last ticked (-1: none yet) */
static int resume_bar;                  /* where play starts */
static uint32_t next_at;                /* frames to the next step (or to the gap's end) */
static float frac;                      /* and its fraction, as app.c advances r_pos */
static uint32_t pos;                    /* the song's time, in samples, at the next frame rendered */
static uint32_t dice_salt;
static int catchups;
static float out_l[WB_MAX_FRAMES], out_r[WB_MAX_FRAMES];
static song_ev_t evs[64];

/* the step's length: advances next_at and frac (app.c's r_pos arithmetic, so a catch-up lands on the same samples) */
static uint32_t advance(void)
{
    float adv = song_step_samples(&song) + frac;
    uint32_t ip = (uint32_t)adv;
    frac = adv - (float)ip;
    return ip;
}

/* The staged state becomes the song's: it starts again and ticks silently (events dropped) up to bar target (from 0;
   past the end, bar 0), the old notes released, the voices configured for it. fresh: a new start (play, a seed): the
   engine cleared if nothing sounds, the chain restarted from the seed (vx_song). */
static void land(int target, int fresh)
{
    set_live = set_staged;
    base_live = base_staged;
    lib_set_wb(&set_live, base_live);
    song_override(&ov_staged);
    if (staged.v[RF_MAP] != live.v[RF_MAP])
        target = 0;                     /* a new map redraws everything: from bar 1 */
    live = staged;
    song_start(&song, &live);
    if (target >= song.roll.v[RF_BARS])
        target = 0;
    pos = 0;
    frac = 0.0f;
    while (song.state == SS_PLAYING && song.bar < target) {
        song_tick(&song, evs, 64);
        pos += advance();
    }
    if (fresh) {
        if (vx_sounding())
            vx_release_all();
        else
            vx_init();
        vx_song(&song.roll);
    } else {
        vx_release_all();
        vx_configure(&song.roll);       /* the bed reopens */
    }
    vx_set_gain(1.0f);
    cur_bar = target - 1;
    next_at = 0;
    apply_pend = in_gap = 0;
}

/* one 16th (app.c's step) */
static void tick(void)
{
    const plan_block_t *last = &song.plan.b[song.plan.n ? song.plan.n - 1 : 0];
    int bar = song.bar, st = song.step, n, i;
    float lp[PM_N], hp[PM_N];
    plan_moves(&song.plan, bar, st, lp, hp);
    for (i = 0; i < PM_N; i++) {
        vx_set_filter(i, lp[i]);
        vx_set_hp(i, hp[i]);
    }
    if (song.plan.n && (last->flags & PB_FADE) && bar >= last->start) {
        int len = last->bars * 16, at = (bar - last->start) * 16 + st + 1;
        vx_set_gain(1.0f - (float)at / (float)len);
    }
    n = song_tick(&song, evs, 64);
    for (i = 0; i < n; i++)
        vx_event(&evs[i]);
    cur_bar = bar;
    next_at = advance();
}

/* a step is due: a landing waiting for this bar line, the song's end (the gap, then the loop), or the next 16th */
static void due(void)
{
    if (apply_pend && (song.step == 0 || song.state == SS_ENDED)) {
        catchups++;
        land(song.bar, 0);
    } else if (song.state == SS_ENDED) {
        if (in_gap) {
            land(0, 1);
            return;
        }
        vx_release_all();
        in_gap = 1;
        next_at = GAP;
        return;
    }
    tick();
}

EXPORT void wb_init(void)
{
    song_override(NULL);
    ov_staged = *song_ov();
    memset(&set_staged, 0, sizeof set_staged);
    set_live = set_staged;
    base_live = base_staged = 0;
    lib_set_wb(&set_live, 0);
    vx_init();
    playing = apply_pend = start_pend = restart_pend = in_gap = 0;
    catchups = 0;
    dice_salt = 0;
    roll_song(0, 1, &staged);
    live = staged;
    cur_bar = -1;
    resume_bar = 0;
    pos = 0;
}

EXPORT float *wb_out_l(void) { return out_l; }
EXPORT float *wb_out_r(void) { return out_r; }

EXPORT void wb_render(uint32_t n)
{
    uint32_t done = 0, k;
    if (n > WB_MAX_FRAMES)
        n = WB_MAX_FRAMES;
    memset(out_l, 0, n * sizeof out_l[0]);
    memset(out_r, 0, n * sizeof out_r[0]);
    if (restart_pend) {
        restart_pend = 0;
        resume_bar = 0;
        cur_bar = -1;
        pos = 0;
        if (playing)
            start_pend = 1;
    }
    if (start_pend) {
        start_pend = 0;
        land(resume_bar, 1);
    }
    if (!playing) {
        if (vx_sounding())               /* a stop's 40 ms */
            vx_render(out_l, out_r, n);
        return;
    }
    while (done < n) {
        if (!next_at)
            due();
        k = next_at < n - done ? next_at : n - done;
        vx_render(out_l + done, out_r + done, k);
        done += k;
        next_at -= k;
        pos += k;
    }
}

EXPORT void wb_seed(uint32_t seed)
{
    roll_song(0, seed, &staged);
    restart_pend = 1;
}

EXPORT void wb_play(int on)
{
    if (on && !playing) {
        playing = start_pend = 1;
    } else if (!on && playing) {
        playing = start_pend = apply_pend = 0;
        resume_bar = cur_bar > 0 ? cur_bar : 0;   /* the start of the bar sounding */
        vx_stop();
    }
}

/* the roll's checks (a set suits the mood) see the staged custom set while the page stages */
static void stage_set(int on)
{
    if (on)
        lib_set_wb(&set_staged, base_staged);
    else
        lib_set_wb(&set_live, base_live);
}

EXPORT int wb_roll_get(int field)
{
    return field >= 0 && field < RF_N ? staged.v[field] : -1;
}

EXPORT int wb_roll_set(int field, int value)
{
    int v;
    stage_set(1);
    v = roll_set(&staged, field, value);
    stage_set(0);
    return v;
}

EXPORT int wb_roll_dice(int field)
{
    int ok;
    stage_set(1);
    ok = roll_dice(&staged, field, ++dice_salt);
    stage_set(0);
    return ok ? staged.v[field] : -1;
}

static uint8_t *role_ins(lib_set_t *s, int role)
{
    switch (role) {
    case V_CHORD: return &s->chords;
    case V_BASS: return &s->bass;
    case V_LEAD: return &s->lead;
    case V_CANDY: return &s->candy;
    case V_PAD: return &s->pad;
    default: return NULL;
    }
}

EXPORT void wb_role_set(int role, int ins)
{
    uint8_t *p;
    if (!role_ins(&set_staged, role) || ins < 0 || ins > 255)
        return;
    if (staged.v[RF_SET] != LIB_SET_WB) {   /* the first edit: the song's set copied */
        const lib_set_t *s = lib_set_by_id((uint8_t)staged.v[RF_SET]);
        if (!s)
            return;
        set_staged = *s;
        set_staged.id = LIB_SET_WB;
        base_staged = s->id;
        staged.v[RF_SET] = LIB_SET_WB;
    }
    p = role_ins(&set_staged, role);
    *p = (uint8_t)ins;
}

EXPORT int wb_role_get(int role)
{
    const lib_set_t *p = staged.v[RF_SET] == LIB_SET_WB ? &set_staged : lib_set_by_id((uint8_t)staged.v[RF_SET]);
    lib_set_t s;
    uint8_t *ins;
    if (!p)
        return -1;
    s = *p;
    ins = role_ins(&s, role);
    return ins ? *ins : -1;
}

EXPORT void wb_ov_set(int key, int value)
{
    song_ov_t *o = &ov_staged;
    switch (key) {
    case WB_OV_COMP: o->comp = (int8_t)value; break;
    case WB_OV_LEAD_STYLE: o->lead_style = (int8_t)value; break;
    case WB_OV_PAD_PLACE: o->pad_place = (int8_t)value; break;
    case WB_OV_RIDE: o->ride = (int8_t)value; break;
    case WB_OV_KEYS_OCT: o->keys_oct = (int8_t)value; break;
    case WB_OV_LEAD_OCT: o->lead_oct = (int8_t)value; break;
    case WB_OV_VOICING_SALT: o->voicing_salt = (uint32_t)value; break;
    case WB_OV_PHRASE_SALT: o->phrase_salt = (uint32_t)value; break;
    default: break;
    }
}

EXPORT int wb_ov_get(int key)
{
    const song_ov_t *o = &ov_staged;
    switch (key) {
    case WB_OV_COMP: return o->comp;
    case WB_OV_LEAD_STYLE: return o->lead_style;
    case WB_OV_PAD_PLACE: return o->pad_place;
    case WB_OV_RIDE: return o->ride;
    case WB_OV_KEYS_OCT: return o->keys_oct;
    case WB_OV_LEAD_OCT: return o->lead_oct;
    case WB_OV_VOICING_SALT: return (int)o->voicing_salt;
    case WB_OV_PHRASE_SALT: return (int)o->phrase_salt;
    default: return 0;
    }
}

EXPORT void wb_apply(void)
{
    if (playing)
        apply_pend = 1;                 /* stopped: play lands the staged state */
}

EXPORT int wb_bar(void) { return cur_bar + 1; }
EXPORT int wb_bars(void) { return cur_bar >= 0 ? live.v[RF_BARS] : staged.v[RF_BARS]; }
EXPORT float wb_seconds(void) { return (float)pos / (float)SR; }
EXPORT int wb_catchups(void) { return catchups; }

EXPORT const char *wb_consts(void)
{
    static const struct { const char *name; int v; } RF[] = {
        {"KEY", RF_KEY}, {"TEMPO", RF_TEMPO}, {"MOTION", RF_MOTION}, {"MOOD", RF_MOOD}, {"MODE", RF_MODE},
        {"MAP", RF_MAP}, {"PROG", RF_PROG}, {"COLOR", RF_COLOR}, {"SET", RF_SET}, {"PATTERN", RF_PATTERN},
        {"SWING", RF_SWING}, {"DUST", RF_DUST}, {"NOISE", RF_NOISE}, {"AMBIENCE", RF_AMBIENCE}, {"BARS", RF_BARS},
        {"N", RF_N}}, OV[] = {
        {"COMP", WB_OV_COMP}, {"LEAD_STYLE", WB_OV_LEAD_STYLE}, {"PAD_PLACE", WB_OV_PAD_PLACE}, {"RIDE", WB_OV_RIDE},
        {"KEYS_OCT", WB_OV_KEYS_OCT}, {"LEAD_OCT", WB_OV_LEAD_OCT}, {"VOICING_SALT", WB_OV_VOICING_SALT},
        {"PHRASE_SALT", WB_OV_PHRASE_SALT}, {"N", WB_OV_N}}, V[] = {
        {"CHORD", V_CHORD}, {"BASS", V_BASS}, {"LEAD", V_LEAD}, {"CANDY", V_CANDY}, {"PAD", V_PAD}};
    static char buf[1024];
    int n = 0;
    unsigned i;
#define TABLE(T, key)                                                                    \
    n += snprintf(buf + n, sizeof buf - (unsigned)n, "\"%s\":{", key);                   \
    for (i = 0; i < sizeof T / sizeof T[0]; i++)                                         \
        n += snprintf(buf + n, sizeof buf - (unsigned)n, "%s\"%s\":%d", i ? "," : "", T[i].name, T[i].v); \
    n += snprintf(buf + n, sizeof buf - (unsigned)n, "},");
    if (!buf[0]) {
        n += snprintf(buf, sizeof buf, "{");
        TABLE(RF, "RF")
        TABLE(OV, "OV")
        TABLE(V, "V")
        snprintf(buf + n, sizeof buf - (unsigned)n, "\"LIB_SET_WB\":%d}", LIB_SET_WB);
    }
#undef TABLE
    return buf;
}

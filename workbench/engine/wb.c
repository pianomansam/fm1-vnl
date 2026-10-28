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
#include "harmony.h"

#define EXPORT __attribute__((used, visibility("default")))
#define SR 44100u
#define GAP (2u * SR)                   /* the silence between the song's end and its loop */

static song_t song;
static song_roll_t live, staged;        /* the roll playing; the one the next landing applies */
static song_ov_t ov_staged;
static lib_set_t set_live, set_staged;  /* the custom set (LIB_SET_WB): playing; staged */
static uint8_t base_live, base_staged;  /* the library set each was copied from (its id-keyed rules) */
static lib_pattern_t pat_live, pat_staged;  /* the custom pattern (LIB_PATTERN_WB: a kick phrase over a library one) */
static uint8_t pat_base;                /* the library pattern the staged custom one was copied from */
static int kick_a, kick_b;              /* the staged kick phrase's step masks (0, 0: the pattern's own kick) */
static struct {                         /* wb_stage_mark's copy, for wb_stage_undo */
    song_roll_t roll; song_ov_t ov; lib_set_t set; lib_pattern_t pat; uint8_t base, pat_base; int kick_a, kick_b;
} mark;
static song_t scratch[2];
static int lead_oct_applied;            /* the live lead_oct as the engine applied it (0: refused everywhere) */
static int landings;
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

/* a step's lead notes, hashed (0: none) */
static uint32_t lead_notes(song_t *s)
{
    uint32_t h = 0;
    int n = song_tick(s, evs, 64), i;
    for (i = 0; i < n; i++)
        if (evs[i].voice == V_LEAD && evs[i].vel)
            h = (h ^ evs[i].note) * 16777619u + 1u;
    return h;
}

/* The staged lead_oct if it moves any lead note (lead.c keeps the register where the instrument can't move), else 0:
   the staged song with and without it, run side by side to the first difference (scratch runs: song.c's per-bar
   state is rebuilt by the catch-up that follows in land) */
static int lead_oct_moves(void)
{
    song_ov_t flat = ov_staged;
    int moved = 0;
    if (!ov_staged.lead_oct)
        return 0;
    flat.lead_oct = 0;
    song_start(&scratch[0], &staged);
    song_start(&scratch[1], &staged);
    while (!moved && scratch[0].state == SS_PLAYING && scratch[1].state == SS_PLAYING) {
        uint32_t h = lead_notes(&scratch[0]);
        song_override(&flat);
        moved = h != lead_notes(&scratch[1]);
        song_override(&ov_staged);
    }
    return moved ? ov_staged.lead_oct : 0;
}

/* The staged state becomes the song's: it starts again and ticks silently (events dropped) up to bar target (from 0;
   past the end, bar 0), the old notes released, the voices configured for it. fresh: a new start (play, a seed): the
   engine cleared if nothing sounds, the chain restarted from the seed (vx_song). */
static void land(int target, int fresh)
{
    set_live = set_staged;
    base_live = base_staged;
    lib_set_wb(&set_live, base_live);
    pat_live = pat_staged;
    lib_pattern_wb(pat_live.id ? &pat_live : NULL);
    song_override(&ov_staged);
    lead_oct_applied = lead_oct_moves();
    landings++;
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
    memset(&pat_staged, 0, sizeof pat_staged);
    pat_live = pat_staged;
    pat_base = 0;
    kick_a = kick_b = 0;
    lib_pattern_wb(NULL);
    lead_oct_applied = landings = 0;
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

/* the roll's checks (a set suits the mood) see the staged custom set and pattern while the page stages */
static void stage_set(int on)
{
    if (on) {
        lib_set_wb(&set_staged, base_staged);
        lib_pattern_wb(pat_staged.id ? &pat_staged : NULL);
    } else {
        lib_set_wb(&set_live, base_live);
        lib_pattern_wb(pat_live.id ? &pat_live : NULL);
    }
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

/* the staged custom set, the song's set copied into it on the first edit (RF_SET switches to LIB_SET_WB); NULL if the
   song's set is unknown */
static lib_set_t *custom_set(void)
{
    if (staged.v[RF_SET] != LIB_SET_WB) {
        const lib_set_t *s = lib_set_by_id((uint8_t)staged.v[RF_SET]);
        if (!s)
            return NULL;
        set_staged = *s;
        set_staged.id = LIB_SET_WB;
        base_staged = s->id;
        staged.v[RF_SET] = LIB_SET_WB;
    }
    return &set_staged;
}

EXPORT void wb_role_set(int role, int ins)
{
    if (!role_ins(&set_staged, role) || ins < 0 || ins > 255 || !custom_set())
        return;
    *role_ins(&set_staged, role) = (uint8_t)ins;
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

/* the song's drum kit (KIT_*), as lib_song_kit draws it from the staged set */
EXPORT int wb_kit_get(void)
{
    int k;
    stage_set(1);
    k = lib_song_kit(&staged);
    stage_set(0);
    return k;
}

/* the kit in the custom set, so lib_song_kit plays it always: a synth kit with no sampled one, or the reverse */
EXPORT void wb_kit_set(int kit)
{
    lib_set_t *s;
    if (kit < 0 || kit >= KIT_N || !(s = custom_set()))
        return;
    s->kit = lib_kit_sampled(kit) ? KIT_N : (uint8_t)kit;
    s->skit = lib_kit_sampled(kit) ? (uint8_t)kit : KIT_N;
}

/* A two-bar kick phrase (step masks for bars A and B) over the staged pattern: the custom pattern copies the library
   one (both bars alike) with its kick lane replaced and unghosted; 0, 0 returns to the library pattern itself */
EXPORT void wb_kick_phrase(int a, int b)
{
    const lib_pattern_t *p;
    int i;
    if (staged.v[RF_PATTERN] != LIB_PATTERN_WB)
        pat_base = (uint8_t)staged.v[RF_PATTERN];
    if (!a && !b) {
        staged.v[RF_PATTERN] = pat_base;
        return;
    }
    if (!(p = lib_pattern_by_id(pat_base)))
        return;
    pat_staged = *p;
    pat_staged.id = LIB_PATTERN_WB;
    pat_staged.two_bar = 1;
    for (i = 0; i < 6; i++) {
        pat_staged.hit_b[i] = p->hit[i];
        pat_staged.ghost_b[i] = p->ghost[i];
    }
    pat_staged.hit[D_KICK] = (uint16_t)a;
    pat_staged.hit_b[D_KICK] = (uint16_t)b;
    pat_staged.ghost[D_KICK] = pat_staged.ghost_b[D_KICK] = 0;
    kick_a = a;
    kick_b = b;
    staged.v[RF_PATTERN] = LIB_PATTERN_WB;
}

/* 1 if roll_set would take value for field as staged (the dropdowns offer only these) */
EXPORT int wb_roll_legal(int field, int value)
{
    song_roll_t t = staged;
    int v;
    stage_set(1);
    v = roll_set(&t, field, value);
    stage_set(0);
    return v == value;
}

/* the staged state saved, and put back (a change that would move a locked row is undone whole) */
EXPORT void wb_stage_mark(void)
{
    mark.roll = staged;
    mark.ov = ov_staged;
    mark.set = set_staged;
    mark.pat = pat_staged;
    mark.base = base_staged;
    mark.pat_base = pat_base;
    mark.kick_a = kick_a;
    mark.kick_b = kick_b;
}

EXPORT void wb_stage_undo(void)
{
    staged = mark.roll;
    ov_staged = mark.ov;
    set_staged = mark.set;
    pat_staged = mark.pat;
    base_staged = mark.base;
    pat_base = mark.pat_base;
    kick_a = mark.kick_a;
    kick_b = mark.kick_b;
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

/* The register the engine applies, -1, 0 or +1: KEYS_OCT for the staged state (harmony_keys_oct: 0 where the
   instrument's range can't hold the move); LEAD_OCT for the song playing, as of its last landing (lead.c keeps the
   lead's register where under an octave would remain: 0 if no note moved). Else wb_ov_get's. */
EXPORT int wb_ov_applied(int key)
{
    song_ov_t live_ov = *song_ov();
    int d;
    if (key == WB_OV_LEAD_OCT)
        return lead_oct_applied;
    if (key != WB_OV_KEYS_OCT)
        return wb_ov_get(key);
    stage_set(1);
    song_override(&ov_staged);
    d = harmony_keys_oct(&staged) / 12;
    song_override(&live_ov);
    stage_set(0);
    return d;
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
EXPORT int wb_landings(void) { return landings; }

/* JSON: the staged state the page shows (the roll, the roles, the overrides, the kit, the library pattern under a
   kick phrase and the phrase's masks), and the library's maps and progressions with their notes */
EXPORT const char *wb_state(void)
{
    static char buf[512];
    int n, i, custom = staged.v[RF_PATTERN] == LIB_PATTERN_WB;
    n = snprintf(buf, sizeof buf, "{\"seed\":%u,\"roll\":[", (unsigned)staged.seed);
    for (i = 0; i < RF_N; i++)
        n += snprintf(buf + n, sizeof buf - (unsigned)n, "%s%d", i ? "," : "", staged.v[i]);
    n += snprintf(buf + n, sizeof buf - (unsigned)n, "],\"roles\":[");
    for (i = V_CHORD; i <= V_PAD; i++)
        n += snprintf(buf + n, sizeof buf - (unsigned)n, "%s%d", i ? "," : "", wb_role_get(i));
    n += snprintf(buf + n, sizeof buf - (unsigned)n, "],\"ov\":[");
    for (i = 0; i < WB_OV_N; i++)
        n += snprintf(buf + n, sizeof buf - (unsigned)n, "%s%d", i ? "," : "", wb_ov_get(i));
    snprintf(buf + n, sizeof buf - (unsigned)n, "],\"kit\":%d,\"pattern\":%d,\"kick\":[%d,%d]}", wb_kit_get(),
             custom ? pat_base : staged.v[RF_PATTERN], custom ? kick_a : 0, custom ? kick_b : 0);
    return buf;
}

EXPORT const char *wb_library(void)
{
    static char buf[8192];
    int n, i;
    if (buf[0])
        return buf;
    n = snprintf(buf, sizeof buf, "{\"maps\":[");
    for (i = 0; i < LIB_NMAPS; i++)
        n += snprintf(buf + n, sizeof buf - (unsigned)n, "%s{\"id\":%d,\"name\":\"%s\"}", i ? "," : "",
                      LIB_MAPS[i].id, LIB_MAPS[i].name);
    n += snprintf(buf + n, sizeof buf - (unsigned)n, "],\"progs\":[");
    for (i = 0; i < LIB_NPROGS; i++)
        n += snprintf(buf + n, sizeof buf - (unsigned)n, "%s{\"id\":%d,\"src\":\"%s\"}", i ? "," : "",
                      LIB_PROGS[i].id, LIB_PROGS[i].src);
    snprintf(buf + n, sizeof buf - (unsigned)n, "]}");
    return buf;
}

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

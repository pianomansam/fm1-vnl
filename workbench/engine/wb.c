/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The workbench's engine glue (wb.h). The render runs the song as the app does (app.c's step: the plan's filter
   moves, the last block's fade, the step's events to the voices at its start sample), without the app's mailbox,
   requests or gaps between songs: one song, looped after a 2 s gap at its end. Exports for the worklet follow
   base/web/emu/emu_web.c's pattern. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "wb.h"
#include "wb_drums.h"
#include "song.h"
#include "library.h"
#include "override.h"
#include "voices.h"
#include "harmony.h"

#define EXPORT __attribute__((used, visibility("default")))
#define SR 44100u
#define GAP (2u * SR)                   /* the silence between the song's end and its loop */

static song_t song;
/* Everything the page decides: the roll, the overrides, the custom set (LIB_SET_WB) and pattern (LIB_PATTERN_WB) */
typedef struct {
    song_roll_t roll;
    song_ov_t ov;
    lib_set_t set;
    lib_pattern_t pat;                  /* a kick phrase over a library pattern (id 0: none) */
    uint8_t base;                       /* the library set the custom one was copied from (its id-keyed rules) */
    uint8_t pat_base;                   /* the library pattern the custom one was copied from */
    int kick_a, kick_b;                 /* the kick phrase's step masks (0, 0: the pattern's own kick) */
    wb_drums_t drums;                   /* the drum lanes (wb_drums.h): heard only with the custom set (drums_of) */
} stage_t;
/* st: the page's (staged); com: wb_apply's (committed: the only one a landing reads, so staging is never heard
   half-done); live: playing (the hooks point into it); mark: wb_stage_mark's copy */
static stage_t st, com, live, mark;
static song_t scratch[2];               /* wb_lead_oct_probe's (a probe instance's, never the one playing) */
static uint8_t playing, apply_pend, start_pend, in_gap;
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

/* a stage's drum lanes: they go with its custom set (a new seed's set drops them) */
static const wb_drums_t *drums_of(const stage_t *s)
{
    return s->roll.v[RF_SET] == LIB_SET_WB ? &s->drums : NULL;
}

/* the library hooks into a stage's custom set and pattern */
static void hooks(const stage_t *s)
{
    lib_set_wb(&s->set, s->base);
    lib_pattern_wb(s->pat.id ? &s->pat : NULL);
}

/* The committed state becomes the song's: it starts again and ticks silently (events dropped) up to bar target (from
   0; past the end, bar 0), the old notes released, the voices configured for it. A new seed or map redraws
   everything: from bar 1. fresh: a new start (play, a seed): the engine cleared if nothing sounds, the chain
   restarted from the seed (vx_song). */
static void land(int target, int fresh)
{
    if (com.roll.seed != live.roll.seed || com.roll.v[RF_MAP] != live.roll.v[RF_MAP])
        target = 0;
    live = com;
    hooks(&live);
    wb_drums_set(drums_of(&live));
    song_override(&live.ov);
    song_start(&song, &live.roll);
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
        wb_drums_song(song.roll.seed);
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
        land(song.bar, com.roll.seed != live.roll.seed);
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
    memset(&st, 0, sizeof st);
    st.drums.preset = -1;
    song_override(NULL);
    st.ov = *song_ov();
    roll_song(0, 1, &st.roll);
    com = live = mark = st;
    hooks(&live);
    vx_init();
    wb_drums_init();
    playing = apply_pend = start_pend = in_gap = 0;
    catchups = 0;
    dice_salt = 0;
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
    roll_song(0, seed, &st.roll);
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
    hooks(on ? &st : &live);
}

EXPORT int wb_roll_get(int field)
{
    return field >= 0 && field < RF_N ? st.roll.v[field] : -1;
}

EXPORT int wb_roll_set(int field, int value)
{
    int v;
    stage_set(1);
    v = roll_set(&st.roll, field, value);
    stage_set(0);
    return v;
}

EXPORT int wb_roll_dice(int field)
{
    int ok;
    stage_set(1);
    ok = roll_dice(&st.roll, field, ++dice_salt);
    stage_set(0);
    return ok ? st.roll.v[field] : -1;
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
    if (st.roll.v[RF_SET] != LIB_SET_WB) {
        const lib_set_t *s = lib_set_by_id((uint8_t)st.roll.v[RF_SET]);
        if (!s)
            return NULL;
        st.set = *s;
        st.set.id = LIB_SET_WB;
        st.base = s->id;
        st.roll.v[RF_SET] = LIB_SET_WB;
        memset(&st.drums, 0, sizeof st.drums);  /* (any left from an earlier custom set) */
        st.drums.preset = -1;
    }
    return &st.set;
}

EXPORT void wb_role_set(int role, int ins)
{
    if (!role_ins(&st.set, role) || ins < 0 || ins > 255 || !custom_set())
        return;
    *role_ins(&st.set, role) = (uint8_t)ins;
}

EXPORT int wb_role_get(int role)
{
    const lib_set_t *p = st.roll.v[RF_SET] == LIB_SET_WB ? &st.set : lib_set_by_id((uint8_t)st.roll.v[RF_SET]);
    lib_set_t s;
    uint8_t *ins;
    if (!p)
        return -1;
    s = *p;
    ins = role_ins(&s, role);
    return ins ? *ins : -1;
}

/* the song's drum kit (KIT_*), as lib_song_kit draws it from the staged set; a preset's kit row value
   (WB_KIT_PRESET + its id) while one is staged */
EXPORT int wb_kit_get(void)
{
    int k;
    if (drums_of(&st) && st.drums.preset >= 0)
        return WB_KIT_PRESET + st.drums.preset;
    stage_set(1);
    k = lib_song_kit(&st.roll);
    stage_set(0);
    return k;
}

EXPORT void wb_kit_preset(int id);

/* the kit in the custom set, so lib_song_kit plays it always: a synth kit with no sampled one, or the reverse (the drum
   lanes cleared: the kit plays itself); a preset's kit row value stages the preset */
EXPORT void wb_kit_set(int kit)
{
    lib_set_t *s;
    if (kit >= WB_KIT_PRESET && kit < WB_KIT_PRESET + WB_KIT_PRESETS) {
        wb_kit_preset(kit - WB_KIT_PRESET);
        return;
    }
    if (kit < 0 || kit >= KIT_N || !(s = custom_set()))
        return;
    s->kit = lib_kit_sampled(kit) ? KIT_N : (uint8_t)kit;
    s->skit = lib_kit_sampled(kit) ? (uint8_t)kit : KIT_N;
    memset(&st.drums, 0, sizeof st.drums);
    st.drums.preset = -1;
}

/* A synth kit as shipped (WB_KIT_*: wb_drums_preset) on every lane, over the Brushes kit in the custom set (a synth
   kit's song: no ride section, its compressor) */
EXPORT void wb_kit_preset(int id)
{
    lib_set_t *s;
    if (id < 0 || id >= WB_KIT_PRESETS || !(s = custom_set()))
        return;
    s->kit = KIT_BRUSHES;
    s->skit = KIT_N;
    wb_drums_preset(&st.drums, id);
}

/* Lane (0..WB_LANES - 1: V_KICK's order) plays a source (WB_SRC_*; wb_drums_src_ok's), its level kept. The first
   lane set on a song with none copies the song's Brushes kit onto the others (BUILTIN; a sampled kit's: Task 9), so
   they keep playing; the preset stays named (the kit row), the lane its own. */
EXPORT void wb_lane_src(int lane, int kind, int a, int b)
{
    wb_src_t *s;
    int k, i;
    if (lane < 0 || lane >= WB_LANES || !wb_drums_src_ok(kind, a, b) || !custom_set())
        return;
    if (kind != WB_SRC_NONE && !wb_drums_any(&st.drums) && (k = wb_kit_get()) == KIT_BRUSHES)
        for (i = 0; i <= V_RIM - V_KICK; i++)
            st.drums.lane[i] = (wb_src_t){WB_SRC_BUILTIN, (uint8_t)k, (uint8_t)i, st.drums.lane[i].gain_db};
    s = &st.drums.lane[lane];
    s->kind = (uint8_t)kind;
    s->a = (uint8_t)a;
    s->b = (uint8_t)b;
}

/* lane's level in dB (-40..+12) */
EXPORT void wb_lane_db(int lane, float db)
{
    if (lane < 0 || lane >= WB_LANES || !custom_set())
        return;
    st.drums.lane[lane].gain_db = db < -40.0f ? -40.0f : db > 12.0f ? 12.0f : db;
}

/* lane's sound now (vel 1-127) as staged, outside the song, through the drums' chain (stopped too): the song's own
   kit's when no lane has a source */
EXPORT void wb_audition(int lane, int vel)
{
    const wb_drums_t *d = drums_of(&st);
    wb_src_t none = {WB_SRC_NONE, 0, 0, 0.0f};
    if (lane < 0 || lane >= WB_LANES)
        return;
    vel = vel < 1 ? 1 : vel > 127 ? 127 : vel;
    if (!wb_drums_any(d))
        wb_drums_audition(&none, lane, vel);
    else if (d->lane[lane].kind != WB_SRC_NONE)
        wb_drums_audition(&d->lane[lane], lane, vel);
}

/* A two-bar kick phrase (step masks for bars A and B) over the staged pattern: the custom pattern copies the library
   one (both bars alike) with its kick lane replaced and unghosted; 0, 0 returns to the library pattern itself */
EXPORT void wb_kick_phrase(int a, int b)
{
    const lib_pattern_t *p;
    int i;
    if (st.roll.v[RF_PATTERN] != LIB_PATTERN_WB)
        st.pat_base = (uint8_t)st.roll.v[RF_PATTERN];
    if (!a && !b) {
        st.roll.v[RF_PATTERN] = st.pat_base;
        return;
    }
    if (!(p = lib_pattern_by_id(st.pat_base)))
        return;
    st.pat = *p;
    st.pat.id = LIB_PATTERN_WB;
    st.pat.two_bar = 1;
    for (i = 0; i < 6; i++) {
        st.pat.hit_b[i] = p->hit[i];
        st.pat.ghost_b[i] = p->ghost[i];
    }
    st.pat.hit[D_KICK] = (uint16_t)a;
    st.pat.hit_b[D_KICK] = (uint16_t)b;
    st.pat.ghost[D_KICK] = st.pat.ghost_b[D_KICK] = 0;
    st.kick_a = a;
    st.kick_b = b;
    st.roll.v[RF_PATTERN] = LIB_PATTERN_WB;
}

/* 1 if roll_set would take value for field as staged (the dropdowns offer only these) */
EXPORT int wb_roll_legal(int field, int value)
{
    song_roll_t t = st.roll;
    int v;
    stage_set(1);
    v = roll_set(&t, field, value);
    stage_set(0);
    return v == value;
}

/* the staged state saved, and put back (a change that would move a locked row is undone whole) */
EXPORT void wb_stage_mark(void) { mark = st; }
EXPORT void wb_stage_undo(void) { st = mark; }

/* the staged state's bytes, for a probe instance (wb_lead_oct_probe): the page copies them across (the same build,
   so the library pointers inside hold) */
EXPORT void *wb_stage_ptr(void) { return &st; }
EXPORT int wb_stage_size(void) { return (int)sizeof st; }

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

/* The staged lead_oct as the engine applies it: itself if it moves any lead note (lead.c keeps the register where the
   instrument can't move), else 0. The staged song with and without it, run side by side to the first difference.
   Scratch runs share song.c's per-bar state with the song playing: call it on a probe instance only. */
EXPORT int wb_lead_oct_probe(void)
{
    song_ov_t flat = st.ov;
    int moved = 0;
    if (!st.ov.lead_oct)
        return 0;
    flat.lead_oct = 0;
    stage_set(1);
    song_override(&st.ov);
    song_start(&scratch[0], &st.roll);
    song_start(&scratch[1], &st.roll);
    while (!moved && scratch[0].state == SS_PLAYING && scratch[1].state == SS_PLAYING) {
        uint32_t h = lead_notes(&scratch[0]);
        song_override(&flat);
        moved = h != lead_notes(&scratch[1]);
        song_override(&st.ov);
    }
    song_override(&live.ov);
    stage_set(0);
    return moved ? st.ov.lead_oct : 0;
}

EXPORT void wb_ov_set(int key, int value)
{
    song_ov_t *o = &st.ov;
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
    const song_ov_t *o = &st.ov;
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

/* The keys' register as the engine applies it to the staged state, -1, 0 or +1 (harmony_keys_oct: 0 where the
   instrument's range can't hold the move) */
EXPORT int wb_keys_oct_applied(void)
{
    int d;
    stage_set(1);
    song_override(&st.ov);
    d = harmony_keys_oct(&st.roll) / 12;
    song_override(&live.ov);
    stage_set(0);
    return d;
}

EXPORT void wb_apply(void)
{
    com = st;
    if (playing)
        apply_pend = 1;                 /* stopped: play lands it */
}

EXPORT int wb_bar(void) { return cur_bar + 1; }
EXPORT int wb_bars(void) { return cur_bar >= 0 ? live.roll.v[RF_BARS] : com.roll.v[RF_BARS]; }
EXPORT float wb_seconds(void) { return (float)pos / (float)SR; }
EXPORT int wb_catchups(void) { return catchups; }
/* the song playing (as last landed): a roll field, or the seed for RF_N */
EXPORT int wb_live_get(int field)
{
    return field == RF_N ? (int)live.roll.seed : field >= 0 && field < RF_N ? live.roll.v[field] : -1;
}

/* JSON writers: append, stopping at the buffer's end (out_ok: nothing was cut) */
typedef struct { char *b; unsigned cap, n; } out_t;

static void put(out_t *o, const char *fmt, ...)
{
    va_list ap;
    int k;
    if (o->n >= o->cap)
        return;
    va_start(ap, fmt);
    k = vsnprintf(o->b + o->n, o->cap - o->n, fmt, ap);
    va_end(ap);
    o->n = k < 0 || (unsigned)k >= o->cap - o->n ? o->cap : o->n + (unsigned)k;
}

static void put_str(out_t *o, const char *s)
{
    put(o, "\"");
    for (; *s; s++)
        put(o, *s == '"' || *s == '\\' ? "\\%c" : "%c", *s);
    put(o, "\"");
}

/* the buffer, or an error object if it was cut */
static const char *out_done(out_t *o)
{
    return o->n < o->cap ? o->b : "{\"error\":\"truncated\"}";
}

/* JSON: the staged state the page shows (the roll, the roles, the overrides, the kit, the library pattern under a
   kick phrase and the phrase's masks), and the library's maps and progressions with their notes */
EXPORT const char *wb_state(void)
{
    static char buf[1024];
    out_t o = {buf, sizeof buf, 0};
    int i, custom = st.roll.v[RF_PATTERN] == LIB_PATTERN_WB;
    const wb_drums_t *d = drums_of(&st);
    put(&o, "{\"seed\":%u,\"roll\":[", (unsigned)st.roll.seed);
    for (i = 0; i < RF_N; i++)
        put(&o, "%s%d", i ? "," : "", st.roll.v[i]);
    put(&o, "],\"roles\":[");
    for (i = V_CHORD; i <= V_PAD; i++)
        put(&o, "%s%d", i ? "," : "", wb_role_get(i));
    put(&o, "],\"ov\":[");
    for (i = 0; i < WB_OV_N; i++)
        put(&o, "%s%d", i ? "," : "", wb_ov_get(i));
    put(&o, "],\"kit\":%d,\"pattern\":%d,\"kick\":[%d,%d]", wb_kit_get(),
        custom ? st.pat_base : st.roll.v[RF_PATTERN], custom ? st.kick_a : 0, custom ? st.kick_b : 0);
    put(&o, ",\"drums\":{\"preset\":%d,\"lanes\":[", d ? d->preset : -1);
    for (i = 0; i < WB_LANES; i++) {
        const wb_src_t *s = d ? &d->lane[i] : NULL;
        put(&o, "%s[%d,%d,%d,%g]", i ? "," : "", s ? s->kind : 0, s ? s->a : 0, s ? s->b : 0, s ? (double)s->gain_db : 0.0);
    }
    put(&o, "]}}");
    return out_done(&o);
}

/* wb_state's JSON for the song being heard (live once it has landed, else the committed one) */
EXPORT const char *wb_live_state(void)
{
    stage_t save = st;
    const char *s;
    st = cur_bar >= 0 ? live : com;
    s = wb_state();
    st = save;
    return s;
}

EXPORT int wb_pending(void) { return apply_pend; }   /* a commit is waiting for its bar line */

EXPORT const char *wb_library(void)
{
    static char buf[8192];
    static const char *done;
    out_t o = {buf, sizeof buf, 0};
    int i;
    if (done)
        return done;
    put(&o, "{\"maps\":[");
    for (i = 0; i < LIB_NMAPS; i++) {
        put(&o, "%s{\"id\":%d,\"name\":", i ? "," : "", LIB_MAPS[i].id);
        put_str(&o, LIB_MAPS[i].name);
        put(&o, "}");
    }
    put(&o, "],\"progs\":[");
    for (i = 0; i < LIB_NPROGS; i++) {
        put(&o, "%s{\"id\":%d,\"src\":", i ? "," : "", LIB_PROGS[i].id);
        put_str(&o, LIB_PROGS[i].src);
        put(&o, "}");
    }
    put(&o, "]}");
    return done = out_done(&o);
}

EXPORT const char *wb_consts(void)
{
    typedef struct { const char *name; int v; } kv_t;
    static const kv_t RF[] = {
        {"KEY", RF_KEY}, {"TEMPO", RF_TEMPO}, {"MOTION", RF_MOTION}, {"MOOD", RF_MOOD}, {"MODE", RF_MODE},
        {"MAP", RF_MAP}, {"PROG", RF_PROG}, {"COLOR", RF_COLOR}, {"SET", RF_SET}, {"PATTERN", RF_PATTERN},
        {"SWING", RF_SWING}, {"DUST", RF_DUST}, {"NOISE", RF_NOISE}, {"AMBIENCE", RF_AMBIENCE}, {"BARS", RF_BARS},
        {"N", RF_N}}, OV[] = {
        {"COMP", WB_OV_COMP}, {"LEAD_STYLE", WB_OV_LEAD_STYLE}, {"PAD_PLACE", WB_OV_PAD_PLACE}, {"RIDE", WB_OV_RIDE},
        {"KEYS_OCT", WB_OV_KEYS_OCT}, {"LEAD_OCT", WB_OV_LEAD_OCT}, {"VOICING_SALT", WB_OV_VOICING_SALT},
        {"PHRASE_SALT", WB_OV_PHRASE_SALT}, {"N", WB_OV_N}}, V[] = {
        {"CHORD", V_CHORD}, {"BASS", V_BASS}, {"LEAD", V_LEAD}, {"CANDY", V_CANDY}, {"PAD", V_PAD}}, SRC[] = {
        {"NONE", WB_SRC_NONE}, {"PT", WB_SRC_PT}, {"OPT", WB_SRC_OPT}, {"SAMPLE", WB_SRC_SAMPLE},
        {"BUILTIN", WB_SRC_BUILTIN}}, KIT[] = {
        {"PRESET", WB_KIT_PRESET}, {"PT808", WB_KIT_PT808}, {"PTCR78", WB_KIT_PTCR78}, {"OPTLOFI", WB_KIT_OPTLOFI},
        {"PRESETS", WB_KIT_PRESETS}, {"LANES", WB_LANES}};
    static const struct { const char *key; const kv_t *t; unsigned n; } T[] = {
        {"RF", RF, sizeof RF / sizeof RF[0]}, {"OV", OV, sizeof OV / sizeof OV[0]}, {"V", V, sizeof V / sizeof V[0]},
        {"SRC", SRC, sizeof SRC / sizeof SRC[0]}, {"DRUMS", KIT, sizeof KIT / sizeof KIT[0]}};
    static char buf[2048];
    static const char *done;
    out_t o = {buf, sizeof buf, 0};
    unsigned i, j;
    if (done)
        return done;
    put(&o, "{");
    for (j = 0; j < sizeof T / sizeof T[0]; j++) {
        put(&o, "\"%s\":{", T[j].key);
        for (i = 0; i < T[j].n; i++)
            put(&o, "%s\"%s\":%d", i ? "," : "", T[j].t[i].name, T[j].t[i].v);
        put(&o, "},");
    }
    put(&o, "\"LIB_SET_WB\":%d}", LIB_SET_WB);
    return done = out_done(&o);
}

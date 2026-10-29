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
#include "patterns_gen.h"
#include "voices.h"
#include "harmony.h"
#include "poketrack/trommelsynthesizer.h"

#define EXPORT __attribute__((used, visibility("default")))
#define SR 44100u
#define GAP (2u * SR)                   /* the silence between the song's end and its loop */

static song_t song;
/* Everything the page decides: the roll, the overrides, the custom set (LIB_SET_WB) and pattern (LIB_PATTERN_WB) */
typedef struct {
    song_roll_t roll;
    song_ov_t ov;
    lib_set_t set;
    lib_pattern_t pat;                  /* a library pattern copied as two bars, then edited (id 0: none) */
    uint8_t base;                       /* the library set the custom one was copied from (its id-keyed rules) */
    uint8_t pat_base;                   /* the library pattern the custom one was copied from */
    uint8_t grid;                       /* the custom pattern has the grid's edits (not just a kick phrase) */
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
static int jump = -1;                   /* wb_jump's bar, for the next landing (-1: none) */
static unsigned mute;                   /* wb_mute's mask */
static int solo = -1;                   /* wb_solo's role */
static uint32_t heard_h, heard_n;       /* the events tick() played since the last landing, hashed (wb_heard_hash) */
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
    if (jump >= 0)
        target = jump;                  /* (a jump: the page's bar, in the committed song) */
    else if (com.roll.seed != live.roll.seed || com.roll.v[RF_MAP] != live.roll.v[RF_MAP])
        target = 0;
    jump = -1;
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
    wb_drums_kit(lib_song_kit(&song.roll));
    vx_set_gain(1.0f);
    cur_bar = target - 1;
    next_at = 0;
    apply_pend = in_gap = 0;
    heard_h = 2166136261u;
    heard_n = 0;
}

/* an event's (bar, step, role, note) into an FNV-1a hash (wb_heard_hash; the page's events hash alike) */
static uint32_t ev_hash(uint32_t h, int bar, int step, int role, int note)
{
    h = (h ^ (uint32_t)bar) * 16777619u;
    h = (h ^ (uint32_t)step) * 16777619u;
    h = (h ^ (uint32_t)role) * 16777619u;
    return (h ^ (uint32_t)note) * 16777619u;
}

/* an event's role and note as wb_events records them: a drum's note is its lane */
static int ev_note(const song_ev_t *e)
{
    return e->voice >= V_KICK ? e->voice - V_KICK : e->note;
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
    vx_set_pad_level(song_pad_level(&song));   /* the pad layer's fade over a section line */
    for (i = 0; i < n; i++) {
        vx_event(&evs[i]);
        heard_h = ev_hash(heard_h, bar, st, evs[i].voice, ev_note(&evs[i]));
    }
    heard_n += (uint32_t)n;
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
    jump = solo = -1;
    mute = 0;
    vx_mute(0);
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
        resume_bar = jump >= 0 ? jump : cur_bar > 0 ? cur_bar : 0;   /* the start of the bar sounding (or a jump's) */
        vx_stop();
    }
}

/* A jump to bar (from 0: wb_sections' start) of the committed song: playing, the next landing goes there at the next
   bar line, with any commit waiting for it (one catch-up); stopped, play starts there */
EXPORT void wb_jump(int bar)
{
    jump = bar < 0 ? 0 : bar;
    if (playing)
        apply_pend = 1;
    else
        resume_bar = jump;
}

/* the role buses wb_mute and wb_solo cover: vx_mute's bits (the drums' on V_KICK) */
#define WB_BUSES (1u << V_CHORD | 1u << V_BASS | 1u << V_LEAD | 1u << V_PAD | 1u << V_KICK)

/* listening only, heard at once (no landing), kept across landings: a solo mutes every other bus (the mutes set
   aside while it lasts) */
static void listen(void)
{
    int bit = solo >= V_KICK ? V_KICK : solo;
    vx_mute(solo >= 0 ? WB_BUSES & ~(1u << bit) : mute & WB_BUSES);
}

EXPORT void wb_mute(unsigned mask)
{
    mute = mask;
    listen();
}

/* role (V_CHORD, V_BASS, V_LEAD, V_PAD, or a drum voice for the drums) alone; -1 none */
EXPORT void wb_solo(int role)
{
    solo = role >= 0 && role < V_N && (WB_BUSES >> (role >= V_KICK ? V_KICK : role)) & 1u ? role : -1;
    listen();
}

EXPORT uint32_t wb_heard_hash(void) { return heard_h; }
EXPORT int wb_heard_count(void) { return (int)heard_n; }

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

/* a removed lead (an old snapshot's: the muted trumpet, sax, guitar lick, string lead, cello) lands as the approved
   lead library.c stands in for it */
EXPORT void wb_role_set(int role, int ins)
{
    if (!role_ins(&st.set, role) || ins < 0 || ins > 255 || !custom_set())
        return;
    if (role == V_LEAD)
        ins = lib_lead_approved(ins);
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

/* A kit as shipped (WB_KIT_*: wb_drums_preset) on every lane; nothing staged if it can't play (a sampled kit not
   loaded). The custom set's kit under it: Brushes for a synth machine's (a synth kit's song: no ride section, its
   compressor), the mixed Rusty for a sampled one's (a sampled kit's song: the ride section, the hats' eighths) */
EXPORT void wb_kit_preset(int id)
{
    lib_set_t *s;
    wb_drums_t d;
    if (!wb_drums_preset(&d, id) || !(s = custom_set()))
        return;
    s->kit = WB_KIT_SAMPLED(id) ? KIT_N : KIT_BRUSHES;
    s->skit = WB_KIT_SAMPLED(id) ? KIT_S_RUSTY : KIT_N;
    st.drums = d;
}

/* the staged kit's lanes into d (levels kept): a preset's, or a synth kit's (BUILTIN); 0 for a mixed sampled kit
   (retired: its hits are processed copies of pieces, none of them a lane source) */
static int kit_lanes(wb_drums_t *d)
{
    wb_drums_t k;
    int kit = wb_kit_get(), i;
    if (kit >= WB_KIT_PRESET) {
        if (!wb_drums_preset(&k, kit - WB_KIT_PRESET))
            return 0;
    } else if (kit >= 0 && kit <= KIT_BRUSHES) {
        memset(&k, 0, sizeof k);
        for (i = 0; i <= V_RIM - V_KICK; i++)
            k.lane[i] = (wb_src_t){WB_SRC_BUILTIN, (uint8_t)kit, (uint8_t)i, 0.0f};
    } else {
        return 0;
    }
    for (i = 0; i < WB_LANES; i++) {
        k.lane[i].gain_db = d->lane[i].gain_db;
        d->lane[i] = k.lane[i];
    }
    return 1;
}

/* Lane (0..WB_LANES - 1: V_KICK's order) plays a source (WB_SRC_*; wb_drums_src_ok's), its level kept: 1, or 0 if
   refused. The first lane set on a song with none copies the song's kit onto the others (kit_lanes), so they keep
   playing; a mixed sampled kit's song refuses it (choose a shipped kit first). The preset stays named (the kit row),
   the lane its own. */
EXPORT int wb_lane_src(int lane, int kind, int a, int b)
{
    wb_src_t *s;
    if (lane < 0 || lane >= WB_LANES || !wb_drums_src_ok(kind, a, b))
        return 0;
    if (kind != WB_SRC_NONE && !wb_drums_any(drums_of(&st))) {
        wb_drums_t d = drums_of(&st) ? st.drums : (wb_drums_t){.preset = -1};
        if (!kit_lanes(&d) || !custom_set())
            return 0;
        st.drums = d;
    } else if (!custom_set()) {
        return 0;
    }
    s = &st.drums.lane[lane];
    s->kind = (uint8_t)kind;
    s->a = (uint8_t)a;
    s->b = (uint8_t)b;
    return 1;
}

/* lane's level in dB (-40..+12; not a number: unchanged); on a song with no lane sources the kit's are copied first
   (as wb_lane_src's), so the level is heard */
EXPORT void wb_lane_db(int lane, float db)
{
    wb_drums_t d = drums_of(&st) ? st.drums : (wb_drums_t){.preset = -1};
    if (lane < 0 || lane >= WB_LANES || db != db || !custom_set())
        return;
    if (!wb_drums_any(&d) && kit_lanes(&d))
        st.drums = d;
    st.drums.lane[lane].gain_db = db < -40.0f ? -40.0f : db > 12.0f ? 12.0f : db;
}

/* lane's sound now (vel 1-127) as staged, outside the song, through the drums' chain (stopped too): the staged kit's
   when no lane has a source */
EXPORT void wb_audition(int lane, int vel)
{
    const wb_drums_t *d = drums_of(&st);
    wb_drums_t k = {.preset = -1};
    if (lane < 0 || lane >= WB_LANES)
        return;
    vel = vel < 1 ? 1 : vel > 127 ? 127 : vel;
    if (wb_drums_any(d))
        wb_drums_audition(&d->lane[lane], lane, vel);
    else if (kit_lanes(&k))
        wb_drums_audition(&k.lane[lane], lane, vel);
    else
        wb_drums_audition_kit(wb_kit_get(), lane, vel);
}

/* the staged custom pattern, the song's library pattern copied into it (bar B as bar A) on the first edit
   (RF_PATTERN switches to LIB_PATTERN_WB); NULL if the song's pattern is unknown */
static lib_pattern_t *custom_pat(void)
{
    const lib_pattern_t *p;
    int i;
    if (st.roll.v[RF_PATTERN] == LIB_PATTERN_WB)
        return &st.pat;
    if (!(p = lib_pattern_by_id((uint8_t)st.roll.v[RF_PATTERN])))
        return NULL;
    st.pat = *p;
    st.pat.id = LIB_PATTERN_WB;
    st.pat.two_bar = 1;
    for (i = 0; i < 6; i++) {
        st.pat.hit_b[i] = (uint16_t)lib_lane_hits(p, i, 1);
        st.pat.ghost_b[i] = (uint16_t)lib_lane_ghosts(p, i, 1);
    }
    st.pat_base = p->id;
    st.grid = 0;
    st.roll.v[RF_PATTERN] = LIB_PATTERN_WB;
    return &st.pat;
}

/* the staged pattern the song plays (the custom one, or its library pattern) */
static const lib_pattern_t *staged_pat(void)
{
    return st.roll.v[RF_PATTERN] == LIB_PATTERN_WB ? &st.pat : lib_pattern_by_id((uint8_t)st.roll.v[RF_PATTERN]);
}

/* a step of p's lane in bar 0 (A) or 1 (B): 0 empty, 1 hit, 2 ghost (groove.c's reading: a hit over a ghost; a poly3
   shaker's three-step cycle as it falls in the song's first two bars) */
static int cell(const lib_pattern_t *p, int lane, int bar01, int step)
{
    if (p->poly3 && lane == D_SHAKER)
        return (bar01 * 16 + step) % 3 == 0;
    if ((lib_lane_hits(p, lane, bar01) >> step) & 1)
        return 1;
    return (lib_lane_ghosts(p, lane, bar01) >> step) & 1 ? 2 : 0;
}

/* a lane's masks in the custom pattern; a poly3 shaker becomes its first two bars' steps (a two-bar loop) */
static void set_lane(lib_pattern_t *p, int lane, unsigned ha, unsigned ga, unsigned hb, unsigned gb)
{
    p->hit[lane] = (uint16_t)ha;
    p->ghost[lane] = (uint16_t)ga;
    p->hit_b[lane] = (uint16_t)hb;
    p->ghost_b[lane] = (uint16_t)gb;
    if (lane == D_SHAKER)
        p->poly3 = 0;
}

static void lane_masks(const lib_pattern_t *p, int lane, unsigned m[4])
{
    int b, i, c;
    m[0] = m[1] = m[2] = m[3] = 0;
    for (b = 0; b < 2; b++)
        for (i = 0; i < 16; i++)
            if ((c = cell(p, lane, b, i)))
                m[b * 2 + (c == 2)] |= 1u << i;
}

/* A two-bar kick phrase (step masks for bars A and B) on the staged pattern's kick lane, unghosted; 0, 0: the library
   pattern's own kick (and the library pattern itself if the grid hasn't edited the custom one) */
EXPORT void wb_kick_phrase(int a, int b)
{
    lib_pattern_t *p;
    unsigned m[4];
    if (!a && !b) {
        const lib_pattern_t *base = lib_pattern_by_id(st.pat_base);
        if (st.roll.v[RF_PATTERN] != LIB_PATTERN_WB || !base)
            return;
        if (!st.grid) {
            st.roll.v[RF_PATTERN] = st.pat_base;
            return;
        }
        lane_masks(base, D_KICK, m);
        set_lane(&st.pat, D_KICK, m[0], m[1], m[2], m[3]);
        return;
    }
    if ((p = custom_pat()))
        set_lane(p, D_KICK, (unsigned)a & 0xFFFFu, 0, (unsigned)b & 0xFFFFu, 0);
}

/* The grid: lane (D_KICK..D_RIM), bar 0 (A) or 1 (B), step 0-15 to state (0 empty, 1 hit, 2 ghost) in the custom
   pattern (the song switches to it on the first edit); staged, so it lands at the next bar line after wb_apply */
EXPORT void wb_pat_step(int lane, int bar01, int step, int state)
{
    lib_pattern_t *p;
    unsigned m[4], bit = 1u << (step & 15);
    if (lane < 0 || lane > D_RIM || bar01 < 0 || bar01 > 1 || step < 0 || step > 15 || state < 0 || state > 2 ||
        !(p = custom_pat()))
        return;
    lane_masks(p, lane, m);
    m[bar01 * 2] = state == 1 ? m[bar01 * 2] | bit : m[bar01 * 2] & ~bit;
    m[bar01 * 2 + 1] = state == 2 ? m[bar01 * 2 + 1] | bit : m[bar01 * 2 + 1] & ~bit;
    set_lane(p, lane, m[0], m[1], m[2], m[3]);
    st.grid = 1;
}

/* a step of the staged pattern (wb_pat_step's states; -1 out of range) */
EXPORT int wb_pat_get(int lane, int bar01, int step)
{
    const lib_pattern_t *p = staged_pat();
    if (!p || lane < 0 || lane > D_RIM || bar01 < 0 || bar01 > 1 || step < 0 || step > 15)
        return -1;
    return cell(p, lane, bar01, step);
}

/* a library pattern into the custom one, the grid's to edit (a saved pattern's base): 1, or 0 if roll_set wouldn't
   take it for the staged song (nothing staged) */
EXPORT int wb_pat_load(int id)
{
    if (id < 0 || id >= LIB_PATTERN_WB || !wb_roll_legal(RF_PATTERN, id))
        return 0;
    st.roll.v[RF_PATTERN] = id;
    custom_pat();
    st.grid = 1;
    return 1;
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
/* the committed state's, likewise (wb_events) */
EXPORT void *wb_commit_ptr(void) { return &com; }

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

/* wb_events' records: every event of the committed song (8 bytes each, little-endian) */
typedef struct __attribute__((packed)) { uint16_t bar; uint8_t step, role, note, vel; uint16_t len_steps; } wb_ev_t;
#define WB_EV_MAX 32768
static wb_ev_t events[WB_EV_MAX];
static int nevents;

/* wb_chords' records: where the chord changes (the root, a pitch class), from the same run */
typedef struct __attribute__((packed)) { uint16_t bar; uint8_t step, root; } wb_chord_t;
static wb_chord_t chords[1024];
static int nchords;

/* The committed song run silently from bar 1 (as a landing starts it), every event recorded: the count, or -1 if
   there were more than max (or WB_EV_MAX; max 0: WB_EV_MAX) and the rest were dropped. A drum's role is its voice
   (V_KICK..V_RIDE) and its note the lane (voice - V_KICK); len_steps the length in 16ths, rounded. Runs a scratch
   song: a probe instance only, as wb_lead_oct_probe. */
EXPORT int wb_events(uint32_t max)
{
    song_t *s = &scratch[0];
    int over = 0;
    if (!max || max > WB_EV_MAX)
        max = WB_EV_MAX;
    nevents = nchords = 0;
    hooks(&com);
    song_override(&com.ov);
    song_start(s, &com.roll);
    while (!over && s->state == SS_PLAYING) {
        int bar = s->bar, step = s->step, n, i;
        float ss;
        if (!step) {   /* a bar: its chord, kept if it differs from the last (the pitch classes, root first) */
            const plan_block_t *b = plan_at(&s->plan, bar);
            const lib_prog_t *p = lib_prog_by_id((uint8_t)s->roll.v[RF_PROG]);
            int loop = p && p->n * p->bars_per_chord ? p->n * p->bars_per_chord : 4, pcs[5], np;
            static int last[5], nlast;
            np = harmony_chord_at(&s->roll, bar, bar / loop + 1, b && (b->flags & PB_DARK), pcs);
            if (np > 0 && nchords < 1024 && (!nchords || np != nlast || memcmp(pcs, last, np * sizeof *pcs))) {
                chords[nchords].bar = (uint16_t)bar;
                chords[nchords].step = 0;
                chords[nchords++].root = (uint8_t)pcs[0];
                memcpy(last, pcs, np * sizeof *pcs);
                nlast = np;
            }
        }
        n = song_tick(s, evs, 64);
        ss = song_step_samples(s);
        for (i = 0; i < n; i++) {
            wb_ev_t *r = &events[nevents];
            if ((uint32_t)nevents >= max) {
                over = 1;
                break;
            }
            r->bar = (uint16_t)bar;
            r->step = (uint8_t)step;
            r->role = evs[i].voice;
            r->note = (uint8_t)ev_note(&evs[i]);
            r->vel = evs[i].vel;
            r->len_steps = (uint16_t)((float)evs[i].len / ss + 0.5f);
            nevents++;
        }
    }
    song_override(&live.ov);
    hooks(&live);
    return over ? -1 : nevents;
}

EXPORT uint32_t wb_events_ptr(void) { return (uint32_t)(uintptr_t)events; }
EXPORT int wb_events_count(void) { return nevents; }
/* the chord changes of the last wb_events run: 4-byte records { u16 bar; u8 step; u8 root pitch class } */
EXPORT uint32_t wb_chords_ptr(void) { return (uint32_t)(uintptr_t)chords; }
EXPORT int wb_chords_count(void) { return nchords; }

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
    case WB_OV_BASS_SALT: o->bass_salt = (uint32_t)value; break;
    case WB_OV_PAD_SALT: o->pad_salt = (uint32_t)value; break;
    case WB_OV_DRUMS_SALT: o->drums_salt = (uint32_t)value; break;
    case WB_OV_PAD_ON: o->pad_on = (int8_t)value; break;
    case WB_OV_LEAD_DOUBLE: o->lead_double = (int8_t)value; break;
    case WB_OV_STRUM_PAT: o->strum_pat = (int8_t)value; break;
    case WB_OV_ARP_RHY: o->arp_rhy = (int8_t)value; break;
    case WB_OV_ARP_PAT: o->arp_pat = (int8_t)value; break;
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
    case WB_OV_BASS_SALT: return (int)o->bass_salt;
    case WB_OV_PAD_SALT: return (int)o->pad_salt;
    case WB_OV_DRUMS_SALT: return (int)o->drums_salt;
    case WB_OV_PAD_ON: return o->pad_on;
    case WB_OV_LEAD_DOUBLE: return o->lead_double;
    case WB_OV_STRUM_PAT: return o->strum_pat;
    case WB_OV_ARP_RHY: return o->arp_rhy;
    case WB_OV_ARP_PAT: return o->arp_pat;
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

/* The keys' comping style (CS_*) as the engine applies it to the staged state: the override's, else the song's roll */
EXPORT int wb_comp_applied(void)
{
    int cs;
    stage_set(1);
    song_override(&st.ov);
    cs = harmony_style(&st.roll);
    song_override(&live.ov);
    stage_set(0);
    return cs;
}

/* a seed or map change is a new song: a jump staged before it (a bar of the old song's sections) is dropped, so
   the song starts at bar 1 (in this call: no bar line can land between the two) */
EXPORT void wb_apply(void)
{
    if (st.roll.seed != com.roll.seed || st.roll.v[RF_MAP] != com.roll.v[RF_MAP]) {
        if (jump >= 0 && !playing)
            resume_bar = 0;
        jump = -1;
    }
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

/* JSON: the staged state the page shows: the roll, the roles, the overrides, the kit, the library pattern under the
   custom one, the kick lane where it isn't the pattern's own ([A, B hits, 1], or with ghosts [.., 2]), the drum
   lanes, the staged kit's own (kit_lanes: what a first lane edit copies; null for a mixed sampled kit) and the grid
   (the staged pattern's lanes, [A hits, A ghosts, B hits, B ghosts] each; edited: it has the grid's edits) */
EXPORT const char *wb_state(void)
{
    static char buf[2048];
    out_t o = {buf, sizeof buf, 0};
    int i, custom = st.roll.v[RF_PATTERN] == LIB_PATTERN_WB;
    const wb_drums_t *d = drums_of(&st);
    const lib_pattern_t *p = staged_pat();
    const lib_pattern_t *base = lib_pattern_by_id(custom ? st.pat_base : (uint8_t)st.roll.v[RF_PATTERN]);
    unsigned m[4] = {0}, k[4] = {0};
    wb_drums_t kl = {.preset = -1};
    if (p && base) {
        lane_masks(p, D_KICK, m);
        lane_masks(base, D_KICK, k);
    }
    put(&o, "{\"seed\":%u,\"roll\":[", (unsigned)st.roll.seed);
    for (i = 0; i < RF_N; i++)
        put(&o, "%s%d", i ? "," : "", st.roll.v[i]);
    put(&o, "],\"roles\":[");
    for (i = V_CHORD; i <= V_PAD; i++)
        put(&o, "%s%d", i ? "," : "", wb_role_get(i));
    put(&o, "],\"ov\":[");
    for (i = 0; i < WB_OV_N; i++)
        put(&o, "%s%d", i ? "," : "", wb_ov_get(i));
    put(&o, "],\"kit\":%d,\"pattern\":%d,", wb_kit_get(), custom ? st.pat_base : st.roll.v[RF_PATTERN]);
    if (!memcmp(m, k, sizeof m))
        put(&o, "\"kick\":[0,0,0]");
    else
        put(&o, "\"kick\":[%u,%u,%d]", m[0], m[2], m[1] || m[3] ? 2 : 1);
    put(&o, ",\"drums\":{\"preset\":%d,\"lanes\":[", d ? d->preset : -1);
    for (i = 0; i < WB_LANES; i++) {
        const wb_src_t *s = d ? &d->lane[i] : NULL;
        put(&o, "%s[%d,%d,%d,%g]", i ? "," : "", s ? s->kind : 0, s ? s->a : 0, s ? s->b : 0, s ? (double)s->gain_db : 0.0);
    }
    put(&o, "],\"kit_lanes\":");
    if (kit_lanes(&kl)) {
        for (i = 0; i < WB_LANES; i++)
            put(&o, "%s[%d,%d,%d]", i ? "," : "[", kl.lane[i].kind, kl.lane[i].a, kl.lane[i].b);
        put(&o, "]");
    } else {
        put(&o, "null");
    }
    put(&o, ",\"grid\":{\"base\":%d,\"edited\":%d,\"lanes\":[", base ? base->id : 0, custom && st.grid);
    for (i = 0; p && i <= D_RIM; i++) {
        lane_masks(p, i, m);
        put(&o, "%s[%u,%u,%u,%u]", i ? "," : "", m[0], m[1], m[2], m[3]);
    }
    put(&o, "]}}}");
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

/* JSON: the committed song's sections (its plan's blocks): [{"start": bar from 0, "bars", "name"}]; heard: the song
   playing (as last landed) instead, once it has landed */
EXPORT const char *wb_sections(int heard)
{
    const stage_t *src = heard && cur_bar >= 0 ? &live : &com;
    static char buf[1536];
    static song_plan_t p;
    out_t o = {buf, sizeof buf, 0};
    int i;
    hooks(src);
    plan_build(&src->roll, &p);
    hooks(&live);
    put(&o, "[");
    for (i = 0; i < p.n; i++) {
        put(&o, "%s{\"start\":%d,\"bars\":%d,\"name\":", i ? "," : "", p.b[i].start, p.b[i].bars);
        put_str(&o, plan_section_name(p.b[i].section));
        put(&o, "}");
    }
    put(&o, "]");
    return out_done(&o);
}

EXPORT const char *wb_library(void)
{
    static char buf[16384];             /* about 100 B a progression: room for the imported ones */
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
        put(&o, "%s{\"id\":%d,\"status\":\"%s\",\"src\":", i ? "," : "", LIB_PROGS[i].id,
            LIB_PROGS_STATUS[i] == 1 ? "approved" : LIB_PROGS_STATUS[i] == 2 ? "retired" : "candidate");
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
        {"PHRASE_SALT", WB_OV_PHRASE_SALT}, {"BASS_SALT", WB_OV_BASS_SALT}, {"PAD_SALT", WB_OV_PAD_SALT},
        {"DRUMS_SALT", WB_OV_DRUMS_SALT}, {"PAD_ON", WB_OV_PAD_ON}, {"LEAD_DOUBLE", WB_OV_LEAD_DOUBLE},
        {"STRUM_PAT", WB_OV_STRUM_PAT}, {"ARP_RHY", WB_OV_ARP_RHY}, {"ARP_PAT", WB_OV_ARP_PAT},
        {"N", WB_OV_N}}, V[] = {
        {"CHORD", V_CHORD}, {"BASS", V_BASS}, {"LEAD", V_LEAD}, {"PAD", V_PAD}, {"KICK", V_KICK}, {"SNARE", V_SNARE},
        {"HAT", V_HAT}, {"OHAT", V_OHAT}, {"SHAKER", V_SHAKER}, {"RIM", V_RIM}, {"RIDE", V_RIDE}}, SRC[] = {
        {"NONE", WB_SRC_NONE}, {"PT", WB_SRC_PT}, {"OPT", WB_SRC_OPT}, {"SAMPLE", WB_SRC_SAMPLE},
        {"BUILTIN", WB_SRC_BUILTIN}}, KIT[] = {
        {"PRESET", WB_KIT_PRESET}, {"PT808", WB_KIT_PT808}, {"PTCR78", WB_KIT_PTCR78}, {"OPTLOFI", WB_KIT_OPTLOFI},
        {"GOGODZE", WB_KIT_GOGODZE}, {"RUSTY", WB_KIT_RUSTY}, {"BRUSHES", WB_KIT_BRUSHES},
        {"PRESETS", WB_KIT_PRESETS}, {"LANES", WB_LANES}, {"VL", WB_SMP_VL}, {"RR", WB_SMP_RR}}, PT[] = {
        {"M808", M_808}, {"MCR78", M_CR78}, {"BD", V_BD}, {"SD", V_SD}, {"RS", V_RS}, {"CP", V_CP}, {"LT", V_LT},
        {"MT", V_MT}, {"HT", V_HT}, {"CH", V_CH}, {"OH", V_OH}, {"CY", V_CY}, {"CB", V_CB}, {"TB", V_TB},
        {"LC", V_LC}, {"MC", V_MC}, {"HC", V_HC}, {"MA", V_MA}, {"CL", V_CL}}, SMP[] = {
        {"KICK", WB_P_KICK}, {"SNARE", WB_P_SNARE}, {"RIM", WB_P_RIM}, {"HAT", WB_P_HAT}, {"OHAT", WB_P_OHAT},
        {"RIDE", WB_P_RIDE}};
    static const struct { const char *key; const kv_t *t; unsigned n; } T[] = {
        {"RF", RF, sizeof RF / sizeof RF[0]}, {"OV", OV, sizeof OV / sizeof OV[0]}, {"V", V, sizeof V / sizeof V[0]},
        {"SRC", SRC, sizeof SRC / sizeof SRC[0]}, {"DRUMS", KIT, sizeof KIT / sizeof KIT[0]},
        {"PT", PT, sizeof PT / sizeof PT[0]}, {"SMP", SMP, sizeof SMP / sizeof SMP[0]}};
    static char buf[4096];
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

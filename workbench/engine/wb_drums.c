/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The workbench's drum lanes (wb_drums.h). A slot per lane, and one for auditions: a hit starts the slot's sound on
   its lane's source (a new hit restarts it, as on the machines), at the source's calibrated level times the lane's.
   poketrack's machines run an engine per slot (its own output clip, so a lane's sound never depends on another's); a
   closed hat chokes the open one (4 ms), as both machines do. The built-in kit's lanes go to drums.c, unhooked for
   the call. */
#include <math.h>
#include <string.h>
#include "wb_drums.h"
#include "wb_opt.h"
#include "drums.h"
#include "library.h"
#include "poketrack/trommelsynthesizer.h"

#define EXPORT __attribute__((used, visibility("default")))
#define SR 44100.0f
#define NS (WB_LANES + 1)               /* the lanes' slots, then the audition's */
#define AUD WB_LANES
#define L_HAT (V_HAT - V_KICK)
#define L_OHAT (V_OHAT - V_KICK)
#define CHOKE 176                       /* 4 ms */
#define REL 0.996f                      /* drums_release: under 40 ms to -60 dB, as drums.c */
#define OFF 1e-3f
#define KICK_T60 0.3f                   /* the ducking envelope of a synth kick */

/* the sources' levels (linear): a preset's kick and snare (RMS over a song, before the drums' compressor) at the
   Brushes kit's, the middle of their range over six songs (unity: 808 +7.6..+10.6 dB, CR-78 +5.0..+6.7, LO-FI
   +14.7..+15.5; smoke.mjs checks two within 2 dB); the built-in's is its own */
static const float PT_G[2] = {0.3508f, 0.5129f};    /* M_808 -9.1 dB, M_CR78 -5.8 dB */
static const float OPT_G[WB_OPT_N] = {0.1758f};     /* LO-FI -15.1 dB */

typedef struct {
    wb_src_t src;
    float g;                            /* level */
    float rel;                          /* release: 1, or REL while drums_release's fade runs */
    float fade;                         /* the release's level so far */
    int choke;                          /* samples left of a choke (0: none) */
    int zeros;                          /* PT: the run of zero samples (an engine's voice has ended) */
    uint8_t on;
} slot_t;

static Engine *pt[NS];
static slot_t sl[NS];
static wb_drums_t cfg;
static int cfg_any;
static float kenv, kdec;
static float t[256], tr[256];
/* meters (tests): each lane's and the whole output's sum of squares and sample count, a hash of each lane's samples */
static double m_sum[WB_LANES + 1];
static uint32_t m_n[WB_LANES + 1], m_hash[WB_LANES];

static void h_on(const song_ev_t *e);
static void h_render(float *x, uint32_t n);
static int h_active(void);
static float h_kick_env(void);
static void h_release(void);
static const drums_hook_t HOOK = {h_on, h_render, h_active, h_kick_env, h_release};

/* the built-in drums (drums.c), unhooked for the call */
#define BUILTIN(call)          \
    do {                       \
        drums_hook(NULL);      \
        call;                  \
        drums_hook(&HOOK);     \
    } while (0)

static int pt_machine(int a) { return a == M_808 || a == M_CR78; }

int wb_drums_src_ok(int kind, int a, int b)
{
    switch (kind) {
    case WB_SRC_NONE: return 1;
    case WB_SRC_PT: return pt_machine(a) && b >= 0 && b < V_COUNT && eng_machine_has(a, b);
    case WB_SRC_OPT: return wb_opt_has(a, b);
    case WB_SRC_BUILTIN: return a == KIT_BRUSHES && b >= 0 && b <= V_RIM - V_KICK;
    default: return 0;                  /* (WB_SRC_SAMPLE: Task 9) */
    }
}

int wb_drums_any(const wb_drums_t *d)
{
    int i;
    for (i = 0; d && i < WB_LANES; i++)
        if (d->lane[i].kind != WB_SRC_NONE)
            return 1;
    return 0;
}

int wb_drums_preset(wb_drums_t *d, int id)
{
    /* kick, snare, closed hat, open hat, shaker, rim, ride (the closed hat) */
    static const uint8_t PT_V[WB_LANES] = {V_BD, V_SD, V_CH, V_OH, V_MA, V_RS, V_CH};
    static const uint8_t OPT_L[WB_LANES] = {0, 1, 3, 4, 9, 11, 3};
    int i;
    if (id < 0 || id >= WB_KIT_PRESETS)
        return 0;
    memset(d, 0, sizeof *d);
    for (i = 0; i < WB_LANES; i++) {
        wb_src_t *s = &d->lane[i];
        if (id == WB_KIT_OPTLOFI) {
            s->kind = WB_SRC_OPT;
            s->a = WB_OPT_LOFI;
            s->b = OPT_L[i];
        } else {
            s->kind = WB_SRC_PT;
            s->a = id == WB_KIT_PT808 ? M_808 : M_CR78;
            s->b = PT_V[i];
        }
    }
    d->preset = (int8_t)id;
    return 1;
}

static float src_gain(const wb_src_t *s)
{
    float g = s->kind == WB_SRC_PT ? PT_G[s->a == M_CR78] : s->kind == WB_SRC_OPT ? OPT_G[s->a] : 1.0f;
    return g * powf(10.0f, s->gain_db * 0.05f);
}

static void hook_on(void) { drums_hook(&HOOK); }

/* src's sound in slot i at vel; lane: its lane (the choke, the ducking envelope) */
static void start(int i, const wb_src_t *src, int lane, int vel)
{
    slot_t *s = &sl[i];
    if (lane == L_HAT && i != AUD && sl[L_OHAT].on && !sl[L_OHAT].choke)
        sl[L_OHAT].choke = CHOKE;
    if (src->kind == WB_SRC_BUILTIN) {
        song_ev_t e = {0};
        float v = (float)vel * src_gain(src);
        e.voice = (uint8_t)(V_KICK + src->b);
        e.vel = (uint8_t)(v < 1.0f ? 1.0f : v > 127.0f ? 127.0f : v + 0.5f);
        BUILTIN(drums_set_kit(src->a); drums_on(&e));
        return;
    }
    if (src->kind != WB_SRC_PT && src->kind != WB_SRC_OPT)
        return;
    eng_reset(pt[i]);
    wb_opt_off(i);
    if (src->kind == WB_SRC_PT)
        eng_trigger(pt[i], src->a, src->b, 0, (float)vel * (1.0f / 127.0f));
    else
        wb_opt_on(i, src->a, src->b, vel);
    s->src = *src;
    s->g = src_gain(src);
    s->rel = s->fade = 1.0f;
    s->choke = s->zeros = 0;
    s->on = 1;
    if (lane == 0 && i != AUD)
        kenv = (float)vel * (1.0f / 127.0f);
    hook_on();
}

static void h_on(const song_ev_t *e)
{
    int lane = (int)e->voice - V_KICK;
    if (lane < 0 || lane >= WB_LANES)
        return;
    if (!cfg_any)                       /* (installed for an audition only) the song's own kit */
        BUILTIN(drums_on(e));
    else
        start(lane, &cfg.lane[lane], lane, e->vel);
}

static uint32_t fnv(uint32_t h, float x)
{
    uint32_t u;
    memcpy(&u, &x, 4);
    return (h ^ u) * 16777619u;
}

/* slot i's next n samples, added into x */
static void slot_render(int i, float *x, uint32_t n)
{
    slot_t *s = &sl[i];
    uint32_t k;
    if (s->src.kind == WB_SRC_PT) {
        eng_render(pt[i], t, tr, n);
        for (k = 0; k < n; k++)
            s->zeros = t[k] == 0.0f ? s->zeros + 1 : 0;
        if (s->zeros >= 64)
            s->on = 0;
    } else if (!wb_opt_render(i, t, n)) {
        s->on = 0;
    }
    for (k = 0; k < n; k++) {
        float y = t[k] * s->g * s->fade;
        s->fade *= s->rel;
        if (s->choke) {
            y *= (float)s->choke * (1.0f / CHOKE);
            if (!--s->choke)
                s->on = 0;
        }
        x[k] += t[k] = y;
    }
    if (s->fade < OFF)
        s->on = 0;
    if (i < WB_LANES) {
        for (k = 0; k < n; k++) {
            m_sum[i] += (double)t[k] * t[k];
            m_hash[i] = fnv(m_hash[i], t[k]);
        }
        m_n[i] += n;
    }
}

static void h_render(float *x, uint32_t n)
{
    int i, any = 0;
    uint32_t k;
    if (n > 256)                        /* (drums_render's limit) */
        n = 256;
    memset(x, 0, n * sizeof *x);
    for (i = 0; i < NS; i++)
        if (sl[i].on)
            slot_render(i, x, n);
    drums_hook(NULL);
    if (drums_active()) {
        drums_render(t, n);
        for (k = 0; k < n; k++)
            x[k] += t[k];
    }
    for (i = 0; i < NS; i++)
        any |= sl[i].on;
    kenv = sl[0].on ? kenv * powf(kdec, (float)n) : 0.0f;
    for (k = 0; k < n; k++)
        m_sum[WB_LANES] += (double)x[k] * x[k];
    m_n[WB_LANES] += n;
    if (cfg_any || any || drums_active())
        hook_on();                      /* (else off: the song's own kit, as with no lanes) */
}

static int h_active(void)
{
    int i, c = 0;
    for (i = 0; i < NS; i++)
        c += sl[i].on;
    drums_hook(NULL);
    c += drums_active();
    hook_on();
    return c;
}

static float h_kick_env(void)
{
    float b;
    BUILTIN(b = drums_kick_env());
    return kenv > b ? kenv : b;
}

static void h_release(void)
{
    int i;
    for (i = 0; i < NS; i++)
        sl[i].rel = REL;
    BUILTIN(drums_release());
}

void wb_drums_init(void)
{
    int i;
    for (i = 0; i < NS; i++) {
        if (!pt[i])
            pt[i] = eng_create(SR);
        eng_reset(pt[i]);
        wb_opt_off(i);
    }
    memset(sl, 0, sizeof sl);
    memset(&cfg, 0, sizeof cfg);
    cfg.preset = -1;
    cfg_any = 0;
    kenv = 0.0f;
    kdec = expf(-6.9078f / (KICK_T60 * SR));
    drums_hook(NULL);
}

void wb_drums_set(const wb_drums_t *d)
{
    int i;
    cfg_any = wb_drums_any(d);
    memset(&cfg, 0, sizeof cfg);
    if (cfg_any)
        cfg = *d;
    if (cfg_any)
        hook_on();
    else {                              /* off once nothing of ours sounds (h_render turns it off then) */
        int on = 0;
        for (i = 0; i < NS; i++)
            on |= sl[i].on;
        if (!on)
            drums_hook(NULL);
    }
}

void wb_drums_song(uint32_t seed) { wb_opt_seed(seed); }

void wb_drums_audition(const wb_src_t *src, int lane, int vel)
{
    if (lane < 0 || lane >= WB_LANES)
        return;
    if (src->kind == WB_SRC_NONE) {     /* the song's own kit's lane */
        song_ev_t e = {0};
        e.voice = (uint8_t)(V_KICK + lane);
        e.vel = (uint8_t)vel;
        BUILTIN(drums_on(&e));          /* (h_render turns the hook off again once it has ended) */
        return;
    }
    start(AUD, src, lane, vel);
}

/* tests: a lane's (0..6) or the whole hooked output's (WB_LANES) mean square since the reset, a lane's samples' hash */
EXPORT float wb_drum_meter(int lane) { return lane >= 0 && lane <= WB_LANES && m_n[lane] ? (float)(m_sum[lane] / m_n[lane]) : 0.0f; }
EXPORT int wb_drum_hash(int lane) { return lane >= 0 && lane < WB_LANES ? (int)m_hash[lane] : 0; }
EXPORT void wb_drum_meter_reset(void)
{
    memset(m_sum, 0, sizeof m_sum);
    memset(m_n, 0, sizeof m_n);
    memset(m_hash, 0, sizeof m_hash);
}

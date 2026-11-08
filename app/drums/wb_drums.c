/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The drum lanes (wb_drums.h), the FM-1's and the workbench's. A slot per lane, and one for auditions: a hit starts
   the slot's sound on its lane's source (a new hit restarts it, as on the machines), at the source's calibrated level
   times the lane's. FOMAJAM's modeled machines' voices play back from RAM, rendered once (fomajam/fj_drums.h:
   integers, so the FM-1 and the workbench have the same samples; below); a closed hat chokes the open one (4 ms), as
   both machines do. A sampled piece plays its layer from the sample bank (samples.h smp_piece: ADPCM, read at 44.1 kHz
   with a linear step). The built-in kits' lanes go to drums.c, unhooked for the call: a hit on another kit than the
   song's leaves drums.c on the song's (at once for its routing; its synth kit once nothing sounds, as a hit sounding
   reads its kit's table). */
#include <string.h>
#include "fastmath.h"
#include "wb_drums.h"
#include "wb_opt.h"
#include "drums.h"
#include "library.h"
#include "sampler.h"
#include "fomajam/fj_drums.h"
#include "cpuguard.h"

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
   Brushes kit's, the middle of their range over six songs (unity: 808 +11.3..+14.3 dB, CR-78 +8.8..+10.5, LO-FI
   +14.7..+15.5; smoke.mjs checks two within 2 dB); the built-in's is its own */
static const float PT_G[2] = {0.2281f, 0.3308f};    /* M_808 -12.8 dB, M_CR78 -9.6 dB (FOMAJAM's voices) */
static const float OPT_G[WB_OPT_N] = {0.1758f};     /* LO-FI -15.1 dB */
/* a sampled kit's level in dB (its snare at the Brushes kit's snare, by the same measure), and its kick's against
   that (the kick at Brushes' kick): as shipped (the SFZ programs' default mic mixes, the files as prep_kits.py makes
   them) the Gogodze kick sits 6.3 dB further over its snare than Brushes' does (its room mics), Big Rusty's sticks'
   4.2 dB further under, the brush snare 6.3 dB under its kick; every other piece moves with the kit */
static const float SMP_DB[WB_SMP_N] = {-3.8f, -4.4f, 6.1f};
static const float SMP_KICK_DB[WB_SMP_N] = {-6.3f, 4.2f, -6.3f};

typedef struct {
    wb_src_t src;
    float g;                            /* level */
    float rel;                          /* release: 1, or REL while drums_release's fade runs */
    float fade;                         /* the release's level so far */
    int choke;                          /* samples left of a choke (0: none) */
    smp_zone_t z;                       /* PT: the voice's hit (hit_buf), as a zone */
    smp_dec_t d;                        /* SAMPLE, PT: the decoder, the two samples the read is between, the read's */
    float h0, h1;                       /* place past h0 and its step (16.16: the zone's rate over 44.1 kHz) */
    uint32_t ph, step;
    uint32_t age;                       /* samples since its hit (the guard's tails) */
    uint8_t on;
} slot_t;

static slot_t sl[NS];
static wb_drums_t cfg;
static int cfg_any;
static float kenv, kdec;
static uint8_t g_tails;                 /* wb_drums_guard's */
static int song_kit, stale;             /* the song's kit (vx_set_kit's); drums.c's synth kit isn't it (a BUILTIN hit) */
static float t[256];
/* meters (the workbench's tests: wasm only; the firmware has no double): each lane's and the whole output's sum
   of squares and sample count, a hash of each lane's samples */
#ifdef __EMSCRIPTEN__
#define METER 1
static double m_sum[WB_LANES + 1];
static uint32_t m_n[WB_LANES + 1], m_hash[WB_LANES];
#endif

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

static int piece_ok(int set, int p) { return smp_piece(set, p) != 0; }

/* ---- the modeled machines' voices (fomajam/fj_drums.h), rendered once into RAM as the lanes need them, then played
   back as samples, at the hit's velocity (only its level moves with velocity). 4-bit IMA ADPCM at 44.1 kHz (samples.h's
   format), the decoder's start index the one of HIT_IDX0 that encodes the first 256 samples best. A voice is cut at
   HIT_CAP (faded over its last quarter second; only the CR-78 cymbal rings that long), and the set at HIT_BYTES (a
   voice that would pass it shorter, faded: only a workbench lane mix of the two machines' longest voices comes near).
   The render (wb_drums_work) runs in the main loop on the FM-1, a slice at a time; it starts a new set only once no
   slot plays from the old one, and a hit plays a voice only once the whole set is there (hit_done). */
#define HIT_MAX (WB_LANES + 1)          /* a voice a lane, and the audition's */
#define HIT_CAP (44100u * 5u / 2u)      /* 2.5 s */
#define HIT_FADE 11025u                 /* 0.25 s */
#define HIT_END 316                     /* a voice ends where 256 samples all stay 50 dB under its peak (FOMAJAM's
                                           own end, about 66 dB under, rings on unheard under the next hit) */
#define HIT_BYTES 136000u
#ifdef FELUCCA_ID
#define HIT_POOL __attribute__((section(".pool")))
#else
#define HIT_POOL
#endif
static uint8_t hit_buf[HIT_BYTES] HIT_POOL;
typedef struct { uint16_t key; uint32_t off, len; uint8_t idx0; } hit_t;   /* key: machine * V_COUNT + voice + 1 */
static hit_t hit[HIT_MAX];
static uint16_t want_k[HIT_MAX];
static int hit_n, want_n;
static volatile uint8_t hit_done;
static struct {                         /* the render under way: its voice, frames so far, cap and fade's start, the
                                           encoder (prediction, index, a low nibble waiting), the next byte */
    int i;
    uint32_t f, cap, fade, pos;
    int32_t pred, idx, nib, peak;
} job = {-1, 0, 0, 0, 0, 0, 0, -1, 0};
static int16_t pcm[256];

static uint16_t hit_key(const wb_src_t *s)
{
    return s->kind == WB_SRC_PT && pt_machine(s->a) && fj_has(s->a, s->b) ? (uint16_t)(s->a * V_COUNT + s->b + 1) : 0;
}

/* the voices d's lanes need (and extra's), each once, in lane order: -> how many */
static int hit_need(const wb_drums_t *d, const wb_src_t *extra, uint16_t *k)
{
    int n = 0, i, j;
    for (i = 0; i <= WB_LANES; i++) {
        uint16_t x = i < WB_LANES ? (d ? hit_key(&d->lane[i]) : 0) : extra ? hit_key(extra) : 0;
        for (j = 0; j < n && k[j] != x; j++) {}
        if (x && j == n)
            k[n++] = x;
    }
    return n;
}

static const hit_t *hit_of(const wb_src_t *s)
{
    uint16_t x = hit_key(s);
    int i;
    for (i = 0; hit_done && x && i < hit_n; i++)
        if (hit[i].key == x)
            return &hit[i];
    return 0;
}

static int hit_has(const uint16_t *k, int n)
{
    int i, j;
    for (i = 0; i < n; i++) {
        for (j = 0; j < hit_n && hit[j].key != k[i]; j++) {}
        if (j == hit_n)
            return 0;
    }
    return 1;
}

/* IMA ADPCM's code for x from (pred, idx), the state moved on (pack.py's adpcm_encode) */
static int ima_code(int32_t x, int32_t *pred, int32_t *idx)
{
    int32_t step = SMP_STEP[*idx], d = x - *pred, diff = step >> 3;
    int code = 0;
    if (d < 0) {
        code = 8;
        d = -d;
    }
    if (d >= step) {
        code |= 4;
        d -= step;
        diff += step;
    }
    if (d >= step >> 1) {
        code |= 2;
        d -= step >> 1;
        diff += step >> 1;
    }
    if (d >= step >> 2) {
        code |= 1;
        diff += step >> 2;
    }
    *pred += code & 8 ? -diff : diff;
    *pred = *pred < -32768 ? -32768 : *pred > 32767 ? 32767 : *pred;
    *idx += SMP_IDXT[code & 7];
    *idx = *idx < 0 ? 0 : *idx > 88 ? 88 : *idx;
    return code;
}

/* the voice's next n samples (pcm) encoded, faded and cut as its cap says */
static void hit_encode(uint32_t n)
{
    uint32_t k;
    for (k = 0; k < n; k++, job.f++) {
        int32_t x = pcm[k];
        int c;
        if (job.f >= job.fade)
            x = x * (int32_t)(job.cap - job.f) / (int32_t)(job.cap - job.fade);   /* (32767 x 11025 at most) */
        c = ima_code(x, &job.pred, &job.idx);
        if (job.nib < 0) {
            job.nib = c;
        } else {
            hit_buf[job.pos++] = (uint8_t)(job.nib | c << 4);
            job.nib = -1;
        }
    }
}

void wb_drums_want(const wb_drums_t *d) { want_n = hit_need(d, 0, want_k); }

int wb_drums_ready(const wb_drums_t *d)
{
    uint16_t k[HIT_MAX];
    int n = hit_need(d, 0, k);
    return !n || (hit_done && hit_has(k, n));
}

int wb_drums_work(uint32_t frames, int now)
{
    int i;
    if (!want_n || (job.i < 0 && hit_done && hit_has(want_k, want_n)))
        return 0;
    if (job.i < 0 || !hit_has(want_k, want_n)) {   /* a new set: once no slot plays the old one */
        hit_done = 0;
        for (i = 0; i < NS; i++)
            if (sl[i].on && sl[i].src.kind == WB_SRC_PT) {
                if (!now)
                    return 1;
                sl[i].on = 0;
            }
        for (hit_n = 0; hit_n < want_n; hit_n++)
            hit[hit_n] = (hit_t){want_k[hit_n], 0, 0, 0};
        job.i = 0;
        job.pos = 0;
        job.f = 0;
    }
    while (job.i < hit_n && (now || frames)) {
        hit_t *h = &hit[job.i];
        uint32_t want = 256u, got;
        if (job.f == 0) {               /* a voice begins: its cap, its first samples, the index they start best at */
            static const uint8_t HIT_IDX0[8] = {0, 12, 24, 36, 48, 60, 72, 84};
            uint32_t best = 0xFFFFFFFFu;
            uint32_t room = (HIT_BYTES - job.pos) * 2u;
            fj_voice_begin((h->key - 1) / V_COUNT, (h->key - 1) % V_COUNT);
            job.cap = room < HIT_CAP ? room : HIT_CAP;
            job.fade = job.cap - (job.cap / 4u < HIT_FADE ? job.cap / 4u : HIT_FADE);
            got = fj_voice_next(pcm, want < job.cap ? want : job.cap);
            for (i = 0; i < 8; i++) {
                int32_t p = 0, x = HIT_IDX0[i];
                uint32_t k, e = 0;
                for (k = 0; k < got; k++) {
                    int32_t q = pcm[k] - (ima_code(pcm[k], &p, &x), p);
                    e += (uint32_t)(q < 0 ? -q : q);   /* (the error's sum: 256 at most, no overflow) */
                }
                if (e < best) {
                    best = e;
                    h->idx0 = HIT_IDX0[i];
                }
            }
            h->off = job.pos;
            job.peak = 0;
            job.pred = 0;
            job.idx = h->idx0;
            job.nib = -1;
        } else {
            uint32_t left = job.cap - job.f;
            want = left < want ? left : want;
            if (!now && frames < want)
                want = frames;
            got = fj_voice_next(pcm, want);
        }
        {                               /* its peak so far; a piece all under HIT_END of it: the voice ends there */
            uint32_t k;
            int32_t m = 0;
            for (k = 0; k < got; k++)
                m = pcm[k] > m ? pcm[k] : -pcm[k] > m ? -pcm[k] : m;
            job.peak = m > job.peak ? m : job.peak;
            if (job.f >= 2048u && m * HIT_END < job.peak)
                got = 0;
        }
        hit_encode(got);
        frames = frames > got ? frames - got : 0;
        if (got < want || job.f >= job.cap) {   /* the voice ended, or its cap */
            if (job.nib >= 0)
                hit_buf[job.pos++] = (uint8_t)job.nib;
            h->len = job.f;
            job.i++;
            job.f = 0;
        }
    }
    if (job.i < hit_n)
        return 1;
    job.i = -1;
    hit_done = 1;
    return 0;
}

/* a hit on an engine kit's lane through drums.c (unhooked), then drums.c back on the song's kit: its routing (a
   sampled kit's voices keep their kit) at once; its synth kit when nothing sounds or before the song's own next hit */
static void kit_on(int kit, const song_ev_t *e)
{
    drums_hook(NULL);
    if (stale && kit == song_kit) {
        drums_set_kit(song_kit);
        stale = 0;
    } else if (kit != song_kit) {
        drums_set_kit(kit);
    }
    drums_on(e);
    if (kit != song_kit) {
        if (lib_kit_sampled(kit) || lib_kit_sampled(song_kit))
            drums_set_kit(song_kit);    /* (the synth lanes' table stays: drums_set_kit leaves it for a sampled kit) */
        stale |= !lib_kit_sampled(kit);
    }
    drums_hook(&HOOK);
}

int wb_drums_src_ok(int kind, int a, int b)
{
    switch (kind) {
    case WB_SRC_NONE: return 1;
    case WB_SRC_PT: return pt_machine(a) && fj_has(a, b);
    case WB_SRC_OPT: return wb_opt_has(a, b);
    case WB_SRC_SAMPLE: return piece_ok(a, b);
    case WB_SRC_BUILTIN: return a >= 0 && a <= KIT_BRUSHES && b >= 0 && b <= V_RIM - V_KICK;
    default: return 0;
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
    /* the sampled kits' (kits.json's lanes): no shaker; Gogodze has no ride (its closed hat) */
    static const int8_t SMP_P[WB_SMP_N][WB_LANES] = {
        {WB_P_KICK, WB_P_SNARE, WB_P_HAT, WB_P_OHAT, -1, WB_P_RIM, WB_P_HAT},
        {WB_P_KICK, WB_P_SNARE, WB_P_HAT, WB_P_OHAT, -1, WB_P_RIM, WB_P_RIDE},
        {WB_P_KICK, WB_P_SNARE, WB_P_HAT, WB_P_OHAT, -1, WB_P_RIM, WB_P_RIDE}};
    int i;
    if (id < 0 || id >= WB_KIT_PRESETS)
        return 0;
    for (i = 0; WB_KIT_SAMPLED(id) && i < WB_LANES; i++)
        if (SMP_P[id - WB_KIT_GOGODZE][i] >= 0 && !piece_ok(id - WB_KIT_GOGODZE, SMP_P[id - WB_KIT_GOGODZE][i]))
            return 0;
    memset(d, 0, sizeof *d);
    for (i = 0; i < WB_LANES; i++) {
        wb_src_t *s = &d->lane[i];
        if (WB_KIT_SAMPLED(id)) {
            int p = SMP_P[id - WB_KIT_GOGODZE][i];
            if (p >= 0)
                *s = (wb_src_t){WB_SRC_SAMPLE, (uint8_t)(id - WB_KIT_GOGODZE), (uint8_t)p, 0.0f};
        } else if (id == WB_KIT_OPTLOFI) {
            s->kind = WB_SRC_OPT;
            s->a = WB_OPT_LOFI;
            s->b = OPT_L[i];
        } else {
            s->kind = WB_SRC_PT;
            s->a = id == WB_KIT_PT808 ? M_808 : M_CR78;
            s->b = PT_V[i];
            if (id == WB_KIT_PTCR78 && PT_V[i] == V_RS)  /* the CR-78's rim lane: its cymbal (Sam, 2026-11-04: not its
                                                            rimshot) */
                s->b = V_CY;
        }
    }
    d->preset = (int8_t)id;
    return 1;
}

static float src_gain(const wb_src_t *s)
{
    float g = s->kind == WB_SRC_PT ? PT_G[s->a == M_CR78] : s->kind == WB_SRC_OPT ? OPT_G[s->a] : 1.0f;
    if (s->kind == WB_SRC_SAMPLE)
        g = fm_powf(10.0f, (SMP_DB[s->a] + (s->b == WB_P_KICK ? SMP_KICK_DB[s->a] : 0.0f)) * 0.05f);
    return g * fm_powf(10.0f, s->gain_db * 0.05f);
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
        kit_on(src->a, &e);
        return;
    }
    if (src->kind == WB_SRC_SAMPLE ? !piece_ok(src->a, src->b) : src->kind == WB_SRC_PT ? !hit_of(src)
                                                                : src->kind != WB_SRC_OPT)
        return;                          /* (a voice not rendered yet: silent) */
    wb_opt_off(i);
    s->g = src_gain(src);
    if (src->kind == WB_SRC_SAMPLE) {   /* the range up to whose top the velocity falls: its kept layer, its level */
        const smp_piece_t *p = smp_piece(src->a, src->b);
        const smp_zone_t *z;
        int v = vel * p->vel / 100, l = 0;
        v = v < 1 ? 1 : v;
        while (l < p->n - 1 && v > p->hi[l])
            l++;
        z = p->z[p->use[l]];
        smp_dec_init(&s->d, z);
        s->h0 = (float)smp_dec_fast(&s->d);
        s->h1 = (float)smp_dec_fast(&s->d);
        s->ph = 0;
        s->step = (uint32_t)z->rate * 65536u / 44100u;   /* (a rate under 65536 Hz) */
        s->g *= z->gain * (1.0f / 32768.0f) * p->lvl[l] * fm_powf((float)v / (float)p->hi[l], (float)p->curve);
    } else if (src->kind == WB_SRC_PT) {  /* the voice at full velocity, scaled to this one */
        const hit_t *h = hit_of(src);
        s->z = (smp_zone_t){0, h->len, 0, 0, 44100u, 60, h->idx0, 1.0f};
        memset(&s->d, 0, sizeof s->d);
        s->d.z = &s->z;
        s->d.p = hit_buf + h->off;
        s->d.idx = s->d.lidx = h->idx0;
        s->h0 = (float)smp_dec_fast(&s->d);
        s->h1 = (float)smp_dec_fast(&s->d);
        s->ph = 0;
        s->step = 65536u;
        s->g *= (float)vel * (1.0f / (127.0f * 32768.0f));
    } else {
        wb_opt_on(i, src->a, src->b, vel);
    }
    s->src = *src;
    s->rel = s->fade = 1.0f;
    s->choke = 0;
    s->age = 0;
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
        kit_on(song_kit, e);
    else
        start(lane, &cfg.lane[lane], lane, e->vel);
}

#ifdef METER
static uint32_t fnv(uint32_t h, float x)
{
    uint32_t u;
    memcpy(&u, &x, 4);
    return (h ^ u) * 16777619u;
}
#endif

void wb_drums_guard(int tails) { g_tails = (uint8_t)(tails != 0); }

/* slot i's next n samples, added into x */
static void slot_render(int i, float *x, uint32_t n)
{
    slot_t *s = &sl[i];
    uint32_t k;
    s->age += n;
    if (g_tails && s->age > (uint32_t)(CG_TAIL_S * SR) && s->rel == 1.0f)
        s->rel = REL;                   /* (the guard's: the ring goes, as drums_release fades it) */
#ifndef METER
    if (s->src.kind == WB_SRC_PT && !s->choke && s->rel == 1.0f) {   /* the common case in one pass (fade stays 1:
                                                                        the same samples as below) */
        float g = s->g;
        x[0] += s->h0 * g;
        if (n > 1)
            x[1] += s->h1 * g;
        for (k = 2; k < n; k++)
            x[k] += (float)smp_dec_fast(&s->d) * g;
        s->h0 = (float)smp_dec_fast(&s->d);
        s->h1 = (float)smp_dec_fast(&s->d);
        if (s->d.ni >= s->d.z->len)   /* (the decoder stops at len: its last two samples, 50 dB down, go unplayed) */
            s->on = 0;
        return;
    }
#endif
    if (s->src.kind == WB_SRC_PT) {      /* (at the zone's own rate: no read between samples) */
        t[0] = s->h0;
        if (n > 1)
            t[1] = s->h1;
        for (k = 2; k < n; k++)
            t[k] = (float)smp_dec_fast(&s->d);
        s->h0 = (float)smp_dec_fast(&s->d);
        s->h1 = (float)smp_dec_fast(&s->d);
        if (s->d.ni >= s->d.z->len)   /* (the decoder stops at len: its last two samples, 50 dB down, go unplayed) */
            s->on = 0;
    } else if (s->src.kind == WB_SRC_SAMPLE) {
        for (k = 0; k < n; k++) {
            t[k] = s->h0 + (s->h1 - s->h0) * (float)(s->ph & 0xFFFFu) * (1.0f / 65536.0f);
            for (s->ph += s->step; s->ph >> 16; s->ph -= 1u << 16) {
                s->h0 = s->h1;
                s->h1 = (float)smp_dec_fast(&s->d);
            }
        }
        if (s->d.ni >= s->d.z->len)     /* (played out: the zone's tail faded to nothing) */
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
#ifdef METER
    if (i < WB_LANES) {
        for (k = 0; k < n; k++) {
            m_sum[i] += (double)t[k] * t[k];
            m_hash[i] = fnv(m_hash[i], t[k]);
        }
        m_n[i] += n;
    }
#endif
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
    } else if (stale) {
        drums_set_kit(song_kit);
        stale = 0;
    }
    for (i = 0; i < NS; i++)
        any |= sl[i].on;
    kenv = sl[0].on ? kenv * fm_powf(kdec, (float)n) : 0.0f;
#ifdef METER
    for (k = 0; k < n; k++)
        m_sum[WB_LANES] += (double)x[k] * x[k];
    m_n[WB_LANES] += n;
#endif
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
    hit_n = want_n = 0;
    hit_done = 0;
    job.i = -1;
    for (i = 0; i < NS; i++) {
        wb_opt_off(i);
    }
    memset(sl, 0, sizeof sl);
    memset(&cfg, 0, sizeof cfg);
    song_kit = KIT_DUSTY;               /* (vx_init's) */
    stale = 0;
    cfg.preset = -1;
    cfg_any = 0;
    kenv = 0.0f;
    kdec = fm_expf(-6.9078f / (KICK_T60 * SR));
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

void wb_drums_song(uint32_t seed)
{
    wb_opt_seed(seed);
}

void wb_drums_kit(int kit)
{
    song_kit = kit;
    stale = 0;
}

void wb_drums_audition(const wb_src_t *src, int lane, int vel)
{
    if (src->kind == WB_SRC_PT && !hit_of(src)) {   /* (the workbench's: a voice the lanes don't play, rendered now) */
        want_n = hit_need(cfg_any ? &cfg : 0, src, want_k);
        wb_drums_work(0, 1);
    }
    if (lane >= 0 && lane < WB_LANES && src->kind != WB_SRC_NONE)
        start(AUD, src, lane, vel);
}

void wb_drums_audition_kit(int kit, int lane, int vel)
{
    song_ev_t e = {0};
    if (lane < 0 || lane > (lib_kit_sampled(kit) ? V_RIDE - V_KICK : V_RIM - V_KICK))
        return;
    e.voice = (uint8_t)(V_KICK + lane);
    e.vel = (uint8_t)vel;
    kit_on(kit, &e);                    /* (h_render turns the hook off again once it has ended) */
}

#ifdef METER
/* tests: a lane's (0..6) or the whole hooked output's (WB_LANES) mean square since the reset, a lane's samples' hash */
EXPORT float wb_drum_meter(int lane) { return lane >= 0 && lane <= WB_LANES && m_n[lane] ? (float)(m_sum[lane] / m_n[lane]) : 0.0f; }
EXPORT uint32_t wb_drum_frames(void) { return m_n[WB_LANES]; }   /* the frames the meters ran (a drum sounding) */
EXPORT int wb_drum_hash(int lane) { return lane >= 0 && lane < WB_LANES ? (int)m_hash[lane] : 0; }
EXPORT void wb_drum_meter_reset(void)
{
    memset(m_sum, 0, sizeof m_sum);
    memset(m_n, 0, sizeof m_n);
    memset(m_hash, 0, sizeof m_hash);
}
#endif

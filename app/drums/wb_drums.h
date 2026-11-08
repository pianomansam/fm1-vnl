/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The workbench's drums: seven lanes (kick, snare, closed hat, open hat, shaker, rim, ride: V_KICK..V_RIDE's order),
   each playing a sound from a source: FOMAJAM's modeled 808 and CR-78 (fomajam/, GPL-3.0-only), Optimist's synth kits
   (optimist/, GPL-3.0-only: LO-FI), the engine's built-in synth kits (Brushes; Dusty, Soft and our 808 when a song
   has one), or the shipped sampled kits (Karoryfer's Gogodze and Big Rusty, CC0: reduced into the sample bank,
   samples.h smp_piece). Installed with
   drums_hook while any lane has a source, so the lanes go through the song's own drum bus, compressor, filters and
   ducking; with none, the song's own kit plays as before. Float out, no allocation after wb_drums_init. */
#ifndef WB_DRUMS_H
#define WB_DRUMS_H
#include <stdint.h>

#define WB_LANES 7
/* a lane's source: what a and b name */
enum {
    WB_SRC_NONE,    /* silent (every lane NONE: the song's own kit) */
    WB_SRC_PT,      /* a: the modeled machine (M_808, M_CR78), b: its voice (V_BD, V_SD, ...: fomajam/fj_drums.h) */
    WB_SRC_OPT,     /* a: the Optimist kit (WB_OPT_*), b: its lane (0 kick, 1 snare, 3 hat, 4 open hat, 9 shaker, 11 rim:
                       drum_synth.c DS_LANES) */
    WB_SRC_SAMPLE,  /* a: the sample set (WB_SMP_*), b: its piece (WB_P_*); only with a sample bank */
    WB_SRC_BUILTIN, /* a: a synth kit (KIT_DUSTY..KIT_BRUSHES), b: its lane (0 kick .. 5 rim) */
    WB_SRC_N
};
enum { WB_OPT_LOFI, WB_OPT_N };                 /* optimist/felucca_drumkits.h's kits */
/* the sample sets (samples/kits.json's ids; samples.h SK_*) and their pieces (SK_KICK..) */
enum { WB_SMP_GOGODZE, WB_SMP_RUSTY, WB_SMP_BRUSHES, WB_SMP_N };
enum { WB_P_KICK, WB_P_SNARE, WB_P_RIM, WB_P_HAT, WB_P_OHAT, WB_P_RIDE, WB_P_N };
/* the shipped kits (wb_drums_preset); the kit row's value for one is WB_KIT_PRESET + its id, past the engine's KIT_*:
   the synth machines' kits, then the sampled kits (a sample set each, in WB_SMP_* order) */
enum { WB_KIT_PT808, WB_KIT_PTCR78, WB_KIT_OPTLOFI, WB_KIT_GOGODZE, WB_KIT_RUSTY, WB_KIT_BRUSHES, WB_KIT_PRESETS };
#define WB_KIT_PRESET 16
#define WB_KIT_SAMPLED(id) ((id) >= WB_KIT_GOGODZE && (id) < WB_KIT_PRESETS)

typedef struct { uint8_t kind; uint8_t a, b; float gain_db; } wb_src_t;
/* a song's drums (in the workbench's stage): the lanes, and the preset they came from (-1: none) */
typedef struct {
    wb_src_t lane[WB_LANES];
    int8_t preset;
} wb_drums_t;

void wb_drums_init(void);                       /* nothing sounding, no lanes, the hook off */
/* the lanes the song plays from now (NULL: none; the song's own kit). Hits already sounding finish on their source. */
void wb_drums_set(const wb_drums_t *d);
void wb_drums_song(uint32_t seed);              /* a song's start: the sources' noise reseeded from its seed */
int wb_drums_any(const wb_drums_t *d);          /* 1 if a lane has a source */
int wb_drums_src_ok(int kind, int a, int b);    /* 1 if the source exists (a sound the machine or kit has) */
/* d as preset id ships: every lane from its own machine or kit (a synth kit's ride on its closed hat; a sampled
   kit's as samples/kits.json says), levels 0 dB; 0 if id isn't one or a sampled kit's pieces aren't there (no
   bank) */
int wb_drums_preset(wb_drums_t *d, int id);
/* the modeled machines' voices (WB_SRC_PT), rendered once into RAM for the lanes to play back (wb_drums.c): the ones
   d's lanes need (wb_drums_want), rendered up to frames at a time (wb_drums_work: the FM-1's main loop; now: at once,
   cutting a slot that plays an old one: the workbench's), and whether all d needs is there (wb_drums_ready; a lane
   whose voice isn't is silent). wb_drums_work: 1 while work remains. */
void wb_drums_want(const wb_drums_t *d);
int wb_drums_work(uint32_t frames, int now);
int wb_drums_ready(const wb_drums_t *d);
void wb_drums_kit(int kit);                     /* the song's kit (KIT_*), as vx_set_kit has just set it */
/* The CPU guard's (cpuguard.h CG_TAILS): 1, a lane's sound past CG_TAIL_S fades out (drums_release's fade); 0, the
   default: as before, sample for sample */
void wb_drums_guard(int tails);
/* src's sound on lane now (vel 1-127), outside the song, through the same chain (NONE: nothing) */
void wb_drums_audition(const wb_src_t *src, int lane, int vel);
void wb_drums_audition_kit(int kit, int lane, int vel);    /* an engine kit's (KIT_*) lane, the same way */

#endif

/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The workbench's drums: seven lanes (kick, snare, closed hat, open hat, shaker, rim, ride: V_KICK..V_RIDE's order),
   each playing a sound from a source: poketrack's synthesized 808 and CR-78 (poketrack/, zlib), Optimist's synth kits
   (optimist/, GPL-3.0-only: LO-FI), the engine's built-in kit (Brushes), or (Task 9) samples. Installed with
   drums_hook while any lane has a source, so the lanes go through the song's own drum bus, compressor, filters and
   ducking; with none, the song's own kit plays as before. Float out, no allocation after wb_drums_init. */
#ifndef WB_DRUMS_H
#define WB_DRUMS_H
#include <stdint.h>

#define WB_LANES 7
/* a lane's source: what a and b name */
enum {
    WB_SRC_NONE,    /* silent (every lane NONE: the song's own kit) */
    WB_SRC_PT,      /* a: the poketrack machine (M_808, M_CR78), b: its voice (V_BD, V_SD, ...: trommelsynthesizer.h) */
    WB_SRC_OPT,     /* a: the Optimist kit (WB_OPT_*), b: its lane (0 kick, 1 snare, 3 hat, 4 open hat, 9 shaker, 11 rim:
                       drum_synth.c DS_LANES) */
    WB_SRC_SAMPLE,  /* a: the sample set, b: the piece (Task 9; silent until then) */
    WB_SRC_BUILTIN, /* a: KIT_BRUSHES, b: the built-in kit's lane (0 kick .. 5 rim) */
    WB_SRC_N
};
enum { WB_OPT_LOFI, WB_OPT_N };                 /* optimist/felucca_drumkits.h's kits */
/* the shipped kits (wb_drums_preset); the kit row's value for one is WB_KIT_PRESET + its id, past the engine's KIT_* */
enum { WB_KIT_PT808, WB_KIT_PTCR78, WB_KIT_OPTLOFI, WB_KIT_PRESETS };
#define WB_KIT_PRESET 16

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
/* d as preset id ships: every lane from its own machine or kit, the ride on its closed hat (none has a ride the
   song can use), levels 0 dB; 0 if id isn't one */
int wb_drums_preset(wb_drums_t *d, int id);
/* src's sound on lane now (vel 1-127), outside the song, through the same chain; NONE: the song's own kit's lane
   when no lane has a source */
void wb_drums_audition(const wb_src_t *src, int lane, int vel);

#endif

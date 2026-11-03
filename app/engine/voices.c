/* SPDX-License-Identifier: MPL-2.0 */
/* TPT SVF adapted from AMB-1 (fm1-amb), MPL-2.0. */
/* Voices and mix (see voices.h). The pitched roles are pools of voices: chords 4 (8 for a pad or a sampler), lead 2,
   candy 2, the bass, a mono pool of 1 whose level slews (AMP_K) and whose M_BASS retriggers are legato, and the pad
   layer 4. Each voice keeps the patch it was struck with, and voice_sample() dispatches on the patch's model:
   keys (M_EP), pads (M_PAD), winds (M_WIND), basses (M_BASS), a chip triangle (M_CHIP), the
   sampler (M_SAMP: sampler.c, its own envelope), the modeled vibraphone (M_MODAL: modal.h), the tonewheel organ
   (M_TW) and the pad layer's ensemble (M_ENS, at a quarter of the rate in a loop of its own: pad_seg). A key's or a
   pad's note whose partials stay under 9 kHz, and the organ on the keys, render at half the rate (half_ok), its
   role's half-rate voices upsampled by a half-band; the organ on the pad layer at a quarter, beside the ensemble. The
   pad's solo lane (lane 4) may hold its own patch (vx_assign_pad_solo), rendered beside the ensemble (pad_seg) into
   the pad's bus. A pitched note's level is the patch envelope: a natural decay (nat, from 1 toward sus) times a gate
   (g: a linear rise, a smoothed fall), so a note-off, a steal or vx_release_all fades instead of clicking. Drums are a noise burst or a swept sine. Then the mix (Task 8: the role
   gains, the keys' level per comping, the mud high-pass on the chords', the lead's, the ear candy's and the pad layer's
   sums), per role bus (chords, bass, lead, candy, pad, drums): the drums through their compressor, each bus through its
   section filters (a one-pole high-pass and low-pass, vx_set_filter/vx_set_hp; skipped while every bus is open), the
   chords' dry scaled for the chorus, the song gain; the buses summed into the music, the plate's send (SEND per role)
   and the tape delay's (each role's patch's delay), the chords' bus and the pad layer's send (its patch's chorus) kept
   for the chorus; then the lo-fi chain (fx.c: chorus, delay, dust, ducking, the soft clip, the plate, the bed).
   Per-sample work is bounded by the fixed voice count; the filters and the compressor move at a control rate of BCR
   samples. */
#include <stdint.h>
#include <string.h>
#include "fastmath.h"
#include "library.h"
#include "voices.h"
#include "drums.h"
#include "skits.h"
#include "fx.h"
#include "rng.h"
#include "harmony.h"
#include "strum.h"
#include "pads.h"
#include "sampler.h"
#include "modal.h"

#define QN 64
#define NCH 8 /* chord voices: a held pad uses all 8 (its release crosses the next chord's swell), the rest the first 4 */
#define NCH_KEYS 4
#define NSTR 6 /* a strum's strings: the chords' first 6 voices, one each (the other 2 damp re-struck strings) */
#define PAD_LONG 22050u /* a pad note this long (half a second) or longer is held: it may take the pool's other 4 */
#define NLD 2 /* lead voices */
#define NCD 2 /* candy voices */
#define NPD 4 /* the pad layer's voices: a 3-note chord and the solo note */
#define NFM (NCH + NLD + NCD)
#define VB NFM       /* the bass voice: fv[VB] */
#define PD (VB + 1)  /* the pad layer's first voice, after the bass */
#define NV (PD + NPD) /* every pitched voice */
#define INV_SR (1.0f / 44100.0f)
#define SR 44100.0f
#define T60 6.9078f  /* ln 1000: time constants in a -60 dB time */
#define OFF_THR 1e-3f
#define G_STEAL 0.02f /* gate fall per sample for a steal: below STEAL_THR in about 4 ms */
#define STEAL_THR 0.02f
#define AMP_K 0.01f /* bass level slew per sample (about 2 ms) */
#define CHUNK 256 /* vx_render's blocks through the lo-fi chain */
/* A pad lane's legato after a short rest: a note within LEG_GAP samples of its voice's last note's end glides. */
#define LEG_GAP 1102u /* 25 ms */
#define ENS_GLIDE 0.06f  /* a pad lane's glide to its next note, in seconds */
#define ENS_QR_TOP 4000.0f /* a pad note renders at a quarter of the rate while its low-pass (key-tracked) is this or
                              under, else at half */
#define NBUS 6   /* the role buses: V_CHORD, V_BASS, V_LEAD, V_CANDY, V_PAD and the drums (PM_*) */
#define DRB 5    /* the drums' bus */
#define BCR 32u  /* the buses' control rate: filters and the compressor per 32 samples */
#define FLT_RAMP ((float)BCR / 2205.0f) /* a filter move lands in 50 ms ... */
#define HP_OPEN ((float)BCR / 132.0f)   /* ... a high-pass opening in 3 ms */
#define LP_TOP 18000.0f  /* the low-pass at amount 0 (bypassed once settled there) ... */
#define LP_OCT 6.4919f   /* ... down to 200 Hz at 1: log2(18000 / 200) */
#define HP_BOT 30.0f     /* the high-pass's one-pole at amount 0+ ... */
#define HP_OCT 4.3219f   /* ... up to 600 Hz at 1: log2(20); its mix fades in over the first eighth */
/* the drums' compressor: a threshold (peak) per kit (KIT_*: 3-6 dB off a full-velocity kick and snare in every kit;
   the sampled kits' by the synth kits' rule, half the geometric mean of the two peaks), 4:1, at most 6 dB, 0.9 dB of
   make-up */
static const float CP_THR_KIT[KIT_N] = {0.095f, 0.090f, 0.084f, 0.162f, 0.110f, 0.087f, 0.133f, 0.115f, 0.100f};
#define CP_SLOPE 0.75f   /* 1 - 1/4 */
#define CP_MIN 0.5f
#define CP_MAKEUP 1.11f
#define CP_REL (1.0f / (0.12f * 44100.0f)) /* the level's release: 120 ms */
#define KR_STOP (T60 / (0.04f * SR)) /* vx_release_all's release: 40 ms to -60 dB */
#define TINE_R 14.0f   /* the EP tine: a high partial, fading with the index */
#define STRIKE_T60 0.25f /* the EP strike's fade */
#define BARK_T60 0.04f   /* the Wurli bark's fade */
#define BARK_LP 0.3f     /* the bark's one-pole on the noise: about 2.5 kHz */
#define IDX_K 0.02f      /* the electric bass's index slew per sample (about 1 ms): a legato restrike doesn't step */
#define WIND_CR 32       /* the winds' control rate: the vibrato updates every 32 samples */
#define WIND_LP_LO 0.3f  /* a wind's low-pass at envelope 0, as a fraction of its cutoff at full */
#define VIB_RATE 5.0f    /* the winds' vibrato: 5 Hz, waiting 0.3 s, then fading in over 0.6 s */
#define VIB_DELAY 0.3f
#define VIB_FADE 0.6f
#define BREATH_K 1.4f    /* the breath's band-pass damping (1/Q): a wide band */
#define CHIFF 1.5f       /* the breath's extra at the onset (the chiff), gone in CHIFF_T60 */
#define CHIFF_T60 0.08f
#define MUTE_DRY 0.2f    /* a muted wind keeps this much of the open tone under the mute's band */
#define HR_TOP 9000.0f   /* a half-rate voice's highest partial: below its 11 kHz Nyquist and the upsampler's band edge */
#define HR_PAD_H 2.0f   /* a pad's highest partial counted: its upper saw's second harmonic (the filter takes the rest) */
#define HB_M 7           /* the 2x upsampler: a 27-tap half-band (Kaiser, beta 5), 7 coefficient pairs */
#define HB_H (2 * HB_M)  /* the half-rate samples it keeps between chunks (its latency: 2 HB_M = 14 samples) */
#define TW_MAXP 6        /* the organ: a registration's sounding wheels at most (the percussion's counted) */
#define TW_CR 16u        /* its control rate in samples: the rotors, the percussion, the wheels' gain ramps (TW_CS) */
#define TW_CS(v) ((v)->qr ? 4u * TW_CR : TW_CR) /* a voice's control step in samples: the pad's organ (a quarter)
                                                   every 16 of its steps, as cheap as its slow rotor allows */
#define TW_FOLD 6000.0f  /* a wheel above this folds down an octave (the top of a tonewheel set) */
#define TW_XOVER 800.0f  /* the rotary's split: wheels under it in the drum, over it in the horn */
#define TW_HORN_HZ 0.8f  /* the slow rotors (SLOOP's WHEEL: horn 0.8, drum 0.7 Hz) */
#define TW_DRUM_HZ 0.7f
#define TW_TOP 96        /* the manual's top note (C7): a raised note past it drops back an octave */
#define TW_SC_HZ 6.86f   /* the scanner chorus's rotation (412 rpm) */
#define TW_SC_D 0.35e-3f /* its delay's swing (C3), in seconds */
#define TW_SC_N 32u      /* its delay line, at half the rate or a quarter (a power of two) */
#define TW_SC_KEYS 0.635f /* its dry and delayed, each at this gain: the organ's level kept (an even mix of the two
                             loses 2.1 dB to their partial cancellation) */
#define TW_SC_PAD 0.7182f /* (the pad's organ, its 4' nearer the comb's first notch: matched to the warm pad) */
#define TW_SC_XF 0.06f    /* the pad's scanner switching on or off by section: a crossfade (s) */
/* a sampled instrument (M_SAMP) on the lead, over its level on the keys, per bank (Knight, nylon, Emilyguitar): its
   patch's level matches the Rhodes on the keys; the nylon +7 dB (the flute's level on the lead), the Knight +3.85 and
   the Emilyguitar +3.5, tuned on their sets' lead over keys (test_lead_over_keys: a struck lead reads 8-10 dB over the
   keys at about 4 dB under the flute's bar energy) */
static const float SAMP_LEAD[SB_N] = {1.5578f, 2.239f, 1.4962f};

/* big zero-initialised buffers go in the device's .pool RAM region (fm1-base's PLAT_POOL; a Mach-O host has no such
   section) */
#if defined PLAT_HOST || defined __APPLE__
#define VX_POOL
#else
#define VX_POOL __attribute__((section(".pool")))
#endif

/* the models the pad's solo lane may play (solo_sample) beside voice_sample: inlined into both, so voice_sample's
   loop (render_seg) keeps them inline */
#define VX_INLINE static inline __attribute__((always_inline))

/* The pool roles: their voices fv[first .. first+count) and level. */
typedef struct {
    uint8_t first, count;
    float gain;
} role_t;
#define SEND_DRUMS 0.06f
static const float PUMP_DB[VX_PUMP_N] = {1.0f, 3.0f, 6.0f};
/* The role gains (Task 8's mix): the lead the focus, the keys under it, the bass felt under them, the ear candy an
   ornament; each patch's level (patch.c) evens its instruments out within the role, so every set mixes alike: while
   the lead plays it sits about 4-6 dB over the keys (with the keys' duck, DUCK_*), whatever the set. */
static const role_t ROLES[V_PAD + 1] = {
    [V_CHORD] = {0, NCH, 0.0946f},  /* 4 dB down in all (Sam, twice: the comping quieter; the lead 8-10 dB over it) */
    [V_BASS] = {VB, 1, 0.0625f},
    [V_LEAD] = {NCH, NLD, 0.154f},
    [V_CANDY] = {NCH + NLD, NCD, 0.118f},
    [V_PAD] = {PD, NPD, 0.056f}, /* the pad layer under the keys (levels: the pads plan's Task 3) */
};

/* A pooled voice. The envelope fields are shared by every model; m is the model's own state. */
typedef struct {
    const vx_patch_t *pt; /* the patch it was struck with (kept through its release) */
    float nat, nk, sus, g, ka, kr, amp;        /* envelope: nat *= nk; g rises by ka, falls by g * kr */
    float amp_t;                               /* a slewing voice (slew): amp slews to amp_t times the natural decay */
    union {
        struct { /* M_EP: phases, increments, index and its decay, tine */
            float pc, pm, ic, im, idx, idec, tine;
            float hit, hk, bark, bk, by; /* EP: strike and its decay, bark level, decay, noise state */
            float la, ly;                /* EP: the tone one-pole's coefficient (0: none) and state */
            float bc;                    /* EP: the bark's noise one-pole's coefficient (at the voice's rate) */
        } ep;
        struct { /* M_PAD: two saws, then one SVF (a low-pass); at half the rate, a matched biquad in its place */
            float p1, p2, i1, i2, l2;
            float c1[4], s[2]; /* SVF coefficients (a1, a2, a3, k) and states (the biquad's: w1, w2) */
            float bq[5];       /* the half-rate biquad: b0, b1, b2, a1, a2 */
        } pad;
        struct { /* M_WIND: an oscillator blend, a two-pole low-pass following the envelope, a mute, breath, vibrato */
            float ph, inc, f0, pw, shp, a0, a1, l1, l2;
            float bc[4], bs[2], br, ch, chk; /* the breath's band-pass (coefficients, states), level, chiff */
            float mc[4], ms[2];             /* the mute's band-pass (mc[3] = 0: no mute) */
            float vp, vd;                   /* vibrato phase (turns) and depth (a fraction of f0) */
            float ft, gk;                   /* the solo lane's glide: f0 by gk a step, gn steps, to ft */
            uint32_t gn;
            uint32_t vt;                    /* samples since the onset, in control steps */
            uint8_t cc;
        } wd;
        struct { /* M_BASS: 2-op FM (the index's excess ix over the floor i0 slewing to the struck it, which falls)
                    plus a sub-octave sine */
            float pc, pm, ps, ic, im, is, ix, it, i0, ik, sub;
        } bs;
        struct { /* M_CHIP: a phase and its increment; the crush: its steps per unit of output (a unit of the
                    voice's level, amp_t, is 2^(bits-1) steps; 0 for none) and their size; the last output before and
                    after it (tests: vx_chip_out) */
            float ph, inc, q, qi, raw, out, lk, lp;
        } ch;
        struct { /* M_ENS: the saw's and its partner's phases and increments, the partner's level; the 2-pole
                    low-pass's coefficient and states, or at half the rate its matched biquad's (q, w); a lane's glide
                    (gn steps left, by gk a step, to t1, t2), and its level's ramp to a new velocity's (an steps left, by
                    ak a step, to the voice's amp_t) */
            float p1, p2, i1, i2, l2, a, y1, y2;
            float q[5], w[2];
            float t1, t2, gk, ak;
            uint32_t gn, an;
        } en;
        smp_voice_t sp; /* M_SAMP: the sampler voice (sampler.h), its envelope its own */
        struct { /* M_TW: the sounding wheels (n, compact): phases and increments (Q32 turns), the unmodulated
                    increment (a fraction of a step), the bar's gain, the gain now and its ramp per step, the rotor
                    (hn: the horn's); the percussion's wheel (ps, -1: none), its level and fall per control step; the
                    click's level, fall and high-pass, its own noise (nz); the rotors' phases (turns); the onset (t0);
                    the control count, the key's release not yet clicked (hd), the note the wheels are on (nt) */
            uint32_t ph[TW_MAXP], inc[TW_MAXP];
            float bi[TW_MAXP], bg[TW_MAXP], g[TW_MAXP], gd[TW_MAXP];
            float pe, pk, ck, ckk, hl, r0, r1;
            uint32_t t0, nz;
            int8_t ps;
            uint8_t n, cc, hd, nt, hn[TW_MAXP];
        } tw;
        md_voice_t md;  /* M_MODAL: the modeled vibraphone (modal.h) */
    } m;
    uint32_t gate, age;
    uint8_t on, held, role;
    uint8_t note; /* the last note struck */
    uint32_t off; /* when its gate closes (or closed): the engine's clock (t_on) at its onset plus its length */
    uint8_t slew; /* the level slews (the bass's AMP_K): the bass, and the pad's solo lane's own voice */
    uint8_t hr;   /* rendered at half the rate, on even samples (a step is two), into its role's upsampler */
    uint8_t qr;   /* the pad layer's: rendered at a quarter of the rate (a step is four; pad_seg) */
    uint8_t pend; /* a steal in progress: fade out, then start pe on ppt */
    uint8_t damp; /* a re-struck string's last note, moved to a spare voice: fades out as a steal does, then off */
    const vx_patch_t *ppt;
    song_ev_t pe;
} voice_t;

typedef struct {
    song_ev_t e;
    uint32_t rem;
} qe_t;

static qe_t q[QN];
static int qn;
static voice_t fv[NV];
static uint32_t age_ctr, rng = 0x1234567u;
static uint32_t clk;  /* the engine's clock at the chunk's start */
static uint32_t t_on; /* the engine's clock (samples since vx_init) at the events firing now (vx_render) */
static uint32_t n_steal; /* tests (vx_steals): chord voices stolen while still sounding */
static float gain, gain_tgt, gain_step;
static float ch_m[CHUNK], ch_s[CHUNK]; /* a chunk's music (into fx_process_pairs) and its right channel (out) */
static float bb[NBUS][CHUNK];          /* the chunk's role buses (the drums' always; the others while filtering) */
/* a bus's section filters: amounts ramping to their targets (steps per block), the one-poles' coefficients, states */
typedef struct {
    float a, at, as, h, ht, hs; /* low-pass, high-pass amounts */
    float y, z, hm;             /* low-pass state; high-pass one-pole state and mix */
    float ka, kh, ac, hc;       /* the one-poles' coefficients, and the amounts they were made for */
} bus_t;
static bus_t bus[NBUS];
/* The half-rate voices: summed per role (V_CHORD, V_LEAD, V_CANDY, V_PAD; the bass is never half-rate) at every even sample of
   the engine's clock into hx[role], after HB_H samples of the last chunk's (the upsampler's history); then each role
   with something in it is upsampled to the full rate into its bus. */
#define NHX (HB_H + CHUNK / 2 + 1)
static float hx[V_PAD + 1][NHX] VX_POOL;
static float hup[CHUNK] VX_POOL;
static float pq0, pq1; /* the pad layer's quarter-rate sum's last two samples: its interpolation's ends (pad_seg) */
static float pq_mc[3], pq_mz[2]; /* its mud high-pass at a quarter of the rate: coefficients, states */
/* The pad bus's section filters on the quarter-rate sum (pad_seg), at that rate, before its interpolation up: the
   bus's own amounts (bus[V_PAD], moved in the mix as any bus's), the one-poles' coefficients for a step of 4 samples
   (1 - (1 - k)^4: the same corner), their states and whether each ran in the last segment (a move starts from the
   signal, as bus_filter's). The bus's full-rate filters then see only its full- and half-rate voices (rare: a note
   whose low-pass is over 4 kHz, or VX_OFF_HALF). */
static float pb_y, pb_z, pb_ka4, pb_kh4, pb_ac = -1.0f, pb_hc = -1.0f;
static uint8_t pb_lon, pb_hon;
static float pq_buf[CHUNK]; /* the chunk's quarter-rate sum, filtered, at the full rate (pq_used: written) */
static int pq_used;         /* pq_buf holds the chunk's quarter-rate sum so far (else it is silent: not read) */
static int pd_filt;         /* the chunk's section filters run (vx_render's filt) */
static uint32_t hpar;        /* the engine's clock's parity at the chunk's start: 0, its first sample is even */
static uint32_t hn;          /* render_seg's place: the half-rate samples made so far in the chunk */
static unsigned hr_live, hr_tail; /* per role bit: a half-rate voice rendered in the chunk; the history not yet silent */
static uint32_t hw;          /* hx holds the chunk's half-rate samples [0, hw) (rendered or zeroed) */
static int dr_live;          /* a drum sounded in the chunk (else the drums' bus is all zeros) */
/* The plate's per-bus sends (a fraction of the bus's level): keys and lead wet, ear candy and the pad layer wetter,
   the bass dry, the drums a touch. */
static const float sendp[NBUS] = {[V_CHORD] = 0.30f, [V_BASS] = 0.0f, [V_LEAD] = 0.35f, [V_CANDY] = 0.45f,
                                  [V_PAD] = 0.40f, [DRB] = SEND_DRUMS}; /* (the bass's 0 is not summed) */
static float dsend[NBUS];   /* the delay's per-bus send: the role's patch's delay */
/* The pad layer's chorus: one chorus at the chords' patch's depth, so the pad's send into it is its own patch's depth
   over the chords' (at most PAD_CH_MAX times; 0 with no chorus), its wet then as deep as its patch asks; its dry is
   scaled as the chords' is for their chorus (fx_chorus_dry_at its depth), eased as theirs. */
#define PAD_CH_MAX 4.0f
static float csend;         /* the pad layer's send into the chorus */
static float pd_dep;        /* the depth that send gives it */
static float pdg = 1.0f, pdg0, pdgd; /* its dry scale: eased toward fx_chorus_dry_at(pd_dep); this chunk's ramp */
static float pd_lvl = 1.0f;          /* the pad layer's level (vx_set_pad_level), folded into pdg's target */
/* The formant choir (INS_CHOIR: an M_ENS patch with p[4] set, on the pad layer or its solo lane): its voices sum
   apart from the other ensemble voices, at the quarter rate, into one formant bank (one bank, not one per voice: the
   choir sings one vowel), then join the layer's sum. The bank: Klatt's two-pole resonators (Klatt 1980), F1 and F2 in
   cascade (unity at DC: the body below F1 stays), F3 in parallel on their input (unity at its peak, times its level
   CH_G3: the cascade alone leaves it 35-40 dB under F1), at sung-vowel formants (tenor and alto averaged, the
   bandwidths widened by half for a choir's spread). The vowel moves from "oo" to "aah" and back over CH_MORPH_T (a
   log-frequency blend), all three formants drifting together by CH_DRIFT over CH_DRIFT_T, both on the engine's clock.
   The coefficients change once per CH_UPD samples of that clock (the morph's fastest move, about 0.3 octave a second
   at F1, is under 8 cents a step there), at the same samples however the chunks fall. The resonances lift whatever
   harmonics land on a formant, so the bank's gain swings with the chord's register and the vowel: its output is
   held to its input's power, both followed over CH_LVL_T and the gain ramped across each CH_UPD (a level, not a
   compressor: the swell passes through it). The patch's level sets the choir's (within 0.5 dB of the warm pad's).
   With VX_OFF_HALF (every voice at the full rate) the choir sings without its formants. */
#define CH_UPD 1024u      /* the clock samples between coefficient (and gain) updates: a multiple of 4 */
#define CH_LVL_T 0.4f     /* the power match's time constant (s) */
#define CH_DRIFT 0.03f    /* the formants' slow drift: +-3% ... */
#define CH_DRIFT_T 11.0f  /* ... over 11 s */
#define CH_MORPH_T 12.0f  /* "oo" -> "aah" -> "oo" */
/* sung "oo" and "aah": F1-F3 and their bandwidths (Hz); F3's level (about 25 and 20 dB under F1 in the pad's bus) */
static const float CH_F[2][3] = {{340.0f, 650.0f, 2615.0f}, {725.0f, 1115.0f, 2725.0f}};
static const float CH_BW[2][3] = {{70.0f, 90.0f, 250.0f}, {120.0f, 135.0f, 250.0f}};
static const float CH_G3[2] = {4.0f, 7.0f};
static float cho_xc[CHUNK / 4 + 2]; /* the segment's choir voices, at the quarter rate */
static float cho_c[3][3], cho_s[3][2]; /* the resonators' a, b, c (y = a x + b y1 + c y2) and y1, y2 */
static float cho_f[3];                /* their formants (Hz), for vx_choir_formant */
static uint32_t cho_cell = 0xFFFFFFFFu; /* the CH_UPD cell of the clock the coefficients are for (none yet) */
static float cho_pi, cho_po, cho_ai, cho_ao; /* the input's and output's power, followed; this cell's sums */
static float cho_g = 1.0f, cho_dg;         /* the gain matching them, and its ramp per quarter-rate sample */
static int pad_force = INS_NONE;         /* vx_pad_ins: the pad layer's instrument over the song's (tests) */
/* The mud high-pass (Sam: "add both"): a second-order (Butterworth) high-pass on the chords', the lead's, the ear
   candy's and the pad layer's sums at MUD_HZ (MUD_PAD_HZ for a pad or the pad layer), so the bass and the kick keep
   the low end. Per role, at the full rate and at half (its half-rate voices' sum, before the upsampler; the pad
   layer's also at a quarter, its quarter-rate sum's: pq_mc): b0, a1, a2 (b1 = -2 b0, b2 = b0; transposed direct form II, two
   states); b0 = 0 with no patch. VX_OFF_MUD leaves it out. */
#define MUD_HZ 135.0f
#define MUD_PAD_HZ 150.0f
static float mud_c[V_PAD + 1][2][3];               /* [role][full, half][b0, a1, a2] */
static float mud_z[V_PAD + 1][2], mud_zh[V_PAD + 1][2]; /* the states, full rate and half */
static void mud_coefs(float *c, float hz, float sr)
{
    float w = FM_TWO_PI * hz / sr, cs = fm_cosf(w), al = fm_sinf(w) * 0.70710678f, a0 = 1.0f / (1.0f + al);
    c[0] = 0.5f * (1.0f + cs) * a0;
    c[1] = -2.0f * cs * a0;
    c[2] = (1.0f - al) * a0;
}
static float cg = 1.0f;     /* the chords' dry scale, toward fx_chorus_dry() */
/* The felt piano on the lead (Sam, 2026-10-29): its patch's level is the keys'; on the lead it sat 2-3 dB under the
   other leads (the lead spike), so it comes up to them */
#define FELT_LEAD_G 1.45f
static float keys_g = 1.0f; /* the song's keys level for its comping (keys_gain) */
/* The keys' duck under the lead (Sam: "the lead leads"): while a lead note's gate is open the chords' bus dips
   DUCK_DB, a follower at the chunk rate (a one-pole toward 1 with a note held, else 0: about 25 ms in, 400 ms out,
   so a run of notes holds it down and a rest lets the keys back), the keys' gain ramped linearly over each chunk. */
#define DUCK_DEPTH 0.29205f /* 1 - 10^(-3/20): 3 dB down at full duck */
/* The pad layer dips with them, 1 dB (Task 1b: it plays more of its bars under the lead now, where the keys duck, and
   stays 5 dB or more under them; the full 3 dB would sink it past 11 dB under in the grooves' sets) */
#define DUCK_PAD 0.10875f   /* 1 - 10^(-1/20) */
#define DUCK_KA 0.20717f    /* 1 - exp(-CHUNK / (25 ms * SR)) */
#define DUCK_KR 0.014409f   /* 1 - exp(-CHUNK / (400 ms * SR)) */
static float dk_env;        /* the follower: 0 at rest, 1 fully ducked */
static float kg = 1.0f;     /* the keys' gain (the chorus's dry scale times the duck) at the last chunk's end */
static float kg0, kgd;      /* this chunk's: at its start, the step per sample */
static float cp_env, cp_g = CP_MAKEUP; /* the compressor's level and gain (make-up included) */
static float cp_thr = 0.095f;          /* the kit's threshold */
static unsigned fx_off;     /* VX_OFF_*: kept through vx_init */
static int solo_bus = -1;   /* vx_solo: the one bus mixed, -1 for all (kept through vx_init) */
/* vx_mute: a bit per bus out of the mix (kept through vx_init), and the mask at the last chunk's end (a chunk ramps a
   bus between the two: out or back in over its samples) */
static unsigned mute_mask, mute_was;
static uint32_t gain_n;
static vx_config_t cfg;
static const vx_patch_t *role_pt[V_PAD + 1]; /* the configured patch per pitched role, NULL for none */
static vx_patch_t mute_pt; /* a sampled chords patch muted (p[3] 1): the strum's chucks (a V_CHORD event's pad 1) */
/* The pad's solo lane (lane 4, vx_assign_pad_solo): its patch (NULL: the pad's) and its level's trim, by ear and by
   the level test (song_test's test_pad_solo_patch): each voice as loud as the strings an octave up (in RMS, the flute
   and the organ at MIDI 76, the strings at 88); the formant choir as loud as the vox pad it replaced over C5-C6
   (test_choir_solo: the mean power of every note, the vox pad's as measured when it went, 2026-10-31). */
#define SOLO_V (NV - 1) /* the solo lane's voice: fv[SOLO_V] */
/* the solo lane is the pad layer's last voice (C99 has no _Static_assert: size -1 fails the build) */
typedef char solo_is_last_pad_[(PD + NPD == NV && SOLO_V == PD + NPD - 1) ? 1 : -1];
static const vx_patch_t *solo_pt;
static float solo_trim = 1.0f;
/* (the organ's, the pad's one-stop 4', at a quarter of the rate as on the pad layer; the flute at half) */
static const float SOLO_TRIM[INS_NONE] = {[INS_FLUTE] = 0.83271f, [INS_ORGAN] = 0.7585f,
                                         [INS_STRINGS] = 1.0f, [INS_CHOIR] = 1.131f};

/* The organ: the song's registration (ORG_*) and register (high: its chords sit high), as vx_configure rolls them;
   the tests' forced ones (vx_organ_force, -1: the song's). The scanner chorus's lines: [0] the keys' half-rate sum,
   [1] the pad's organ's quarter-rate sum; the pad's switched by section (vx_organ_scan: on, and its mix now, 0 the
   plain organ to 1 the chorus). The wheels' sine (1024 steps, Q15, a plain lookup: about -60 dB of error, under the
   dust), made at vx_init. */
static int org_reg, org_high, org_freg = -1, org_fhigh = -1;
typedef struct {
    float buf[TW_SC_N], ph;
    uint32_t w;
} tw_sc_t;
static tw_sc_t tw_sc[2];
static int psc_on = 1;
static float psc_m = 1.0f;
static int16_t tw_sin[1024];

static float noise(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (float)(int32_t)rng * (1.0f / 2147483648.0f);
}

static float note_inc(int note)
{
    return 440.0f * fm_exp2f(((float)note - 69.0f) * (1.0f / 12.0f)) * INV_SR;
}

/* per-sample coefficients for a -60 dB time t, and the linear rise for an attack time (one sample at least) */
static float k_t60(float t) { return t * SR > T60 ? T60 / (t * SR) : 1.0f; }
static float k_att(float t) { return t * SR > 1.0f ? 1.0f / (t * SR) : 1.0f; }

/* The voices' side cleared for a song (every voice and drum off, the queue, the buses' filters, the half-rate
   history and clock parity, the mud states, the compressor), the noise generators seeded from the song's seed: with
   nothing sounding, a song starts from the same state whatever played before it (the same seed, the same samples). */
static void voices_clear(uint32_t seed)
{
    memset(q, 0, sizeof q);
    memset(fv, 0, sizeof fv);
    drums_song(seed);
    qn = 0;
    age_ctr = 0;
    n_steal = 0u;
    t_on = clk = 0u;
    rng = seed * 2654435761u ^ 0x1234567u;
    if (!rng)
        rng = 0x1234567u;
    memset(bus, 0, sizeof bus);
    memset(hx, 0, sizeof hx);
    memset(mud_z, 0, sizeof mud_z);
    memset(mud_zh, 0, sizeof mud_zh);
    pq0 = pq1 = 0.0f;
    pq_mz[0] = pq_mz[1] = 0.0f;
    pb_y = pb_z = 0.0f;
    pb_lon = pb_hon = 0;
    pq_used = 0;
    memset(cho_s, 0, sizeof cho_s); /* (the choir's vowel starts again with the clock: "oo") */
    cho_cell = 0xFFFFFFFFu;
    cho_pi = cho_po = cho_ai = cho_ao = cho_dg = 0.0f;
    cho_g = 1.0f;
    hpar = 0u;
    hr_tail = 0u;
    mute_was = mute_mask; /* (nothing sounds: no ramp) */
    cp_env = 0.0f;
    cp_g = CP_MAKEUP;
    dk_env = 0.0f;
    kg = cg;
    memset(tw_sc, 0, sizeof tw_sc); /* the organ's scanners */
    psc_m = (float)psc_on;
}

void vx_init(void)
{
    int i;
    for (i = 0; i < 1024; i++) /* the organ's wheels' sine */
        tw_sin[i] = (int16_t)fm_floorf(32767.0f * fm_sin_turns((float)i * (1.0f / 1024.0f)) + 0.5f);
    voices_clear(0u); /* (the kit: vx_set_kit below) */
    gain = gain_tgt = 1.0f;
    gain_step = 0.0f;
    gain_n = 0;
    cg = kg = 1.0f;
    pdg = 1.0f;
    pd_lvl = 1.0f;
    keys_g = 1.0f;
    fx_init(); /* before the roles: the chords' patch sets the chorus */
    memset(&cfg, 0, sizeof cfg);
    vx_assign_pad_solo(INS_NONE);
    vx_assign(V_CHORD, INS_RHODES);
    vx_assign(V_BASS, INS_SUB);
    vx_assign(V_LEAD, INS_VIBES);
    vx_assign(V_CANDY, INS_NONE); /* no ear candy (Sam, 2026-10-29): the role stays, silent */
    vx_assign(V_PAD, INS_STRINGS);
    vx_set_kit(KIT_DUSTY);
    cfg.flavor = FL_LOFI;
    vx_set_pump(VX_PUMP_SUBTLE);
}

/* a role's patch, its delay send, its mud high-pass and (the chords') the chorus's depth or (the pad layer's) its
   send into the chorus */
static void assign_pt(int voice, const vx_patch_t *p)
{
    role_pt[voice] = p;
    dsend[voice] = role_pt[voice] && voice != V_BASS && !(fx_off & VX_OFF_DELAY) ? role_pt[voice]->delay : 0.0f;
    if (voice != V_BASS) {
        int m = role_pt[voice] ? role_pt[voice]->model : -1;
        float hz = m == M_PAD || m == M_ENS || (voice == V_PAD && m == M_TW) ? MUD_PAD_HZ : MUD_HZ;
        mud_coefs(mud_c[voice][0], hz, SR);
        mud_coefs(mud_c[voice][1], hz, 0.5f * SR);
        if (voice == V_PAD) /* (and the pad layer's at a quarter) */
            mud_coefs(pq_mc, hz, 0.25f * SR);
    }
    if (voice == V_CHORD && p && p->model == M_SAMP) {
        mute_pt = *p;
        mute_pt.p[3] = 1.0f;
    }
    if (voice == V_CHORD)
        fx_chorus(role_pt[voice] && !(fx_off & VX_OFF_CHORUS) ? role_pt[voice]->chorus : 0.0f);
    if (voice == V_CHORD || voice == V_PAD) { /* the pad's send: its depth over the chords' */
        float cd = role_pt[V_CHORD] && !(fx_off & VX_OFF_CHORUS) ? role_pt[V_CHORD]->chorus : 0.0f;
        float pd = role_pt[V_PAD] && !(fx_off & VX_OFF_CHORUS) ? role_pt[V_PAD]->chorus : 0.0f;
        csend = cd > 0.0f ? fm_minf(pd / cd, PAD_CH_MAX) : 0.0f;
        pd_dep = csend * cd;
    }
}

void vx_assign_patch(int voice, const vx_patch_t *p)
{
    if (voice >= V_CHORD && voice <= V_PAD)
        assign_pt(voice, p);
}

void vx_assign(int voice, int ins)
{
    if (voice < V_CHORD || voice > V_PAD)
        return;
    /* the organ: the song's registration and register on the keys, the one-stop 4' on the pad layer */
    assign_pt(voice, ins != INS_ORGAN ? vx_patch(ins)
                     : voice == V_PAD ? vx_organ_pad()
                                      : vx_organ_keys(org_freg >= 0 ? org_freg : org_reg,
                                                      org_fhigh >= 0 ? org_fhigh : org_high));
    ins = role_pt[voice] ? ins : INS_NONE;
    switch (voice) {
    case V_CHORD: cfg.chords = (uint8_t)ins; break;
    case V_BASS: cfg.bass = (uint8_t)ins; break;
    case V_LEAD: cfg.lead = (uint8_t)ins; break;
    case V_PAD: cfg.pad = (uint8_t)ins; break;
    default: cfg.candy = (uint8_t)ins; break;
    }
}

static uint8_t bed_off;                   /* host renders: VNL_BED=0 leaves surface noise and rain out */
void vx_bed_off(int on) { bed_off = (uint8_t)(on != 0); }

void vx_pad_ins(int ins)
{
    pad_force = ins == INS_ORGAN || (vx_patch(ins) && vx_patch(ins)->model == M_ENS) ? ins : INS_NONE;
}

void vx_organ_force(int reg, int high)
{
    org_freg = reg >= 0 && reg < ORG_N ? reg : -1;
    org_fhigh = high >= 0 ? high != 0 : -1;
}

void vx_organ_scan(int on) { psc_on = on != 0; }

/* The keys' level for the song's comping, so the keys sit alike whatever they play (Task 8, retuned with half notes
   in, pads revision 2): struck or plucked keys held decay under the mix, so they sit up (a two-bar chord further);
   half notes restrike, so a little (their second strike softer, a busy lead thins
   them to the chord changes); an arpeggio about level; half notes that hold (half_held) sit as held; a strum (struck
   harder, many strokes a bar) under them all, the ballad's three strokes a bar less so; held on 2 (and 4) as held,
   light eighths as half notes. A pad sustains, except its arpeggio (short notes that barely swell). */
static float keys_gain(const song_roll_t *r)
{
    const lib_set_t *s = lib_set_by_id((uint8_t)r->v[RF_SET]);
    const lib_prog_t *pg = lib_prog_by_id((uint8_t)r->v[RF_PROG]);
    int style = harmony_style(r), ins = s ? s->chords : INS_NONE, two = pg && pg->bars_per_chord >= 2;
    int gtr = ins == INS_NYLON || ins == INS_EMILY;
    /* held and half notes with no arpeggio in the draw (3025034, cf0dca0) carry more energy: the Rhodes, Wurli and
       guitars 0.75 dB down, the DX as it was, the Knight 0.75 dB up (its pad sat too close) */
    float db = 0.0f, cut = ins == INS_KNIGHT ? 0.75f : ins == INS_DX_EP ? 0.0f : -0.75f;
    if (style == CS_HALF && !harmony_half_strikes(r)) /* half notes that hold (half_held) sit as held */
        style = CS_HELD;
    if (lib_ins_sustained(ins))
        db = style == CS_ARP ? 3.0f : 2.5f;   /* held: +2.5 for the softer strikes (Sam) */
    else if (style == CS_HELD)
        db = (two ? 6.75f : 6.0f) + cut;   /* +2.25: the softer strikes (Sam) */
    else if (style == CS_HALF)
        db = (gtr ? 3.5f : 3.0f) + cut;   /* a guitar's half notes a little up (struck softly, they sank under a struck lead) */
    else if (style == CS_STABS)
        db = (two ? 2.75f : 2.0f) - (ins == INS_KNIGHT ? 3.0f : gtr ? 1.0f : 0.0f);   /* struck harder (vel 60-85),
                                                    so 4 dB under held; the Knight's hammer 3 dB more, a guitar's
                                                    pluck 1 (their sets' lead over keys) */
    else if (style == CS_ARP)
        db = -0.25f;
    else if (style == CS_STRUM)   /* struck harder, many strokes a bar: under the arpeggio; the ballad's three less */
        db = strum_pattern(r, NULL) == SP_BALLAD ? -3.0f : -4.5f;
    else if (style == CS_HELD2 || style == CS_HELD24)   /* as held, a bar's chord at most (a first guess: Sam sets
                                                           the level by ear) */
        db = 6.0f + cut;
    else if (style == CS_LIGHT8)  /* struck softer than half notes (vel 30-38) but eight times a bar: as half notes */
        db = 3.0f + cut;
    return fm_db2lin(db);
}

void vx_configure(const song_roll_t *r)
{
    const lib_set_t *s = lib_set_by_id((uint8_t)r->v[RF_SET]);
    if (s) {
        if (s->chords == INS_ORGAN) { /* (the organ's registration and register: vx_assign) */
            org_reg = lib_song_organ(r);
            org_high = harmony_keys_high(r);
        }
        vx_assign(V_CHORD, s->chords);
        vx_assign(V_BASS, s->bass);
        vx_assign(V_LEAD, s->lead);
        vx_assign(V_CANDY, INS_NONE); /* no ear candy (Sam, 2026-10-29): the set's candy goes unplayed */
        vx_assign(V_PAD, pad_force != INS_NONE ? pad_force : lib_song_pad(r)); /* the set's, or the choir drawn */
        vx_set_kit(lib_song_kit(r)); /* the song's: the set's synth kit or its sampled one */
        vx_assign_pad_solo(pads_solo_voice(r)); /* the song's solo voice (none: the pad's, unplayed) */
        cfg.flavor = s->flavor;
    }
    keys_g = keys_gain(r);
    cfg.dust = (uint8_t)fm_clampf((float)r->v[RF_DUST], 0.0f, 3.0f);
    cfg.noise = (uint8_t)fm_clampf((float)r->v[RF_NOISE], 0.0f, 2.0f);
    cfg.ambience = (uint8_t)fm_clampf((float)r->v[RF_AMBIENCE], 0.0f, 1.0f);
    if (bed_off) cfg.noise = cfg.ambience = 0;
    {
        rng_t g;
        rng_seed(&g, r->seed, 170); /* the pump: a third of songs each */
        vx_set_pump(rng_int(&g, VX_PUMP_N));
    }
    fx_configure(cfg.dust, cfg.noise, cfg.ambience, cfg.flavor);
    fx_tempo((float)r->v[RF_TEMPO]);
    if (!vx_active()) /* nothing to hear the dust ramp on */
        fx_land();
}

int vx_reconfigures(const song_roll_t *a, const song_roll_t *b)
{
    /* the set's instruments, the progression (the keys' level: keys_gain), the chain, the delay's time */
    static const uint8_t F[] = {RF_SET, RF_PROG, RF_DUST, RF_NOISE, RF_AMBIENCE, RF_TEMPO};
    unsigned i;
    for (i = 0; i < sizeof F; i++)
        if (a->v[F[i]] != b->v[F[i]])
            return 1;
    return lib_song_kit(a) != lib_song_kit(b); /* the kit (the seed's draw over the set) */
}

void vx_song(const song_roll_t *r)
{
    /* the app skips vx_init while a tail (the plate's, the bed's fade) still sounds: with the voices silent, their
       side starts afresh here too (a voice still sounding keeps the engine as it is) */
    if (!vx_active())
        voices_clear(r->seed);
    fx_song(r->seed, !vx_active());
    vx_configure(r);
    if (!vx_active()) { /* the chorus's dry scales land (they ease only while rendering) */
        kg = cg = fx_chorus_dry();
        pdg = fx_chorus_dry_at(pd_dep) * pd_lvl;
    }
}

const vx_config_t *vx_config(void)
{
    return &cfg;
}

void vx_event(const song_ev_t *e)
{
    if (qn == QN) {
        memmove(&q[0], &q[1], (QN - 1) * sizeof q[0]);
        qn--;
    }
    q[qn].e = *e;
    q[qn].rem = e->delay;
    qn++;
}

/* the solo lane's level trim: lane 4's voice on its own patch (SOLO_TRIM), else 1 */
static float solo_lvl(const voice_t *v, const vx_patch_t *p)
{
    return v == &fv[NV - 1] && p == solo_pt ? solo_trim : 1.0f;
}

/* a voice's step in samples: 1, 2 at half the rate, 4 at a quarter */
static float step_len(const voice_t *v) { return v->qr ? 4.0f : v->hr ? 2.0f : 1.0f; }

/* The envelope's start, shared by every model: nat back to 1, the gate rising from where it is. A slewing voice's
   level (the bass's, the solo lane's own voice's) slews from where it is on a legato retrigger, else starts at its
   target. */
static void env_start(voice_t *v, const vx_patch_t *p, const song_ev_t *e, int legato)
{
    float dv = step_len(v); /* a half-rate voice's steps are two samples long, a quarter-rate one's four */
    v->pt = p;
    v->nat = 1.0f;
    v->nk = 1.0f - k_t60(p->dec / dv);
    v->sus = p->sus;
    /* a pad's (or the pad layer's) swell takes at most half its note: a short note (an arpeggio's) speaks rather than
       never arriving */
    v->ka = k_att(((p->model == M_PAD || p->model == M_ENS || (p->model == M_TW && v->role == V_PAD)) && e->len
                       ? fm_minf(p->att, 0.5f * (float)e->len * INV_SR)
                       : p->att) /
                  dv);
    v->kr = k_t60(p->rel / dv);
    v->amp_t = ROLES[v->role].gain * p->level * (float)e->vel * (1.0f / 127.0f) *
               (v->role == V_CHORD ? keys_g : solo_lvl(v, p)) *
               (p->model == M_SAMP && v->role == V_LEAD ? SAMP_LEAD[(int)p->p[0] % SB_N] : 1.0f) *
               (v->role == V_LEAD && cfg.lead == INS_FELT_PIANO ? FELT_LEAD_G : 1.0f);
    /* (the solo lane's own voice slews, so its legato notes ramp to their velocity's level: lane_legato) */
    v->slew = (uint8_t)(v->role == V_BASS || (v->role == V_PAD && p->model != M_ENS));
    if (p->model == M_SAMP) { /* the sampler's envelope is its own (sampler.c): the gate only steals and stops */
        v->nk = v->sus = v->ka = 1.0f;
        v->kr = 0.0f;
    }
    if (!legato)
        v->amp = v->amp_t;
    v->gate = (e->len + (uint32_t)dv - 1u) / (uint32_t)dv;
    v->held = 1;
    v->on = 1;
    v->pend = 0;
    v->damp = 0;
    v->note = e->note;
    v->off = t_on + e->len; /* (a stolen voice's note starts up to ~5 ms later: its end is read a little early) */
}

/* M_EP's 2-op FM core: p[0..4] as patch.c documents. */
static void fm_start(voice_t *v, const song_ev_t *e)
{
    const vx_patch_t *p = v->pt;
    float dv = v->hr ? 2.0f : 1.0f, f = note_inc(e->note) * dv, v01 = (float)e->vel * (1.0f / 127.0f);
    v->m.ep.ic = f;
    v->m.ep.im = f * p->p[0];
    v->m.ep.idx = p->p[1] + p->p[2] * v01;
    v->m.ep.idec = 1.0f - k_t60(p->p[3] / dv);
    v->m.ep.tine = p->p[4] * v01;
}

/* one-pole low-pass coefficient for a cutoff in Hz (note-on only: it calls fm_expf) */
static float k_lp1(float hz) { return 1.0f - fm_expf(-FM_TWO_PI * fm_clampf(hz, 20.0f, 18000.0f) * INV_SR); }

/* M_EP: the 2-op FM (velocity into the index, the index decaying: the tine's bell into a round body) plus
   a strike that fades fast, a noise bark and a tone one-pole, as patch.c lays out p[]. The modulator restarts at
   the carrier's phase, so every note of a patch has the same attack. */
static void ep_start(voice_t *v, const song_ev_t *e)
{
    const vx_patch_t *p = v->pt;
    float dv = v->hr ? 2.0f : 1.0f, v01 = (float)e->vel * (1.0f / 127.0f);
    fm_start(v, e);
    v->m.ep.pm = v->m.ep.pc;
    v->m.ep.hit = p->p[5] * v01;
    v->m.ep.hk = 1.0f - k_t60(STRIKE_T60 / dv);
    v->m.ep.bk = 1.0f - k_t60(BARK_T60 / dv);
    v->m.ep.bc = v->hr ? 1.0f - (1.0f - BARK_LP) * (1.0f - BARK_LP) : BARK_LP; /* the same one-pole, two samples a step */
    /* white noise drawn at half the rate has twice the power in band: the bark scaled so its one-pole's output power
       (a / (2 - a) of the noise's) matches the full rate's */
    v->m.ep.bark = p->p[6] * v01 *
                   (v->hr ? fm_sqrtf((BARK_LP / (2.0f - BARK_LP)) * (2.0f - v->m.ep.bc) / v->m.ep.bc) : 1.0f);
    v->m.ep.by = v->m.ep.ly = 0.0f; /* the union may hold another model's state */
    v->m.ep.la = p->p[7] > 0.0f ? k_lp1(p->p[7] * (0.6f + 0.4f * v01) * dv) : 0.0f;
}

/* The TPT state-variable filter's coefficients (Zavalishin), as AMB-1's svf_coefs: f in Hz, k the damping (1/Q). */
static void svf_coefs(float *c, float f, float k)
{
    float g = fm_tanf(FM_PI * fm_clampf(f, 40.0f, 16000.0f) * INV_SR);
    c[0] = 1.0f / (1.0f + g * (g + k));
    c[1] = g * c[0];
    c[2] = g * c[1];
    c[3] = k;
}

/* A second-order low-pass (lp: 1) or a band-pass (unity peak) at f Hz, damping k (1/Q), at the rate SR / dv, matched
   to the analog response up to Nyquist (Vicanek 2016, "Matched Second Order Digital Filters"): the bilinear SVF falls
   short of it toward Nyquist, which at half the rate is inside the band the keys' and pads' spectrum is kept to. The
   numerator is added, scaled by gain, into q[0..2] (the poles into q[3..4]), so two responses can share the poles. */
static void bq_match(float *q, float f, float k, int lp, float gain, float dv)
{
    float w = FM_TWO_PI * fm_clampf(f, 40.0f, 9000.0f) * dv * INV_SR, z = fm_clampf(0.5f * k, 0.01f, 0.99f), qq = 1.0f / k;
    float a1 = -2.0f * fm_expf(-z * w) * fm_cosf(fm_sqrtf(1.0f - z * z) * w), a2 = fm_expf(-2.0f * z * w);
    float sn = fm_sinf(0.5f * w), p1 = sn * sn, p0 = 1.0f - p1, p2 = 4.0f * p0 * p1;
    float A0 = (1.0f + a1 + a2) * (1.0f + a1 + a2), A1 = (1.0f - a1 + a2) * (1.0f - a1 + a2), A2 = -4.0f * a2;
    float b0, b1, b2;
    if (lp) {
        float r1 = (A0 * p0 + A1 * p1 + A2 * p2) * qq * qq, B1 = fm_maxf((r1 - A0 * p0) / p1, 0.0f);
        b0 = 0.5f * (fm_sqrtf(A0) + fm_sqrtf(B1));
        b1 = fm_sqrtf(A0) - b0;
        b2 = 0.0f;
    } else {
        float r1 = A0 * p0 + A1 * p1 + A2 * p2, r2 = -A0 + A1 + 4.0f * (p0 - p1) * A2;
        float B2 = (r1 - r2 * p1) / (4.0f * p1 * p1), B1 = fm_maxf(r2 + 4.0f * (p1 - p0) * B2, 0.0f);
        b1 = -0.5f * fm_sqrtf(B1);
        b0 = 0.5f * (fm_sqrtf(fm_maxf(B2 + b1 * b1, 0.0f)) - b1);
        b2 = -b0 - b1;
    }
    q[0] += gain * b0;
    q[1] += gain * b1;
    q[2] += gain * b2;
    q[3] = a1;
    q[4] = a2;
}

/* one direct-form-II step of a biquad q on x, its states w[0..1] */
static float bq_step(const float *q, float *w, float x)
{
    float u = x - q[3] * w[0] - q[4] * w[1], y = q[0] * u + q[1] * w[0] + q[2] * w[1];
    w[1] = w[0];
    w[0] = fm_flush(u);
    return y;
}

/* M_PAD: two saws detuned apart, the second at a ratio, into one low-pass SVF. The filter doesn't move during a note:
   its coefficients are set here. */
static void pad_start(voice_t *v, const song_ev_t *e)
{
    const vx_patch_t *p = v->pt;
    float dv = step_len(v), f = note_inc(e->note), dt = fm_exp2f(p->p[0] * (1.0f / 1200.0f));
    v->m.pad.i1 = f * dv / dt;
    v->m.pad.i2 = f * dv * dt * p->p[1];
    v->m.pad.p1 = 0.0f;
    v->m.pad.p2 = 0.5f;
    v->m.pad.l2 = p->p[2];
    memset(v->m.pad.s, 0, sizeof v->m.pad.s);
    if (v->hr | v->qr) {
        memset(v->m.pad.bq, 0, sizeof v->m.pad.bq);
        bq_match(v->m.pad.bq, p->p[3] + p->p[4] * f * SR, p->p[5], 1, 1.0f, dv);
    } else
        svf_coefs(v->m.pad.c1, p->p[3] + p->p[4] * f * SR, p->p[5]);
}

/* M_ENS's low-pass for a note of f (cycles per sample): two one-poles at the cutoff (at a quarter or the full rate),
   or at half the rate the matched biquad of their response (two real poles: damping 2), as the half-rate pads'. */
static void ens_lp(voice_t *v, float f)
{
    const vx_patch_t *p = v->pt;
    float hz = p->p[2] + p->p[3] * f * SR;
    if (v->hr) {
        memset(v->m.en.q, 0, sizeof v->m.en.q);
        bq_match(v->m.en.q, hz, 2.0f, 1, 1.0f, 2.0f);
    } else
        v->m.en.a = k_lp1(hz * step_len(v));
}

/* M_ENS: the saw from phase 0, its partner p[0] cents sharp from half a cycle on; the 2-pole's coefficient fixed for
   the note (two one-poles at the cutoff). */
static void ens_start(voice_t *v, const song_ev_t *e)
{
    const vx_patch_t *p = v->pt;
    float dv = step_len(v), f = note_inc(e->note);
    v->m.en.i1 = f * dv;
    v->m.en.i2 = f * dv * fm_exp2f(p->p[0] * (1.0f / 1200.0f));
    v->m.en.p1 = 0.0f;
    v->m.en.p2 = 0.5f;
    v->m.en.l2 = p->p[1];
    v->m.en.y1 = v->m.en.y2 = 0.0f;
    v->m.en.w[0] = v->m.en.w[1] = 0.0f;
    ens_lp(v, f);
    v->m.en.gn = 0u;
    v->m.en.an = 0u;
}

/* M_ENS on a held lane (pool_on): the same pitch extends the note; another glides to it over ENS_GLIDE, in even steps
   of pitch, the envelope going on (no restrike) and the low-pass's key tracking set to the new note. Either way the
   level ramps to the new velocity's over ENS_GLIDE (a dark block's softer note, the solo's own level). The voice keeps
   its rate. */
static void ens_legato(voice_t *v, const song_ev_t *e)
{
    const vx_patch_t *p = v->pt;
    float dv = step_len(v), na = fm_maxf(ENS_GLIDE * SR / dv, 1.0f);
    v->gate = (e->len + (uint32_t)dv - 1u) / (uint32_t)dv;
    v->held = 1;
    v->off = t_on + e->len;
    v->amp_t = ROLES[v->role].gain * p->level * (float)e->vel * (1.0f / 127.0f) *
               solo_lvl(v, p); /* (as env_start: V_PAD) */
    if (v->amp_t != v->amp) {
        v->m.en.an = (uint32_t)na;
        v->m.en.ak = (v->amp_t - v->amp) / (float)v->m.en.an;
    }
    if (e->note != v->note) {
        float f = note_inc(e->note), n = fm_maxf(ENS_GLIDE * SR / dv, 1.0f);
        v->m.en.t1 = f * dv;
        v->m.en.t2 = f * dv * fm_exp2f(p->p[0] * (1.0f / 1200.0f));
        v->m.en.gn = (uint32_t)n;
        v->m.en.gk = fm_exp2f(fm_log2f(v->m.en.t1 / v->m.en.i1) / (float)v->m.en.gn);
        ens_lp(v, f);
        v->note = e->note;
    }
}

/* M_WIND: one phase drives the blend (sine, triangle, pulse); a two-pole low-pass opens with the envelope; a muted
   wind goes through a narrow band-pass; a band of breath noise rides under it, with a chiff at the onset; the
   vibrato waits, then fades in. */
static void wind_start(voice_t *v, const song_ev_t *e)
{
    const vx_patch_t *p = v->pt;
    float dv = v->hr ? 2.0f : 1.0f, f = note_inc(e->note) * dv, v01 = (float)e->vel * (1.0f / 127.0f);
    float hi = k_lp1(p->p[2] * f * SR * (0.6f + 0.4f * v01));
    v->m.wd.ph = 0.0f;
    v->m.wd.inc = v->m.wd.f0 = f;
    v->m.wd.pw = fm_clampf(p->p[1], 0.05f, 0.5f);
    v->m.wd.shp = fm_clampf(p->p[0], 0.0f, 2.0f);
    v->m.wd.a0 = k_lp1(WIND_LP_LO * p->p[2] * f * SR);
    v->m.wd.a1 = hi - v->m.wd.a0;
    v->m.wd.l1 = v->m.wd.l2 = 0.0f;
    svf_coefs(v->m.wd.bc, p->p[4] * dv, BREATH_K);
    v->m.wd.bs[0] = v->m.wd.bs[1] = 0.0f;
    /* white noise drawn at half the rate has twice the power in the breath's band: its level by 1/sqrt(2) */
    v->m.wd.br = p->p[3] * (v->hr ? 0.70710678f : 1.0f);
    v->m.wd.ch = CHIFF;
    v->m.wd.chk = 1.0f - k_t60(CHIFF_T60 / dv);
    if (p->p[6] > 0.0f)
        svf_coefs(v->m.wd.mc, p->p[6] * dv, p->p[7]);
    else
        v->m.wd.mc[3] = 0.0f;
    v->m.wd.ms[0] = v->m.wd.ms[1] = 0.0f;
    v->m.wd.vp = 0.0f;
    v->m.wd.vd = p->p[5] * (FM_LN2 / 1200.0f); /* cents, as a fraction of the frequency */
    v->m.wd.vt = 0;
    v->m.wd.cc = 0;
    v->m.wd.gn = 0u;
}

/* M_BASS: 2-op FM whose index slews to a struck target falling to its floor, plus a sine an octave down. A legato
   retrigger keeps the phases and slews the index, so it doesn't step; a fresh note starts every phase at 0. */
static void bass_start(voice_t *v, const song_ev_t *e, int legato)
{
    const vx_patch_t *p = v->pt;
    float f = note_inc(e->note), v01 = (float)e->vel * (1.0f / 127.0f);
    if (!legato) {
        v->m.bs.pc = v->m.bs.pm = v->m.bs.ps = 0.0f;
        v->m.bs.ix = p->p[2] * v01;
    }
    v->m.bs.ic = f;
    v->m.bs.im = f * p->p[0];
    v->m.bs.is = f * 0.5f;
    v->m.bs.i0 = p->p[1];
    v->m.bs.it = p->p[2] * v01;
    v->m.bs.ik = 1.0f - k_t60(p->p[3]);
    v->m.bs.sub = p->p[4];
}

/* M_CHIP: the phase from 0; the crush's step from the voice's level. */
static void chip_start(voice_t *v, const song_ev_t *e)
{
    const vx_patch_t *p = v->pt;
    v->m.ch.ph = 0.0f;
    v->m.ch.inc = note_inc(e->note);
    v->m.ch.qi = p->p[0] >= 1.0f ? v->amp_t * fm_exp2f(1.0f - fm_floorf(p->p[0])) : 0.0f;
    v->m.ch.q = v->m.ch.qi > 0.0f ? 1.0f / v->m.ch.qi : 0.0f;
    v->m.ch.raw = v->m.ch.out = v->m.ch.lp = 0.0f;
    /* the one-pole low-pass's coefficient for a cutoff p[1] times the note's frequency (0: none) */
    v->m.ch.lk = p->p[1] > 0.0f ? 1.0f - fm_exp2f(-9.0647f * p->p[1] * v->m.ch.inc) : 1.0f;   /* 2 pi / ln 2 */
}


/* A pad lane's voice not of the ensemble (the solo lane's flute, the organ) on a held lane (pool_on), as ens_legato:
   the same pitch extends the note; another glides to it over ENS_GLIDE in even steps of pitch, the envelope, the
   filter's state and the breath going on (no restrike; a wind's low-pass follows the new note at once), or (the
   organ) its wheels move to the new key at once; the level slews to the new velocity's (env_start's slew). The voice
   keeps its rate. */
static void lane_legato(voice_t *v, const song_ev_t *e)
{
    const vx_patch_t *p = v->pt;
    float dv = step_len(v), f = note_inc(e->note), n = fm_maxf(ENS_GLIDE * SR / dv, 1.0f), r;
    v->gate = (e->len + (uint32_t)dv - 1u) / (uint32_t)dv;
    v->held = 1;
    v->off = t_on + e->len;
    v->amp_t = ROLES[v->role].gain * p->level * (float)e->vel * (1.0f / 127.0f) * solo_lvl(v, p);
    if (e->note == v->note)
        return;
    v->note = e->note;
    if (p->model == M_WIND) {
        float hi;
        f *= dv;
        hi = k_lp1(p->p[2] * f * SR * (0.6f + 0.4f * (float)e->vel * (1.0f / 127.0f)));
        v->m.wd.ft = f;
        v->m.wd.gn = (uint32_t)n;
        v->m.wd.gk = fm_exp2f(fm_log2f(f / v->m.wd.f0) / (float)v->m.wd.gn);
        v->m.wd.a0 = k_lp1(WIND_LP_LO * p->p[2] * f * SR);
        v->m.wd.a1 = hi - v->m.wd.a0;
    } else if (p->model == M_TW) { /* retuned from the key its wheels are on (v->note has moved already) */
        int k;
        r = f / note_inc(v->m.tw.nt);
        for (k = 0; k < v->m.tw.n; k++)
            v->m.tw.bi[k] *= r;
        v->m.tw.nt = e->note;
    }
}

/* M_TW, the tonewheel organ, after SLOOP's WHEEL engine (its approach and values; our code). Each sounding drawbar is
   a sine from one shared table (tw_sin, a plain lookup) at its footage's ratio, folded down an octave above TW_FOLD
   (at a quarter of the rate, under that rate's Nyquist); a note's wheels start where a hash of the note and voice
   puts them (free-running, as the generators are). The percussion: one more wheel (the 4' or the 2 2/3') struck and
   falling, single trigger (a note while a chord voice struck earlier still holds joins its envelope where it has
   fallen to: a rolled chord rings together, legato playing gets none), the bars 3 dB down with it. The key click: a
   burst of high-passed noise at the key's contact (half of it at the release), about 3 ms. The rotary, light and
   cheap: two rotors (the horn over TW_XOVER, the drum under) give their wheels an amplitude and a Doppler pitch
   wobble, once per control step (no delay line); their phases follow the engine's clock, so every voice hears the same
   rotor. */
static const float TW_RATIO[9] = {0.5f, 1.5f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f};
static void tw_start(voice_t *v, const song_ev_t *e)
{
    const vx_patch_t *p = v->pt;
    int note = e->note + (int)p->p[15], k, n = 0, pk = p->p[9] >= 3.0f ? 4 : p->p[9] >= 2.0f ? 3 : -1, held = 0, i;
    float dv = step_len(v), hz, k3 = pk >= 0 ? 0.70795f : 1.0f, pe = 0.0f;
    for (i = 0; i < NCH; i++) /* single trigger: a chord voice struck before now, still held, shares its envelope */
        if (&fv[i] != v && fv[i].on && fv[i].held && !fv[i].pend && fv[i].pt && fv[i].pt->model == M_TW &&
            fv[i].m.tw.t0 != t_on) {
            held = 1;
            pe = fm_maxf(pe, fv[i].m.tw.pe);
        }
    while (note > TW_TOP)
        note -= 12;
    hz = note_inc(note) * SR;
    v->m.tw.ps = -1;
    for (k = 0; k < 9 && n < TW_MAXP; k++) {
        float f = hz * TW_RATIO[k], lv = p->p[k];
        if (lv <= 0.0f && k != pk)
            continue;
        while (f > (v->qr ? 0.1f * SR : TW_FOLD))
            f *= 0.5f;
        if (k == pk)
            v->m.tw.ps = (int8_t)n;
        v->m.tw.bi[n] = f * INV_SR * dv;
        v->m.tw.bg[n] = lv > 0.0f ? k3 * fm_exp2f((lv - 8.0f) * 0.5f) * (1.0f / 32768.0f) : 0.0f; /* 3 dB a step */
        v->m.tw.hn[n] = (uint8_t)(f >= TW_XOVER);
        v->m.tw.ph[n] = ((uint32_t)k * 0x9E3779B9u) ^ ((uint32_t)e->note * 0x7F4A7C15u) ^ ((uint32_t)(v - fv) << 27);
        v->m.tw.g[n] = v->m.tw.gd[n] = 0.0f;
        n++;
    }
    v->m.tw.n = (uint8_t)n;
    v->m.tw.nt = e->note;
    v->m.tw.pe = v->m.tw.ps < 0 ? 0.0f : held ? pe : 1.0f;
    v->m.tw.pk = fm_expf(-T60 * (float)TW_CS(v) / (fm_maxf(p->p[10], 0.01f) * SR));
    v->m.tw.ck = p->p[11];
    v->m.tw.ckk = fm_expf(-dv / (0.0012f * SR)); /* the click's fall: tau 1.2 ms */
    v->m.tw.hl = 0.0f;
    v->m.tw.nz = 0x9E3779B9u ^ ((uint32_t)e->note << 8) ^ (uint32_t)(v - fv);
    v->m.tw.t0 = t_on;
    v->m.tw.r0 = TW_HORN_HZ * INV_SR * (float)(t_on % 441000u); /* (10 s: whole turns of both rotors) */
    v->m.tw.r1 = TW_DRUM_HZ * INV_SR * (float)(t_on % 441000u);
    v->m.tw.r0 -= fm_floorf(v->m.tw.r0);
    v->m.tw.r1 -= fm_floorf(v->m.tw.r1);
    v->m.tw.cc = 0;
    v->m.tw.hd = 1;
}

/* M_TW's control step: the rotors and the percussion move; each wheel's gain ramps to its new value over the step and
   its increment takes its rotor's Doppler */
static void tw_ctl(voice_t *v)
{
    const vx_patch_t *p = v->pt;
    int k;
    float steps = (float)TW_CS(v) / step_len(v), am[2], fq[2], dh = p->p[13], fh = p->p[14];
    if (p->p[12] > 0.0f) {
        float c0 = fm_sin_turns(v->m.tw.r0 + 0.25f), c1 = fm_sin_turns(v->m.tw.r1 + 0.25f);
        am[1] = 1.0f - dh * (0.5f - 0.5f * c0);
        am[0] = 1.0f - 0.5f * dh * (0.5f - 0.5f * c1);
        fq[1] = 1.0f + fh * fm_sin_turns(v->m.tw.r0);
        fq[0] = 1.0f + 0.25f * fh * fm_sin_turns(v->m.tw.r1);
        v->m.tw.r0 += TW_HORN_HZ * (float)TW_CS(v) * INV_SR;
        v->m.tw.r1 += TW_DRUM_HZ * (float)TW_CS(v) * INV_SR;
        if (v->m.tw.r0 >= 1.0f)
            v->m.tw.r0 -= 1.0f;
        if (v->m.tw.r1 >= 1.0f)
            v->m.tw.r1 -= 1.0f;
    } else
        am[0] = am[1] = fq[0] = fq[1] = 1.0f;
    for (k = 0; k < v->m.tw.n; k++) {
        int h = v->m.tw.hn[k];
        float t = v->m.tw.bg[k];
        if (k == v->m.tw.ps)
            t += v->m.tw.pe * (1.0f / 32768.0f);
        t *= am[h];
        v->m.tw.gd[k] = (t - v->m.tw.g[k]) / steps;
        v->m.tw.inc[k] = (uint32_t)(v->m.tw.bi[k] * fq[h] * 4294967296.0f);
    }
    v->m.tw.pe = fm_flush(v->m.tw.pe * v->m.tw.pk);
}

/* M_TW's raw sample: the wheels, then the click */
static float tw_sample(voice_t *v)
{
    float s = 0.0f;
    int k;
    if (v->m.tw.cc == 0)
        tw_ctl(v);
    if (++v->m.tw.cc == (v->hr ? TW_CR / 2u : TW_CR)) /* (TW_CS(v) samples) */
        v->m.tw.cc = 0;
    for (k = 0; k < v->m.tw.n; k++) {
        v->m.tw.ph[k] += v->m.tw.inc[k];
        v->m.tw.g[k] += v->m.tw.gd[k];
        s += v->m.tw.g[k] * (float)tw_sin[v->m.tw.ph[k] >> 22];
    }
    if (v->m.tw.hd && !v->held) { /* the key's release: half a click */
        v->m.tw.hd = 0;
        v->m.tw.ck = 0.5f * v->pt->p[11];
    }
    if (v->m.tw.ck > 1e-4f) {
        uint32_t z = v->m.tw.nz;
        float y;
        z ^= z << 13;
        z ^= z >> 17;
        z ^= z << 5;
        v->m.tw.nz = z;
        y = (float)(int32_t)z * (1.0f / 2147483648.0f) - v->m.tw.hl;
        v->m.tw.hl += 0.3f * y;
        s += v->m.tw.ck * y;
        v->m.tw.ck *= v->m.tw.ckk;
    } else
        v->m.tw.ck = 0.0f;
    return s;
}

/* The organ's scanner chorus (C3), in place on n samples at the rate's decimation dec (2: the keys' half-rate sum; 4:
   the pad's organ's quarter-rate sum): a short delay line read at a depth swinging at the scanner's rate (linearly
   interpolated), mixed evenly with the dry, each at g. m (NULL: always on): the chorus's mix against the plain dry,
   ramping to mt (0 or 1) over TW_SC_XF; the line runs either way, so it switches on with its history. */
static void tw_scanner(tw_sc_t *sc, uint32_t dec, float g, float *x, uint32_t n, float *m, float mt)
{
    float mm = m ? *m : 1.0f, mk = (float)dec / (TW_SC_XF * SR);
    float a = TW_SC_D * SR / (float)dec, d0 = a + 1.5f, inc = TW_SC_HZ * ((float)dec / SR);
    uint32_t i;
    for (i = 0; i < n; i++) {
        float d = d0 + a * fm_sin_turns(sc->ph), r = (float)sc->w - d + (float)TW_SC_N, fr, y0;
        uint32_t k;
        sc->buf[sc->w & (TW_SC_N - 1u)] = x[i];
        k = (uint32_t)r;
        fr = r - (float)k;
        y0 = sc->buf[k & (TW_SC_N - 1u)];
        y0 = g * (x[i] + y0 + fr * (sc->buf[(k + 1u) & (TW_SC_N - 1u)] - y0));
        if (mm != mt)
            mm = mt > mm ? fm_minf(mm + mk, mt) : fm_maxf(mm - mk, mt);
        x[i] += mm * (y0 - x[i]);
        sc->w++;
        sc->ph += inc;
        if (sc->ph >= 1.0f)
            sc->ph -= 1.0f;
    }
    if (m)
        *m = mm;
}

/* Whether a note renders at half the rate: the keys and pads (not on the bass), when their highest partial (the FM's
   sidebands, the index plus three; the tine; the pad's upper saw's second harmonic) is below HR_TOP; the organ on the
   chords always (its wheels fold under TW_FOLD). Higher notes, and everything with VX_OFF_HALF, stay at the full
   rate. The pad layer's ensemble renders at a quarter of the rate where its key-tracked low-pass is ENS_QR_TOP or
   under (voice_start), else at half, and the organ there at a quarter (all in pad_seg); the pad's solo lane's own
   voice as its model does. */
static int half_ok(const voice_t *v, const vx_patch_t *p, const song_ev_t *e)
{
    float hz = note_inc(e->note) * SR, v01 = (float)e->vel * (1.0f / 127.0f), top;
    if ((fx_off & VX_OFF_HALF) || v->role == V_BASS)
        return 0;
    if (p->model == M_EP) {
        top = hz * (1.0f + p->p[0] * (p->p[1] + p->p[2] * v01 + 3.0f));
        if (p->p[4] > 0.0f)
            top = fm_maxf(top, TINE_R * hz);
    } else if (p->model == M_PAD)
        top = HR_PAD_H * hz * fm_maxf(p->p[1], 1.0f);
    else if (p->model == M_TW)
        return v->role == V_CHORD;
    else if (p->model == M_WIND) /* the pad's solo lane only (a soft note, low-passed) */
        return v->role == V_PAD && p->p[2] * hz < HR_TOP;
    else if (p->model == M_ENS) /* half where its low-pass is too high for a quarter */
        return p->p[2] + p->p[3] * hz > ENS_QR_TOP;
    else if (p->model == M_SAMP) { /* the keys: half while the zone's band, retuned, stays under the half rate's
                                      Nyquist (no fold; the upsampler's edge only darkens its top) */
        const smp_bank_t *b = smp_bank((int)p->p[0]);
        const smp_zone_t *z = b ? samp_zone(b, (float)e->note) : NULL;
        return v->role == V_CHORD && z &&
               0.5f * (float)z->rate * fm_exp2f(((float)e->note - (float)z->root) * (1.0f / 12.0f)) < 0.25f * SR;
    } else
        return 0;
    return top < HR_TOP;
}

static void voice_start(voice_t *v, const vx_patch_t *p, const song_ev_t *e, int legato)
{
    v->hr = (uint8_t)half_ok(v, p, e);
    v->qr = (uint8_t)((p->model == M_ENS || (p->model == M_TW && v->role == V_PAD)) && !(fx_off & VX_OFF_HALF) &&
                      !v->hr);
    env_start(v, p, e, legato);
    switch (p->model) {
    case M_EP: ep_start(v, e); break;
    case M_PAD: pad_start(v, e); break;
    case M_WIND: wind_start(v, e); break;
    case M_CHIP: chip_start(v, e); break;
    case M_ENS: ens_start(v, e); break;
    case M_TW: tw_start(v, e); break;
    case M_MODAL: md_start(&v->m.md, p, (float)e->note, (float)e->vel * (1.0f / 127.0f)); break;
    case M_SAMP:
        v->m.sp.dv = step_len(v);
        samp_start(&v->m.sp, p, (float)e->note, (float)e->vel * (1.0f / 127.0f));
        break;
    default: bass_start(v, e, legato); break; /* M_BASS */
    }
}

/* a released voice's level for the steal's pick: its gate, times a sampler's own envelope (its gate stays open) */
static float gate_lvl(const voice_t *v) { return v->pt && v->pt->model == M_SAMP ? v->g * v->m.sp.env : v->g; }

/* The chords' pool is NCH_KEYS voices, NCH for a held pad's note (PAD_LONG). A free voice starts at once. Otherwise
   the victim is the quietest released voice, else the oldest held one, and it fades out over about 4 ms (G_STEAL)
   before the new note starts: the new note begins up to ~5 ms after its sample-accurate time, with no step in the
   output. */
static void pool_on(const song_ev_t *e, int role)
{
    int i, pick = -1, first = ROLES[role].first, count = ROLES[role].count;
    const vx_patch_t *p = role_pt[role];
    voice_t *v;
    if (role == V_PAD && e->pad == NPD && solo_pt && p)
        p = solo_pt; /* the solo lane's own patch (any model: render_seg's, or the ensemble's pad_seg) */
    else if (!p || (first >= PD ? p->model != M_ENS && p->model != M_TW : p->model == M_ENS))
        return; /* no instrument on this role; the pad layer plays M_ENS or the organ (pad_seg), M_ENS only it */
    if (role == V_PAD && e->pad >= 1 && e->pad <= NPD) { /* a lane (voices.h): its own voice */
        v = &fv[PD + e->pad - 1];
        v->age = ++age_ctr;
        if (v->on && !v->pend && v->pt == p && (v->held || t_on - v->off < LEG_GAP)) {
            if (p->model == M_ENS)
                ens_legato(v, e); /* held (or just ended): extended, or gliding */
            else
                lane_legato(v, e);
            return;
        }
        if (!v->on) {
            v->g = 0.0f;
            v->role = (uint8_t)role;
            voice_start(v, p, e, 0);
        } else { /* released: it fades out, then the new note swells */
            v->pe = *e;
            v->ppt = p;
            v->pend = 1;
        }
        return;
    }
    if (role == V_CHORD && p->model == M_SAMP && (e->pad & 1))
        p = &mute_pt; /* a chuck: the sampler's muted note */
    if (role == V_CHORD && p->model == M_SAMP && e->pad >> 1 >= 1 && e->pad >> 1 <= NSTR) {
        /* a strum's string (pad bits 1-3: string 1-6): its own voice, as on a guitar. A re-strike damps the string's
           last note: moved to a spare voice (the pool's last two) that fades it out over about 4 ms while the new
           note starts on time; with no spare free, the string's voice fades first (a steal: up to ~5 ms late). No
           stroke takes another string's ringing voice. */
        v = &fv[first + (e->pad >> 1) - 1];
        v->age = ++age_ctr;
        if (v->on && !v->pend)
            for (i = first + NSTR; i < first + count; i++)
                if (!fv[i].on) {
                    fv[i] = *v;
                    fv[i].held = 0;
                    fv[i].damp = 1;
                    v->on = 0;
                    break;
                }
        if (!v->on) {
            v->g = 0.0f;
            v->role = (uint8_t)role;
            voice_start(v, p, e, 0);
        } else {
            n_steal++;
            v->pe = *e;
            v->ppt = p;
            v->pend = 1;
        }
        return;
    }
    if (role == V_CHORD && p->model != M_SAMP && (p->model != M_PAD || e->len < PAD_LONG))
        count = NCH_KEYS; /* (a held pad's chord change overlaps: its old chord releases under the new one's swell; a
                             sampled instrument's strum takes up to 6 strings while the last stroke rings out) */
    for (i = first; i < first + count; i++)
        if (!fv[i].on) {
            pick = i;
            break;
        }
    if (pick < 0)
        for (i = first; i < first + count; i++)
            if (!fv[i].held && !fv[i].pend && (pick < 0 || gate_lvl(&fv[i]) < gate_lvl(&fv[pick])))
                pick = i;
    if (pick < 0)
        for (i = first; i < first + count; i++)
            if (pick < 0 || fv[i].age < fv[pick].age)
                pick = i;
    v = &fv[pick];
    v->age = ++age_ctr;
    if (!v->on) {
        v->g = 0.0f;
        v->role = (uint8_t)role;
        voice_start(v, p, e, 0);
    } else if (role == V_BASS && !v->pend && v->pt == p && p->model == M_BASS) {
        voice_start(v, p, e, 1); /* legato: phases, gate and level carry over */
    } else { /* a steal; on the bass, a re-pluck or a new patch */
        n_steal += role == V_CHORD; /* (a voice still sounding: held, or in its release) */
        v->pe = *e;
        v->ppt = p;
        v->pend = 1;
    }
}

static void fire(const song_ev_t *e)
{
    switch (e->voice) {
    case V_CHORD:
    case V_LEAD:
    case V_CANDY:
    case V_PAD:
    case V_BASS: pool_on(e, e->voice); break;
    default:
        if (e->voice < V_N)
            drums_on(e);
        break;
    }
}

/* M_EP's FM core: the raw sample (unit level). */
static float fm_sample(voice_t *v)
{
    float s, mod;
    v->m.ep.pc += v->m.ep.ic;
    if (v->m.ep.pc >= 1.0f)
        v->m.ep.pc -= 1.0f;
    v->m.ep.pm += v->m.ep.im;
    if (v->m.ep.pm >= 1.0f)
        v->m.ep.pm -= 1.0f;
    mod = fm_sin_turns(v->m.ep.pm) * v->m.ep.idx;
    s = fm_sin_turns(v->m.ep.pc + mod * 0.159155f);
    if (v->m.ep.tine > 0.0f) /* the tine fades with the index */
        s += v->m.ep.tine * fm_sin_turns(v->m.ep.pc * TINE_R) * v->m.ep.idx;
    v->m.ep.idx = fm_flush(v->m.ep.idx * v->m.ep.idec); /* decays: flushed before it goes denormal */
    return s;
}

/* M_EP's raw sample: the FM and tine, the strike, the bark, the tone. */
static float ep_sample(voice_t *v)
{
    float s = fm_sample(v) * (1.0f + v->m.ep.hit);
    v->m.ep.hit = fm_flush(v->m.ep.hit * v->m.ep.hk);
    if (v->m.ep.bark > 1e-4f) { /* band-limited noise, gated by the tone: a reed's bark */
        v->m.ep.by += v->m.ep.bc * (noise() - v->m.ep.by);
        s += v->m.ep.bark * v->m.ep.by * (s < 0.0f ? -s : s);
        v->m.ep.bark *= v->m.ep.bk;
    }
    if (v->m.ep.la > 0.0f) {
        v->m.ep.ly = fm_flush(v->m.ep.ly + v->m.ep.la * (s - v->m.ep.ly));
        s = v->m.ep.ly;
    }
    return s;
}

/* a naive saw with a polyBLEP at the wrap: t the phase in turns, dt its increment */
static float saw_blep(float t, float dt)
{
    float y = 2.0f * t - 1.0f;
    if (t < dt) {
        float x = t / dt;
        y -= x + x - x * x - 1.0f;
    } else if (t > 1.0f - dt) {
        float x = (t - 1.0f) / dt;
        y -= x * x + x + x + 1.0f;
    }
    return y;
}

/* one TPT SVF step on x: the band-pass (unscaled) in *bp, the low-pass returned; s[0..1] its states */
static float svf_step(const float *c, float *s, float x, float *bp)
{
    float v3 = x - s[1], v1 = c[0] * s[0] + c[1] * v3, v2 = s[1] + c[1] * s[0] + c[2] * v3;
    s[0] = fm_flush(2.0f * v1 - s[0]);
    s[1] = fm_flush(2.0f * v2 - s[1]);
    *bp = v1;
    return v2;
}

/* M_PAD's raw sample: the two saws, then the low-pass. */
VX_INLINE float pad_sample(voice_t *v)
{
    float x, b1;
    x = saw_blep(v->m.pad.p1, v->m.pad.i1) + v->m.pad.l2 * saw_blep(v->m.pad.p2, v->m.pad.i2);
    v->m.pad.p1 += v->m.pad.i1;
    if (v->m.pad.p1 >= 1.0f)
        v->m.pad.p1 -= 1.0f;
    v->m.pad.p2 += v->m.pad.i2;
    if (v->m.pad.p2 >= 1.0f)
        v->m.pad.p2 -= 1.0f;
    if (v->hr | v->qr) /* the matched biquad */
        return bq_step(v->m.pad.bq, v->m.pad.s, x);
    return svf_step(v->m.pad.c1, v->m.pad.s, x, &b1);
}

/* M_ENS's raw sample: the saw and its partner through the two one-poles. */
static float ens_sample(voice_t *v)
{
    float x = saw_blep(v->m.en.p1, v->m.en.i1) + v->m.en.l2 * saw_blep(v->m.en.p2, v->m.en.i2), a = v->m.en.a;
    v->m.en.p1 += v->m.en.i1;
    if (v->m.en.p1 >= 1.0f)
        v->m.en.p1 -= 1.0f;
    v->m.en.p2 += v->m.en.i2;
    if (v->m.en.p2 >= 1.0f)
        v->m.en.p2 -= 1.0f;
    if (v->m.en.gn) { /* a lane's glide: even steps in pitch, landing exactly */
        if (--v->m.en.gn) {
            v->m.en.i1 *= v->m.en.gk;
            v->m.en.i2 *= v->m.en.gk;
        } else {
            v->m.en.i1 = v->m.en.t1;
            v->m.en.i2 = v->m.en.t2;
        }
    }
    if (v->hr)
        return bq_step(v->m.en.q, v->m.en.w, x);
    v->m.en.y1 += a * (x - v->m.en.y1); /* (the saws never fall silent while it runs: flushed per segment) */
    v->m.en.y2 += a * (v->m.en.y1 - v->m.en.y2);
    return v->m.en.y2;
}

/* M_WIND's raw sample. The vibrato runs at the control rate (WIND_CR). */
VX_INLINE float wind_sample(voice_t *v)
{
    float t = v->m.wd.ph, s = v->m.wd.shp, x, e, a, bp;
    if (v->m.wd.gn) { /* the solo lane's glide: even steps in pitch, landing exactly (the vibrato rides on it) */
        float k = --v->m.wd.gn ? v->m.wd.gk : v->m.wd.ft / v->m.wd.f0;
        v->m.wd.f0 = v->m.wd.gn ? v->m.wd.f0 * k : v->m.wd.ft;
        v->m.wd.inc *= k;
    }
    if (++v->m.wd.cc == (v->hr ? WIND_CR / 2 : WIND_CR)) { /* (every WIND_CR samples at either rate) */
        float r = ((float)(++v->m.wd.vt) * (WIND_CR * INV_SR) - VIB_DELAY) * (1.0f / VIB_FADE);
        v->m.wd.cc = 0;
        if (r > 0.0f) {
            v->m.wd.inc = v->m.wd.f0 * (1.0f + fm_minf(r, 1.0f) * v->m.wd.vd * fm_sin_turns(v->m.wd.vp));
            v->m.wd.vp += VIB_RATE * WIND_CR * INV_SR;
            if (v->m.wd.vp >= 1.0f)
                v->m.wd.vp -= 1.0f;
        }
    }
    if (s < 1.0f) /* sine to triangle */
        x = (1.0f - s) * fm_sin_turns(t) + s * (1.0f - 4.0f * fm_fabsf(t - 0.5f));
    else { /* triangle to pulse: a saw less a saw a width later, each with its polyBLEP */
        float t2 = t + v->m.wd.pw;
        if (t2 >= 1.0f)
            t2 -= 1.0f;
        x = (2.0f - s) * (1.0f - 4.0f * fm_fabsf(t - 0.5f)) +
            (s - 1.0f) * (saw_blep(t, v->m.wd.inc) - saw_blep(t2, v->m.wd.inc));
    }
    v->m.wd.ph = t + v->m.wd.inc;
    if (v->m.wd.ph >= 1.0f)
        v->m.wd.ph -= 1.0f;
    e = v->g * (v->sus + (1.0f - v->sus) * v->nat); /* the envelope opens the low-pass */
    a = v->m.wd.a0 + v->m.wd.a1 * e;
    v->m.wd.l1 = fm_flush(v->m.wd.l1 + a * (x - v->m.wd.l1));
    v->m.wd.l2 = fm_flush(v->m.wd.l2 + a * (v->m.wd.l1 - v->m.wd.l2));
    x = v->m.wd.l2;
    if (v->m.wd.mc[3] > 0.0f) { /* the mute: a narrow band, unity at its center */
        svf_step(v->m.wd.mc, v->m.wd.ms, x, &bp);
        x = MUTE_DRY * x + bp * v->m.wd.mc[3];
    }
    svf_step(v->m.wd.bc, v->m.wd.bs, noise(), &bp);
    x += v->m.wd.br * (1.0f + v->m.wd.ch) * bp * BREATH_K;
    v->m.wd.ch = fm_flush(v->m.wd.ch * v->m.wd.chk);
    return x;
}

/* M_BASS's raw sample: the FM and the sub-octave sine. */
static float bass_sample(voice_t *v)
{
    float s, idx;
    v->m.bs.pc += v->m.bs.ic;
    if (v->m.bs.pc >= 1.0f)
        v->m.bs.pc -= 1.0f;
    v->m.bs.pm += v->m.bs.im;
    if (v->m.bs.pm >= 1.0f)
        v->m.bs.pm -= 1.0f;
    if (v->m.bs.it > 0.0f || v->m.bs.ix > 0.0f) { /* the strike: both flush to exactly 0 once it has settled */
        v->m.bs.it = fm_flush(v->m.bs.it * v->m.bs.ik);
        v->m.bs.ix = fm_flush(v->m.bs.ix + IDX_K * (v->m.bs.it - v->m.bs.ix));
    }
    idx = v->m.bs.i0 + v->m.bs.ix;
    s = idx > 0.0f ? fm_sin_turns(v->m.bs.pc + fm_sin_turns(v->m.bs.pm) * idx * 0.159155f)
                   : fm_sin_turns(v->m.bs.pc); /* the sub: no modulator */
    if (v->m.bs.sub > 0.0f) {
        v->m.bs.ps += v->m.bs.is;
        if (v->m.bs.ps >= 1.0f)
            v->m.bs.ps -= 1.0f;
        s += v->m.bs.sub * fm_sin_turns(v->m.bs.ps);
    }
    return s;
}

/* M_CHIP's raw sample: a triangle of 32 steps a cycle, 16 levels. */
static float chip_sample(voice_t *v)
{
    float t = v->m.ch.ph;
    int k = (int)(t * 32.0f);
    t += v->m.ch.inc;
    v->m.ch.ph = t >= 1.0f ? t - 1.0f : t;
    return (float)(k < 16 ? k : 31 - k) * (2.0f / 15.0f) - 1.0f;
}

/* The gate's step after a voice's sample: a steal's fade (then the queued note starts), the note's end, the rise or
   the release (off below OFF_THR). */
static inline void gate_step(voice_t *v)
{
    if (v->pend) { /* fade the victim out, then start the queued note (first: a damped string stolen still starts
                      it; voice_start clears the damp) */
        v->g -= v->g * (G_STEAL * step_len(v));
        if (v->g < STEAL_THR)
            voice_start(v, v->ppt, &v->pe, 0);
        return;
    }
    if (v->damp) { /* a damped string: out over about 4 ms */
        v->g -= v->g * (G_STEAL * step_len(v));
        if (v->g < STEAL_THR)
            v->on = v->damp = 0;
        return;
    }
    if (v->held && v->gate && --v->gate == 0)
        v->held = 0;
    if (v->held) {
        v->g += v->ka;
        if (v->g > 1.0f)
            v->g = 1.0f;
    } else {
        v->g -= v->g * v->kr;
        if (v->g < OFF_THR)
            v->on = 0;
    }
}

/* One sample of a pooled voice: the model's raw sample times the envelope (a slewing voice's: its slewed level). */
static float voice_sample(voice_t *v)
{
    float s, o, lv, q = 0.0f;
    switch (v->pt->model) {
    case M_EP: s = ep_sample(v); break;
    case M_PAD: s = pad_sample(v); break;
    case M_MODAL: s = md_sample(&v->m.md); break;
    case M_WIND: s = wind_sample(v); break;
    case M_CHIP:
        s = chip_sample(v);
        q = v->m.ch.q;
        break;
    case M_TW: s = tw_sample(v); break;
    case M_SAMP: /* its gate closed (the note's end, vx_release_all): its own release; done: off */
        if (!v->held && v->m.sp.stage < 2)
            samp_release(&v->m.sp);
        s = samp_sample(&v->m.sp);
        if (!v->m.sp.on && !v->pend)
            v->on = 0;
        if (!v->slew) { /* (exact by construction: env_start sets its nk and sus to 1, so nat stays 1 and the common
                            path's level sus + (1 - sus) nat is exactly 1.0f; s * amp * 1 * g == s * amp * g) */
            o = s * v->amp * v->g;
            gate_step(v);
            return o;
        }
        break;
    default: s = bass_sample(v); break; /* M_BASS (the pad layer's M_ENS renders in pad_seg) */
    }
    v->nat = fm_flush(v->nat * v->nk); /* a note held past ~50 s would reach denormals */
    lv = v->sus + (1.0f - v->sus) * v->nat;
    if (v->slew) {
        v->amp = fm_flush(v->amp + (v->amp_t * lv - v->amp) * AMP_K);
        o = s * v->amp * v->g;
    } else
        o = s * v->amp * lv * v->g;
    if (q > 0.0f) { /* the chip's bit crush: the output rounded to its steps (a tail under half a step is 0) */
        v->m.ch.raw = o;
        o = fm_floorf(o * q + 0.5f) * v->m.ch.qi;
        v->m.ch.out = o;
    }
    if (v->pt->model == M_CHIP) /* the chip's low-pass after its crush (Sam: a little more mellow) */
        o = v->m.ch.lp = fm_flush(v->m.ch.lp + (o - v->m.ch.lp) * v->m.ch.lk);
    gate_step(v);
    return o;
}

/* the chunk's wet inputs, summed per pair of samples (the lo-fi chain's wet step, fx_process_pairs): sample i of the
   chunk is in pair (ph3 + i) / FX_PAIR; the sums of a pair are built in registers and stored once */
#define NTRI (CHUNK / FX_PAIR + 2)
static float t_s[NTRI], t_c[NTRI], t_d[NTRI]; /* the plate's send, the chords' bus, the delay's send */
static uint32_t ph3, tk, tt;                  /* the chunk's phase; render_seg's place: samples in its pair, index */
static float ts, tc, td;                      /* render_seg's open pair's sums */

/* the half-band's odd phase (its even phase is the centre tap, 1): a Kaiser-windowed sinc, normalized to unity */
static const float HB_G[HB_M] = {0.628231535f, -0.188138067f, 0.090460645f, -0.045399865f,
                                 0.020987195f, -0.007939147f, 0.001797704f};

/* one role's half-rate samples of the chunk (nm of them, after its history) to the full rate, c samples into hup;
   then the history kept */
static void hx_up(float *h, uint32_t nm, uint32_t c)
{
    const float *x = h + HB_H; /* x[m]: the chunk's m-th half-rate sample; x[-1] .. x[-HB_H] the history */
    uint32_t i;
    int m = 0;
    for (i = 0; i < c; i++)
        if (((hpar + i) & 1u) == 0u) { /* an even sample: its half-rate sample x[m] is in; out comes x[m - HB_M] */
            hup[i] = x[m - HB_M];
            m++;
        } else { /* an odd one: halfway between x[m-1-HB_M] and x[m-HB_M] */
            const float *q = x + m - HB_M; /* q[0] the later of the two */
            float y = 0.0f;
            int k;
            for (k = 0; k < HB_M; k++)
                y += HB_G[k] * (q[k] + q[-1 - k]);
            hup[i] = y;
        }
    memmove(h, h + nm, HB_H * sizeof(float));
}

/* a voice's role bit (its pool's) */
static unsigned role_bit(int j)
{
    return j < NCH ? 1u << V_CHORD : j < NCH + NLD ? 1u << V_LEAD : j < NFM ? 1u << V_CANDY : j == VB ? 1u << V_BASS
                                                                                                 : 1u << V_PAD;
}

/* the voices sounding (the pad layer's aside: pad_seg): bit j of *mf at the full rate, of the return at half; *hb
   the half-rate ones' role bits */
static unsigned seg_mask(unsigned *mf, unsigned *hb)
{
    unsigned h = 0u;
    int j;
    *mf = 0u;
    *hb = 0u;
    for (j = 0; j < PD; j++)
        if (fv[j].on) {
            if (fv[j].hr) {
                h |= 1u << j;
                *hb |= role_bit(j);
            } else
                *mf |= 1u << j;
        }
    return h;
}

/* hx's half-rate samples [hw, to) of the chunk zeroed (nothing was rendered there) */
static void hx_zero(uint32_t to)
{
    if (hw < to) {
        memset(hx[V_CHORD] + HB_H + hw, 0, (to - hw) * sizeof(float));
        memset(hx[V_LEAD] + HB_H + hw, 0, (to - hw) * sizeof(float));
        memset(hx[V_CANDY] + HB_H + hw, 0, (to - hw) * sizeof(float));
        memset(hx[V_PAD] + HB_H + hw, 0, (to - hw) * sizeof(float));
        hw = to;
    }
}

/* one step of a mud high-pass (coefficients q: b0, a1, a2) on x, in place; s0, s1 its states (transposed form II) */
#define MUD_STEP(x, q, s0, s1)                                                                                   \
    do {                                                                                                       \
        float x_ = (x), y_ = (q)[0] * x_ + (s0);                                                               \
        (s0) = -2.0f * (q)[0] * x_ - (q)[1] * y_ + (s1);                                                       \
        (s1) = (q)[0] * x_ - (q)[2] * y_;                                                                      \
        (x) = y_;                                                                                              \
    } while (0)

/* which of the mud high-pass's filters run here (bits 0-3: the chords', the lead's, the ear candy's, the pad layer's
   at the full rate; 4-7 at half; the pad layer's quarter-rate one runs in pad_seg): those with voices sounding at
   their rate (mf, mh: voice bits), or states not yet at rest */
static unsigned mud_on(int mud, unsigned mf, unsigned mh)
{
    static const int first[4] = {0, NCH, NCH + NLD, PD}, cnt[4] = {NCH, NLD, NCD, NPD},
                     role[4] = {V_CHORD, V_LEAD, V_CANDY, V_PAD};
    unsigned m = 0u;
    int r;
    if (!mud)
        return 0u;
    for (r = 0; r < 4; r++) {
        unsigned bits = ((1u << cnt[r]) - 1u) << first[r];
        if ((mf & bits) || mud_z[role[r]][0] != 0.0f || mud_z[role[r]][1] != 0.0f)
            m |= 1u << r;
        if ((mh & bits) || mud_zh[role[r]][0] != 0.0f || mud_zh[role[r]][1] != 0.0f)
            m |= 16u << r;
    }
    return m;
}

/* A pad lane's voice not of the ensemble (the solo lane's flute, the organ: M_WIND or M_TW), rendered in pad_seg at
   its rate, out of line and apart from voice_sample, which keeps its one call site (render_seg's loop, inlined
   there). */
static __attribute__((noinline)) float solo_sample(voice_t *v)
{
    float s = v->pt->model == M_WIND ? wind_sample(v) : tw_sample(v);
    v->nat = fm_flush(v->nat * v->nk); /* (voice_sample's envelope, a slewing voice's: env_start) */
    v->amp = fm_flush(v->amp + (v->amp_t * (v->sus + (1.0f - v->sus) * v->nat) - v->amp) * AMP_K);
    s *= v->amp * v->g;
    gate_step(v);
    return s;
}

/* A segment's pad layer (fv[PD..NV)): its M_ENS voices side by side in a loop of their own (no dispatch, the
   model and the envelope inline), times its dry scale (pdg, ramped); its other voices (solo_sample: the solo lane's
   flute, the organ) beside them at their rate (a quarter, half or full: voice_start, half_ok), as an ensemble voice
   at that rate, the organ's quarter-rate sum through its scanner (on or still by section: vx_organ_scan). A
   quarter-rate voice renders on the samples where
   the engine's clock is a multiple of 4, their sum through the layer's mud high-pass at that rate (pq_mc), back to the
   full rate by linear interpolation (a quarter-rate sample's latency; its low-pass keeps the folds and images quiet)
   into pd_f; a half-rate one (its low-pass too high for a quarter) on the even samples into pd_h, for its role's
   upsampler (render_seg: the mud high-pass at half, hx); a full-rate one (VX_OFF_HALF) at every sample into pd_f.
   Returns PAD_F for a full-rate voice, PAD_H for a half-rate one, PAD_Q for the quarter-rate sum (sounding, or not
   yet at rest), 0 for silence. */
#define PAD_F 1u
#define PAD_H 2u
#define PAD_Q 4u
static float pd_f[CHUNK], pd_h[CHUNK / 2 + 1];
static float ens_tick(voice_t *v)
{
    float o = ens_sample(v);
    if (v->m.en.an) /* a held lane's level ramping to its new velocity's */
        v->amp = --v->m.en.an ? v->amp + v->m.en.ak : v->amp_t;
    v->nat *= v->nk; /* (flushed per segment) */
    o *= v->amp * (v->sus + (1.0f - v->sus) * v->nat) * v->g;
    gate_step(v);
    return o;
}
/* A quarter-rate voice's next nq ticks added into xq (voice by voice, its state in registers: the same arithmetic as
   ens_tick, tick by tick); a steal's fade or a glide goes through ens_tick. Stops where the voice falls silent or
   leaves the quarter rate or the ensemble (a steal's new note at another rate, or on the solo lane's own patch). */
/* The pad's organ's next quarter-rate ticks added into x, from 0 (its state in registers between its control steps:
   the same arithmetic as solo_sample over tw_sample, tick by tick); a control step, a steal, a click and the key's
   release go through solo_sample. Returns the ticks done: nq, or fewer where the voice fell silent, left the quarter
   rate or the organ (pad_seg goes on from there). */
static uint32_t tw_run(voice_t *v, float *x, uint32_t nq)
{
    uint32_t j = 0;
    while (j < nq && v->on && v->qr && v->pt->model == M_TW) {
        if (v->pend || v->damp || v->m.tw.cc == 0 || v->m.tw.ck > 0.0f || !v->held) {
            x[j++] += solo_sample(v);
            continue;
        }
        {
            float nat = v->nat, nk = v->nk, amp = v->amp, at = v->amp_t, sus = v->sus, g = v->g, ka = v->ka;
            uint32_t gate = v->gate, cc = v->m.tw.cc;
            int n = v->m.tw.n, k, held = 1;
            for (; j < nq && held && cc; j++) {
                float o = 0.0f;
                for (k = 0; k < n; k++) {
                    v->m.tw.ph[k] += v->m.tw.inc[k];
                    v->m.tw.g[k] += v->m.tw.gd[k];
                    o += v->m.tw.g[k] * (float)tw_sin[v->m.tw.ph[k] >> 22];
                }
                nat = fm_flush(nat * nk);
                amp = fm_flush(amp + (at * (sus + (1.0f - sus) * nat) - amp) * AMP_K);
                o *= amp * g;
                if (gate && --gate == 0)
                    held = 0;
                else {
                    g += ka;
                    if (g > 1.0f)
                        g = 1.0f;
                }
                x[j] += o;
                if (++cc == TW_CR)
                    cc = 0;
            }
            if (!held) { /* (the gate closed on the last tick: its release's first step, as gate_step's) */
                g -= g * v->kr;
                if (g < OFF_THR)
                    v->on = 0;
            }
            v->nat = nat;
            v->amp = amp;
            v->g = g;
            v->gate = gate;
            v->held = (uint8_t)held;
            v->m.tw.cc = (uint8_t)cc;
        }
    }
    return j;
}
static void ens_run(voice_t *v, float *xq, uint32_t nq)
{
    uint32_t j = 0;
    while (j < nq && v->on && v->qr && v->pt->model == M_ENS) { /* (a steal's new note may leave the ensemble) */
        if (v->pend || v->m.en.gn || v->m.en.an) {
            xq[j++] += ens_tick(v);
            continue;
        }
        {
            float p1 = v->m.en.p1, p2 = v->m.en.p2, i1 = v->m.en.i1, i2 = v->m.en.i2, l2 = v->m.en.l2, a = v->m.en.a;
            float y1 = v->m.en.y1, y2 = v->m.en.y2, nat = v->nat, nk = v->nk, amp = v->amp, sus = v->sus, g = v->g;
            float ka = v->ka, kr = v->kr;
            uint32_t gate = v->gate;
            int held = v->held, on = 1;
            for (; j < nq; j++) {
                float x = saw_blep(p1, i1) + l2 * saw_blep(p2, i2), o;
                p1 += i1;
                if (p1 >= 1.0f)
                    p1 -= 1.0f;
                p2 += i2;
                if (p2 >= 1.0f)
                    p2 -= 1.0f;
                y1 += a * (x - y1);
                y2 += a * (y1 - y2);
                nat *= nk;
                o = y2;
                o *= amp * (sus + (1.0f - sus) * nat) * g;
                xq[j] += o;
                if (held && gate && --gate == 0)
                    held = 0;
                if (held) {
                    g += ka;
                    if (g > 1.0f)
                        g = 1.0f;
                } else {
                    g -= g * kr;
                    if (g < OFF_THR) {
                        on = 0;
                        j++;
                        break;
                    }
                }
            }
            v->m.en.p1 = p1;
            v->m.en.p2 = p2;
            v->m.en.y1 = y1;
            v->m.en.y2 = y2;
            v->nat = nat;
            v->g = g;
            v->gate = gate;
            v->held = (uint8_t)held;
            v->on = (uint8_t)on;
        }
    }
}
static float k4_of(float k)
{
    float r = 1.0f - k;
    r *= r;
    return 1.0f - r * r;
}
/* The choir's bank for the CH_UPD cell starting at clock t: the vowel's formants, the resonators' coefficients (at
   the quarter rate), and the power match's step (the cell just ended folded into the followers, the gain ramped to
   theirs over this cell; held where the output has gone silent). */
static void choir_cell(uint32_t t)
{
    static const float LR[3] = {1.09245f, 0.77853f, 0.05945f}; /* log2 of each formant's "aah" over its "oo" */
    uint32_t pd = (uint32_t)(CH_DRIFT_T * SR), pm = (uint32_t)(CH_MORPH_T * SR);
    float dr = 1.0f + CH_DRIFT * fm_sinf(FM_TWO_PI * (float)(t % pd) / (float)pd);
    float m = 0.5f - 0.5f * fm_cosf(FM_TWO_PI * (float)(t % pm) / (float)pm), kf, g;
    int j;
    for (j = 0; j < 3; j++) {
        float f = CH_F[0][j] * fm_exp2f(LR[j] * m) * dr, bw = CH_BW[0][j] + (CH_BW[1][j] - CH_BW[0][j]) * m;
        float r = fm_expf(-FM_PI * bw * 4.0f * INV_SR), w = FM_TWO_PI * f * 4.0f * INV_SR;
        cho_f[j] = f;
        cho_c[j][1] = 2.0f * r * fm_cosf(w);
        cho_c[j][2] = -r * r;
        if (j < 2) /* F1, F2 in cascade: unity at DC */
            cho_c[j][0] = 1.0f - cho_c[j][1] - cho_c[j][2];
        else /* F3 in parallel: unity at its peak, times its level */
            cho_c[j][0] = (CH_G3[0] + (CH_G3[1] - CH_G3[0]) * m) * (1.0f - r) *
                         fm_sqrtf(1.0f - 2.0f * r * fm_cosf(2.0f * w) + r * r);
    }
    kf = fm_expf(-(float)CH_UPD / (CH_LVL_T * SR));
    cho_pi = fm_flush(cho_pi * kf + cho_ai);
    cho_po = fm_flush(cho_po * kf + cho_ao);
    cho_ai = cho_ao = 0.0f;
    g = cho_po > 1e-12f ? fm_clampf(fm_sqrtf(cho_pi / cho_po), 0.05f, 4.0f) : cho_g;
    cho_dg = (g - cho_g) * (4.0f / (float)CH_UPD);
    cho_cell = t / CH_UPD;
}
/* nq quarter-rate samples of the choir's voices (x, the first at clock t, then every 4) through its bank, added into
   y; its coefficients and gain step at the CH_UPD cells' first samples */
static void choir_run(const float *x, float *y, uint32_t nq, uint32_t t)
{
    uint32_t i = 0;
    if (nq && t / CH_UPD != cho_cell) /* (a cell begun at rest: its coefficients now) */
        choir_cell(t - t % CH_UPD);
    while (i < nq) {
        uint32_t tc = t + 4u * i, end = i + (CH_UPD - tc % CH_UPD + 3u) / 4u;
        float a0 = cho_c[0][0], b0 = cho_c[0][1], c0 = cho_c[0][2], a1 = cho_c[1][0], b1 = cho_c[1][1], c1 = cho_c[1][2];
        float a2 = cho_c[2][0], b2 = cho_c[2][1], c2 = cho_c[2][2], g = cho_g, dg = cho_dg, ai = cho_ai, ao = cho_ao;
        float s00 = cho_s[0][0], s01 = cho_s[0][1], s10 = cho_s[1][0], s11 = cho_s[1][1], s20 = cho_s[2][0],
              s21 = cho_s[2][1];
        if (tc % CH_UPD == 0u && tc / CH_UPD != cho_cell) {
            choir_cell(tc);
            continue;
        }
        if (end > nq)
            end = nq;
        for (; i < end; i++) {
            float u = x[i], f1, f2, f3;
            f3 = a2 * u + b2 * s20 + c2 * s21;
            s21 = s20;
            s20 = f3;
            f1 = a0 * u + b0 * s00 + c0 * s01;
            s01 = s00;
            s00 = f1;
            f2 = a1 * f1 + b1 * s10 + c1 * s11;
            s11 = s10;
            s10 = f2;
            f2 += f3;
            ai += u * u;
            ao += f2 * f2;
            g += dg;
            y[i] += f2 * g;
        }
        cho_s[0][0] = fm_flush(s00);
        cho_s[0][1] = fm_flush(s01);
        cho_s[1][0] = fm_flush(s10);
        cho_s[1][1] = fm_flush(s11);
        cho_s[2][0] = fm_flush(s20);
        cho_s[2][1] = fm_flush(s21);
        cho_ai = ai;
        cho_ao = ao;
        cho_g = g;
    }
}
/* the choir's bank still rings (its resonators' states) */
static int choir_rings(void)
{
    int j;
    for (j = 0; j < 3; j++)
        if (fm_fabsf(cho_s[j][0]) > 1e-7f || fm_fabsf(cho_s[j][1]) > 1e-7f)
            return 1;
    return 0;
}
static unsigned pad_seg(uint32_t pos, uint32_t n)
{
    voice_t *vs[NPD], *os[NPD]; /* the ensemble's voices; the others (each at its own rate) */
    uint32_t i, ih = 0;
    unsigned got = 0u;
    int k, nv = 0, no = 0, mud = !(fx_off & VX_OFF_MUD);
    float z0 = pq_mz[0], z1 = pq_mz[1], a0 = pq0, a1 = pq1, gd = pdgd, g0 = pdg0 + gd * (float)pos;
    for (k = PD; k < NV; k++)
        if (fv[k].on) {
            if (fv[k].pt->model == M_ENS)
                vs[nv++] = &fv[k];
            else
                os[no++] = &fv[k];
            got |= fv[k].qr ? PAD_Q : fv[k].hr ? PAD_H : PAD_F;
        }
    if (!(got & PAD_Q)) { /* the quarter-rate path still ringing out, or at rest (then it is silent: no work) */
        if (fm_fabsf(a0) > 1e-7f || fm_fabsf(a1) > 1e-7f || fm_fabsf(z0) > 1e-7f || fm_fabsf(z1) > 1e-7f ||
            fm_fabsf(pb_y) > 1e-7f || fm_fabsf(pb_z) > 1e-7f || choir_rings())
            got |= PAD_Q;
        else {
            pq0 = pq1 = pq_mz[0] = pq_mz[1] = pb_y = pb_z = 0.0f;
            pb_lon = pb_hon = 0;
            memset(cho_s, 0, sizeof cho_s);
            if (pq_used)
                memset(pq_buf + pos, 0, n * sizeof(float));
        }
    }
    if (got & PAD_Q) { /* (the bus's amounts as the last chunk's mix left them: they follow a chunk late) */
        const bus_t *bf = &bus[V_PAD];
        int lon = pd_filt && bf->a > 0.0f, hon = pd_filt && (bf->h > 0.0f || bf->hm > 0.0f);
        float hm = bf->hm, y = pb_y, z = pb_z, d = (a1 - a0) * 0.25f, *o = pq_buf + pos;
        uint32_t ph = (clk + pos) & 3u, f0 = (4u - ph) & 3u, nq = f0 < n ? (n - f0 + 3u) / 4u : 0u, t = 0;
        static float xq[CHUNK / 4 + 2];
        int sing = 0;
        memset(xq, 0, nq * sizeof(float));
        for (k = 0; k < nv; k++)
            if (vs[k]->qr && vs[k]->pt->p[4] > 0.0f) { /* the choir's voices: into its bank */
                if (!sing++)
                    memset(cho_xc, 0, nq * sizeof(float));
                ens_run(vs[k], cho_xc, nq);
            } else if (vs[k]->qr)
                ens_run(vs[k], xq, nq);
        if (sing || choir_rings()) {
            if (!sing)
                memset(cho_xc, 0, nq * sizeof(float));
            choir_run(cho_xc, xq, nq, clk + pos + f0);
        }
        if (no) { /* the others at a quarter (till silent or leaving the rate: a steal), the organ via its scanner */
            static float xo[CHUNK / 4 + 2];
            int org = 0;
            for (k = 0; k < no; k++) {
                voice_t *so = os[k];
                float *x = so->pt->model == M_TW ? xo : xq;
                if (x == xo && !org++)
                    memset(xo, 0, nq * sizeof(float));
                for (t = x == xo ? tw_run(so, xo, nq) : 0; t < nq && so->on && so->qr && so->pt->model != M_ENS; t++)
                    x[t] += solo_sample(so);
            }
            if (org) {
                if (!(fx_off & VX_OFF_CHORUS))
                    tw_scanner(&tw_sc[1], 4u, TW_SC_PAD, xo, nq, &psc_m, (float)psc_on);
                for (t = 0; t < nq; t++)
                    xq[t] += xo[t];
            }
        }
        t = 0;
        if (!pq_used && pos)
            memset(pq_buf, 0, pos * sizeof(float));
        pq_used = 1;
        if (lon && bf->a != pb_ac) {
            pb_ac = bf->a;
            pb_ka4 = k4_of(k_lp1(LP_TOP * fm_exp2f(-LP_OCT * bf->a)));
        }
        if (hon && bf->h != pb_hc) {
            pb_hc = bf->h;
            pb_kh4 = k4_of(k_lp1(HP_BOT * fm_exp2f(HP_OCT * bf->h)));
        }
        for (i = 0; i < n;) {
            uint32_t run = 4u - ph, j;
            if (ph == 0u) {
                float x = xq[t++];
                x *= g0 + gd * (float)(i + 1u);
                if (mud)
                    MUD_STEP(x, pq_mc, z0, z1);
                if (hon) { /* the bus's high-pass: y = x - mix * one-pole(x) */
                    if (!pb_hon) {
                        z = x;
                        pb_hon = 1;
                    }
                    z += pb_kh4 * (x - z);
                    x -= hm * z;
                }
                if (lon) { /* its low-pass */
                    if (!pb_lon) {
                        y = x;
                        pb_lon = 1;
                    }
                    y += pb_ka4 * (x - y);
                    x = y;
                }
                a0 = a1;
                a1 = x;
                d = (a1 - a0) * 0.25f;
            }
            if (run > n - i)
                run = n - i;
            for (j = 0; j < run; j++)
                o[i + j] = a0 + d * (float)(ph + j);
            i += run;
            ph = (ph + run) & 3u;
        }
        if (!hon) {
            z = 0.0f;
            pb_hon = 0;
        }
        if (!lon) {
            y = 0.0f;
            pb_lon = 0;
        }
        pb_y = fm_flush(y);
        pb_z = fm_flush(z);
        pq0 = fm_flush(a0);
        pq1 = fm_flush(a1);
        pq_mz[0] = fm_flush(z0);
        pq_mz[1] = fm_flush(z1);
    }
    if (got & (PAD_F | PAD_H)) { /* the full- and half-rate voices (rare): through the bus's own filters */
        float g = g0;
        for (i = 0; i < n; i++) {
            uint32_t t = clk + pos + i;
            float f = 0.0f;
            g += gd;
            if ((got & PAD_H) && !(t & 1u)) {
                float x = 0.0f;
                for (k = 0; k < nv; k++) /* (a steal's new note may have left the ensemble) */
                    if (vs[k]->on && vs[k]->hr && vs[k]->pt->model == M_ENS)
                        x += ens_tick(vs[k]);
                for (k = 0; k < no; k++) /* (or joined it: pad_seg's next segment) */
                    if (os[k]->on && os[k]->hr && os[k]->pt->model != M_ENS)
                        x += solo_sample(os[k]);
                pd_h[ih++] = x * g;
            }
            if (got & PAD_F)
                for (k = 0; k < nv; k++)
                    if (vs[k]->on && !vs[k]->qr && !vs[k]->hr && vs[k]->pt->model == M_ENS)
                        f += ens_tick(vs[k]);
            for (k = 0; k < no; k++)
                if (os[k]->on && !os[k]->qr && !os[k]->hr && os[k]->pt->model != M_ENS)
                    f += solo_sample(os[k]);
            pd_f[i] = f * g;
        }
    }
    for (k = 0; k < nv; k++)
        if (vs[k]->pt->model == M_ENS) {
            vs[k]->nat = fm_flush(vs[k]->nat);
            vs[k]->m.en.y1 = fm_flush(vs[k]->m.en.y1);
            vs[k]->m.en.y2 = fm_flush(vs[k]->m.en.y2);
        }
    return got;
}

/* n samples from pos of the chunk: the drums into their bus; the pitched roles summed per role, into their buses
   (filt: the section filters are on) or straight into the chunk's music and the wet inputs' pairs. The half-rate
   voices render on even samples of the engine's clock only, their sums per role into hx (written only while there are
   some, or their upsampler still rings; the rest of hx is zeroed when it is needed). */
static void render_seg(uint32_t pos, uint32_t n, int filt)
{
    uint32_t i, k3 = tk, t3 = tt, m = hn;
    int j;
    unsigned live, mf, mh, pg, pf, ph;
    uint32_t ih = 0;
    float c0 = sendp[V_CHORD], c2 = sendp[V_LEAD], c3 = sendp[V_CANDY], c4 = sendp[V_PAD]; /* (the bass is dry) */
    float g = kg0 + kgd * (float)pos, dg = kgd; /* the keys' gain, ramping over the chunk */
    float d0 = dsend[V_CHORD], d2 = dsend[V_LEAD], d3 = dsend[V_CANDY], d4 = dsend[V_PAD], e4 = csend;
    float as = ts, ac = tc, ad = td;
    int hw_on;
    int dr = drums_active(); /* (a drum starts only at a segment's start, as a voice does) */
    /* the mud high-pass: its coefficients and states in locals; run per role and rate only while it has voices there
       or its states still ring */
    int mud = !(fx_off & VX_OFF_MUD);
    unsigned mon;
    const float *qc = mud_c[V_CHORD][0], *ql = mud_c[V_LEAD][0], *qk = mud_c[V_CANDY][0], *qp = mud_c[V_PAD][0];
    const float *pc = mud_c[V_CHORD][1], *pl = mud_c[V_LEAD][1], *pk = mud_c[V_CANDY][1], *pp = mud_c[V_PAD][1];
    float zc0 = mud_z[V_CHORD][0], zc1 = mud_z[V_CHORD][1], zl0 = mud_z[V_LEAD][0], zl1 = mud_z[V_LEAD][1];
    float zk0 = mud_z[V_CANDY][0], zk1 = mud_z[V_CANDY][1], zp0 = mud_z[V_PAD][0], zp1 = mud_z[V_PAD][1];
    float yc0 = mud_zh[V_CHORD][0], yc1 = mud_zh[V_CHORD][1], yl0 = mud_zh[V_LEAD][0], yl1 = mud_zh[V_LEAD][1];
    float yk0 = mud_zh[V_CANDY][0], yk1 = mud_zh[V_CANDY][1], yp0 = mud_zh[V_PAD][0], yp1 = mud_zh[V_PAD][1];
    dr_live |= dr;
    if (dr)
        drums_render(bb[DRB] + pos, n);
    else
        memset(bb[DRB] + pos, 0, n * sizeof(float));
    pg = pad_seg(pos, n);
    pf = pg & PAD_F ? 1u << PD : 0u; /* the pad layer's full- and half-rate voices, as voice bits for mud_on */
    ph = pg & PAD_H ? 1u << PD : 0u;
    mh = seg_mask(&mf, &live);
    if (ph)
        live |= 1u << V_PAD;
    mon = mud_on(mud, mf | pf, mh | ph);
    hw_on = mh || ph || hr_tail;
    if (hw_on)
        hx_zero(m);
    for (i = pos; i < pos + n; i++) {
        float c = 0.0f, b = 0.0f, l = 0.0f, k = 0.0f, pa = 0.0f, hc = 0.0f, hl = 0.0f, hk = 0.0f, hpa = 0.0f;
        int odd = (int)((hpar + i) & 1u), moved = 0;
        unsigned msk = odd ? mf : mf | mh, mm;
        if (pg & PAD_F)
            pa = pd_f[i - pos]; /* (the full-rate voices' mud high-pass below; the quarter-rate sum's had its own) */
        if (ph && !odd)
            hpa = pd_h[ih++];
        /* (exact by construction: the same voices in the same ascending order, so the same sums, as the walk over
           all PD voices testing each bit) */
        for (mm = msk; mm; mm &= mm - 1u) { /* the voices sounding here, lowest first (one call site, so
                                                voice_sample inlines; the sums stay in registers) */
            int h;
            float y;
            j = __builtin_ctz(mm);
            h = (int)(mh >> j) & 1;
            if (!fv[j].on)
                continue;
            y = voice_sample(&fv[j]);
            moved |= fv[j].hr != h; /* a steal's new note at the other rate */
            if (h) {
                if (j < NCH)
                    hc += y;
                else if (j < NCH + NLD)
                    hl += y;
                else
                    hk += y;
            } else if (j < NCH)
                c += y;
            else if (j < NCH + NLD)
                l += y;
            else if (j < NFM)
                k += y;
            else
                b += y;
        }
        if (moved) {
            unsigned hb;
            mh = seg_mask(&mf, &hb);
            mon = mud_on(mud, mf | pf, mh | ph);
            live |= hb;
            if (mh && !hw_on) {
                hw_on = 1;
                hx_zero(m);
            }
        }
        if (mon) { /* the mud high-pass */
            if (mon & 1u)
                MUD_STEP(c, qc, zc0, zc1);
            if (mon & 2u)
                MUD_STEP(l, ql, zl0, zl1);
            if (mon & 4u)
                MUD_STEP(k, qk, zk0, zk1);
            if (mon & 8u)
                MUD_STEP(pa, qp, zp0, zp1);
            if (!odd) {
                if (mon & 16u)
                    MUD_STEP(hc, pc, yc0, yc1);
                if (mon & 32u)
                    MUD_STEP(hl, pl, yl0, yl1);
                if (mon & 64u)
                    MUD_STEP(hk, pk, yk0, yk1);
                if (mon & 128u)
                    MUD_STEP(hpa, pp, yp0, yp1);
            }
        }
        if (!odd) {
            if (hw_on) {
                hx[V_CHORD][HB_H + m] = hc;
                hx[V_LEAD][HB_H + m] = hl;
                hx[V_CANDY][HB_H + m] = hk;
                hx[V_PAD][HB_H + m] = hpa;
            }
            m++;
        }
        if (filt) {
            bb[V_CHORD][i] = c;
            bb[V_BASS][i] = b;
            bb[V_LEAD][i] = l;
            bb[V_CANDY][i] = k;
            bb[V_PAD][i] = pa;
        } else {
            if (pg & PAD_Q)
                pa += pq_buf[i]; /* the quarter-rate sum (no section filters here: they are open) */
            g += dg;
            c *= g;
            ch_m[i] = c + b + l + k + pa;
            as += c * c0 + l * c2 + k * c3 + pa * c4;
            ac += c + pa * e4;
            ad += c * d0 + l * d2 + k * d3 + pa * d4;
            if (++k3 == FX_PAIR) {
                t_s[t3] += as;
                t_c[t3] += ac;
                t_d[t3] += ad;
                t3++;
                k3 = 0;
                as = ac = ad = 0.0f;
            }
        }
    }
    mud_z[V_CHORD][0] = fm_flush(zc0);
    mud_z[V_CHORD][1] = fm_flush(zc1);
    mud_z[V_LEAD][0] = fm_flush(zl0);
    mud_z[V_LEAD][1] = fm_flush(zl1);
    mud_z[V_CANDY][0] = fm_flush(zk0);
    mud_z[V_CANDY][1] = fm_flush(zk1);
    mud_z[V_PAD][0] = fm_flush(zp0);
    mud_z[V_PAD][1] = fm_flush(zp1);
    mud_zh[V_CHORD][0] = fm_flush(yc0);
    mud_zh[V_CHORD][1] = fm_flush(yc1);
    mud_zh[V_LEAD][0] = fm_flush(yl0);
    mud_zh[V_LEAD][1] = fm_flush(yl1);
    mud_zh[V_CANDY][0] = fm_flush(yk0);
    mud_zh[V_CANDY][1] = fm_flush(yk1);
    mud_zh[V_PAD][0] = fm_flush(yp0);
    mud_zh[V_PAD][1] = fm_flush(yp1);
    tk = k3;
    tt = t3;
    ts = as;
    tc = ac;
    td = ad;
    hn = m;
    if (hw_on)
        hw = m;
    hr_live |= live;
}

/* the half-rate roles of the chunk (c samples) to the full rate, into their buses (filt) or straight into the music
   and the wet inputs' pairs */
static void hx_mix(uint32_t c, int filt)
{
    int r;
    for (r = V_CHORD; r <= V_PAD; r++) {
        uint32_t i, k3 = ph3, t3 = 0;
        float sum = 0.0f, gl, dgl, sp, sc, sd;
        const float *h;
        if (r == V_BASS || !((hr_live | hr_tail) & (1u << r)))
            continue;
        hx_zero(hn);
        if (r == V_CHORD && role_pt[V_CHORD] && role_pt[V_CHORD]->model == M_TW && !(fx_off & VX_OFF_CHORUS))
            tw_scanner(&tw_sc[0], 2u, TW_SC_KEYS, hx[r] + HB_H, hn, NULL, 1.0f); /* the organ's scanner */
        hx_up(hx[r], hn, c);
        h = hx[r];
        hr_tail &= ~(1u << r);
        for (i = 0; i < HB_H; i++)
            if (h[i] != 0.0f)
                hr_tail |= 1u << r;
        if (filt) {
            for (i = 0; i < c; i++)
                bb[r][i] += hup[i];
            continue;
        }
        gl = r == V_CHORD ? kg0 : 1.0f; /* the keys' gain (the chorus's dry scale, the duck), ramping */
        dgl = r == V_CHORD ? kgd : 0.0f;
        sp = sendp[r];
        sc = r == V_CHORD ? 1.0f : r == V_PAD ? csend : 0.0f; /* the chorus's input: the keys, the pad's send */
        sd = dsend[r];
        for (i = 0; i < c; i++) {
            float x = hup[i] * (gl += dgl);
            ch_m[i] += x;
            sum += x;
            if (++k3 == FX_PAIR) {
                t_s[t3] += sum * sp;
                t_c[t3] += sum * sc;
                t_d[t3] += sum * sd;
                t3++;
                k3 = 0;
                sum = 0.0f;
            }
        }
        if (k3) {
            t_s[t3] += sum * sp;
            t_c[t3] += sum * sc;
            t_d[t3] += sum * sd;
        }
    }
}

/* one block of a bus through its section filters, in place: the amounts ramp a step, the one-poles' coefficients
   follow (a block's coefficient held; the high-pass's mix reached linearly) */
static void bus_amounts(bus_t *f)
{
    if (f->a != f->at)
        f->a = f->a < f->at ? fm_minf(f->a + f->as, f->at) : fm_maxf(f->a - f->as, f->at);
    if (f->h != f->ht)
        f->h = f->h < f->ht ? fm_minf(f->h + f->hs, f->ht) : fm_maxf(f->h - f->hs, f->ht);
}

static void bus_filter(bus_t *f, float *x, uint32_t n)
{
    uint32_t i;
    float a0 = f->a, h0 = f->h + f->hm;
    bus_amounts(f);
    if (f->h > 0.0f || f->hm > 0.0f) { /* y = x - mix * one-pole(x): a high-pass at full mix */
        float k, m = f->hm, m1 = fm_minf(8.0f * f->h, 1.0f), dm = (m1 - m) / (float)n;
        float z = h0 > 0.0f ? f->z : x[0]; /* (a move starts from the signal) */
        if (f->h != f->hc) {
            f->hc = f->h;
            f->kh = k_lp1(HP_BOT * fm_exp2f(HP_OCT * f->h));
        }
        k = f->kh;
        for (i = 0; i < n; i++) {
            z += k * (x[i] - z);
            m += dm;
            x[i] -= m * z;
        }
        f->hm = m1;
        f->z = fm_flush(z);
    } else
        f->z = 0.0f;
    if (f->a > 0.0f) {
        float k, y = a0 > 0.0f ? f->y : x[0];
        if (f->a != f->ac) {
            f->ac = f->a;
            f->ka = k_lp1(LP_TOP * fm_exp2f(-LP_OCT * f->a));
        }
        k = f->ka;
        for (i = 0; i < n; i++) {
            y = fm_flush(y + k * (x[i] - y)); /* (fast at the top: a bus falling silent would reach denormals) */
            x[i] = y;
        }
        f->y = y;
    }
}

/* the buses with something to filter in this chunk (a bit per bus): a voice of the role (or a drum) sounding, or its
   filters' state still ringing; the others are silent (all zeros) and only their amounts move */
static unsigned live_buses(void)
{
    unsigned m = drums_active() ? 1u << DRB : 0u;
    int r, k;
    for (r = V_CHORD; r <= V_PAD; r++)
        for (k = ROLES[r].first; k < ROLES[r].first + ROLES[r].count; k++)
            if (fv[k].on && !fv[k].qr) /* (the pad layer's quarter-rate voices: filtered in pad_seg) */
                m |= 1u << r;
    for (r = 0; r < NBUS; r++)
        if (fm_fabsf(bus[r].y) > 1e-7f || fm_fabsf(bus[r].z) > 1e-7f)
            m |= 1u << r;
    return m | hr_tail; /* (a half-rate role's upsampler still ringing out) */
}

/* the drums' compressor's gain over one block: the block's peak (a pre-scan: the attack is instant at the block's
   start) into a level releasing over 120 ms; the gain from it (4:1 above the kit's threshold, at most 6 dB,
   make-up), stepped down at once, released linearly over the block: *g0 at its start, *dg added each sample (applied
   in the mix) */
static void comp(float *x, uint32_t n, float *g0, float *dg)
{
    uint32_t i;
    float pk = 0.0f, t;
    for (i = 0; i < n; i++)
        pk = fm_maxf(pk, fm_fabsf(x[i]));
    cp_env = fm_flush(fm_maxf(pk, cp_env * (1.0f - CP_REL * (float)n)));
    t = cp_env > cp_thr ? fm_maxf(fm_exp2f(-CP_SLOPE * fm_log2f(cp_env / cp_thr)), CP_MIN) : 1.0f;
    t *= CP_MAKEUP;
    *g0 = t < cp_g ? t : cp_g;
    *dg = (t - *g0) / (float)n;
    cp_g = t;
}

/* vx_mute over one block (n samples at o in a chunk of c): a muted bus zeroed, one going out or coming back ramped
   linearly over the chunk (the pad's quarter-rate sum with its bus) */
static void mute_block(uint32_t c, uint32_t o, uint32_t n)
{
    int r;
    uint32_t i;
    for (r = 0; r < NBUS; r++) {
        float g0 = (mute_was >> r) & 1u ? 0.0f : 1.0f, g1 = (mute_mask >> r) & 1u ? 0.0f : 1.0f, dg = (g1 - g0) / (float)c;
        float *x[2] = {bb[r] + o, r == V_PAD && pq_used ? pq_buf + o : NULL};
        int k;
        if (g0 == 1.0f && g1 == 1.0f)
            continue;
        for (k = 0; k < 2 && x[k]; k++)
            if (g0 == g1)
                memset(x[k], 0, n * sizeof(float));
            else
                for (i = 0; i < n; i++)
                    x[k][i] *= g0 + dg * (float)(o + i + 1u);
    }
}

/* the chunk's mix (c samples): per block, the drums compressed, the buses filtered (filt) and summed, into the music
   and the wet inputs' pairs; the song gain on everything */
static const float pq_zero[CHUNK];
static void mix_chunk(uint32_t c, int filt, unsigned live)
{
    uint32_t o, i, k3 = ph3, t3 = 0;
    float as = 0.0f, ac = 0.0f, ad = 0.0f;
    /* the drums' bus silent and the compressor at rest (its gain the make-up, its level under the threshold): the
       compressor's scan and gain, and the drums' adds, are skipped (they would add zeros); its level still falls */
    int dz = !dr_live && cp_env <= cp_thr && cp_g == CP_MAKEUP;
    for (o = 0; o < c; o += BCR) {
        uint32_t n = c - o < BCR ? c - o : BCR;
        float *d = bb[DRB] + o, g = 1.0f, dg = 0.0f;
        if (dz) {
            if (!(fx_off & VX_OFF_COMP))
                cp_env = fm_flush(cp_env * (1.0f - CP_REL * (float)n));
            if (!filt)
                continue;
        } else if (!(fx_off & VX_OFF_COMP))
            comp(d, n, &g, &dg);
        if (filt) {
            float s0 = sendp[V_CHORD], s2 = sendp[V_LEAD], s3 = sendp[V_CANDY], s4 = sendp[V_PAD];
            float cgl = kg0 + kgd * (float)o, cs = csend;
            float e0 = dsend[V_CHORD], e2 = dsend[V_LEAD], e3 = dsend[V_CANDY], e4 = dsend[V_PAD];
            const float *b0 = bb[V_CHORD] + o, *b1 = bb[V_BASS] + o, *b2 = bb[V_LEAD] + o, *b3 = bb[V_CANDY] + o;
            const float *b4 = bb[V_PAD] + o;
            /* the pad's quarter-rate sum, filtered already (pad_seg), beside its bus's full-rate voices */
            const float *q4 = pq_used && (solo_bus < 0 || solo_bus == V_PAD) ? pq_buf + o : pq_zero;
            int r;
            for (i = 0; !dz && i < n; i++) { /* the compressor before the drums' filters */
                g += dg;
                d[i] *= g;
            }
            for (r = 0; r < NBUS; r++)
                if (live & (1u << r))
                    bus_filter(&bus[r], bb[r] + o, n);
                else { /* silent: the amounts move, the states rest */
                    bus_amounts(&bus[r]);
                    bus[r].hm = fm_minf(8.0f * bus[r].h, 1.0f);
                    bus[r].y = bus[r].z = 0.0f;
                }
            if (solo_bus >= 0)
                for (r = 0; r < NBUS; r++)
                    if (r != solo_bus)
                        memset(bb[r] + o, 0, n * sizeof(float));
            if (mute_mask | mute_was)
                mute_block(c, o, n);
            for (i = 0; i < n; i++) {
                float x = b0[i] * (cgl += kgd);
                float p4 = b4[i] + q4[i];
                ch_m[o + i] = x + b1[i] + b2[i] + b3[i] + p4 + d[i];
                as += x * s0 + b2[i] * s2 + b3[i] * s3 + p4 * s4 + d[i] * SEND_DRUMS;
                ac += x + p4 * cs;
                ad += x * e0 + b2[i] * e2 + b3[i] * e3 + p4 * e4;
                if (++k3 == FX_PAIR) {
                    t_s[t3] += as;
                    t_c[t3] += ac;
                    t_d[t3] += ad;
                    t3++;
                    k3 = 0;
                    as = ac = ad = 0.0f;
                }
            }
        } else
            for (i = 0; i < n; i++) { /* the drums into the music and the plate's send */
                float x;
                g += dg;
                x = d[i] * g;
                ch_m[o + i] += x;
                as += x;
                if (++k3 == FX_PAIR) {
                    t_s[t3] += as * SEND_DRUMS;
                    t3++;
                    k3 = 0;
                    as = 0.0f;
                }
            }
    }
    if (k3) { /* the open pair */
        t_s[t3] += filt ? as : as * SEND_DRUMS;
        t_c[t3] += ac;
        t_d[t3] += ad;
    }
    if (gain_n || gain != 1.0f)
        for (i = 0, k3 = ph3, t3 = 0; i < c; i++) { /* (each pair at its last sample's gain) */
            if (gain_n) {
                gain += gain_step;
                if (--gain_n == 0)
                    gain = gain_tgt;
            }
            ch_m[i] *= gain;
            if (++k3 == FX_PAIR || i == c - 1u) {
                t_s[t3] *= gain;
                t_c[t3] *= gain;
                t_d[t3] *= gain;
                t3++;
                k3 = 0;
            }
        }
}

/* any bus's filter not open and settled (the filtered mix path) */
static int filtering(void)
{
    int r;
    if (solo_bus >= 0 || mute_mask || mute_was)
        return 1; /* (vx_solo, vx_mute: the buses kept apart, the others zeroed in the mix) */
    if (fx_off & VX_OFF_FILTERS)
        return 0;
    for (r = 0; r < NBUS; r++)
        if (bus[r].a > 0.0f || bus[r].at > 0.0f || bus[r].h > 0.0f || bus[r].ht > 0.0f || bus[r].hm > 0.0f)
            return 1;
    return 0;
}

/* the duck's follower a chunk (c samples) on, from the lead's gates now; the keys' gain's ramp over the chunk */
static void duck_chunk(uint32_t c)
{
    int k, held = 0;
    float t;
    for (k = NCH; k < NCH + NLD; k++)
        held |= fv[k].on && fv[k].held && fv[k].amp_t > 0.0f; /* (a velocity-0 note is silent) */
    if (held)
        dk_env += (1.0f - dk_env) * DUCK_KA;
    else if (dk_env > 0.0f)
        dk_env = dk_env < 1e-5f ? 0.0f : dk_env * (1.0f - DUCK_KR);
    t = cg * (1.0f - DUCK_DEPTH * dk_env);
    kg0 = kg;
    kgd = (t - kg) / (float)c;
    kg = t;
}

/* The mix, in chunks: the voices and drums (event starts sample-accurate) into the chunk's buses, the mix, then the
   lo-fi chain (fx.c: chorus and delay, dust, ducking by the kick's envelope at the chunk's end, the plate, the clip,
   the bed). */
void vx_render(float *l, float *r, uint32_t n)
{
    uint32_t done = 0, c, k;
    while (done < n) {
        uint32_t pos = 0;
        int filt = filtering();
        unsigned live = filt ? live_buses() : 0u;
        c = n - done < CHUNK ? n - done : CHUNK;
        ph3 = tk = fx_phase();
        tt = 0;
        hn = hw = 0;
        hr_live = 0u;
        dr_live = 0;
        pq_used = 0;
        pd_filt = filt;
        ts = tc = td = 0.0f;
        memset(t_s, 0, sizeof t_s);
        memset(t_c, 0, sizeof t_c);
        memset(t_d, 0, sizeof t_d);
        {
            float cd = fx_chorus_dry();
            cg += (cd - cg) * 0.25f; /* the chorus's dry scale, eased over a few chunks */
            cg = fm_fabsf(cg - cd) < 1e-6f ? cd : cg;
            /* the pad layer's, likewise, times its level and its share of the keys' duck under the lead (it stays
               under them there: Task 1b), ramped over the chunk */
            cd = fx_chorus_dry_at(pd_dep) * pd_lvl * (1.0f - DUCK_PAD * dk_env);
            pdg0 = pdg;
            pdg += (cd - pdg) * 0.25f;
            pdg = fm_fabsf(pdg - cd) < 1e-6f ? cd : pdg;
            pdgd = (pdg - pdg0) / (float)c;
        }
        duck_chunk(c);
        while (pos < c) {
            int i, w = 0;
            uint32_t seg = c - pos;
            t_on = clk + pos;
            for (i = 0; i < qn; i++) { /* start what is due, keeping the order of the rest */
                if (q[i].rem == 0)
                    fire(&q[i].e);
                else
                    q[w++] = q[i];
            }
            qn = w;
            for (i = 0; i < qn; i++)
                if (q[i].rem < seg)
                    seg = q[i].rem;
            render_seg(pos, seg, filt);
            for (i = 0; i < qn; i++)
                q[i].rem -= seg;
            pos += seg;
        }
        if (tk) { /* render_seg's open pair */
            t_s[tt] += ts;
            t_c[tt] += tc;
            t_d[tt] += td;
        }
        hx_mix(c, filt);
        hpar ^= c & 1u;
        clk += c;
        if (filt)
            live |= live_buses(); /* (a voice that started in the chunk) */
        mix_chunk(c, filt, live);
        mute_was = mute_mask;
        fx_process_pairs(ch_m, ch_s, t_s, t_c, t_d, c, drums_kick_env());
        for (k = 0; k < c; k++) {
            l[done + k] += ch_m[k];
            r[done + k] += ch_s[k];
        }
        done += c;
    }
}

void vx_release_all(void)
{
    int i;
    for (i = 0; i < NV; i++) { /* a long release (a pad's) shortened to 40 ms */
        fv[i].held = 0;
        fv[i].pend = 0;
        fv[i].kr = fm_maxf(fv[i].kr, KR_STOP * step_len(&fv[i]));
    }
    drums_release();
    qn = 0;
    fx_close(); /* the bed fades out; the plate rings on */
}

void vx_stop(void)
{
    vx_release_all();
    fx_stop();
}

void vx_set_gain(float g)
{
    gain_tgt = g < 0.0f ? 0.0f : g > 1.0f ? 1.0f : g;
    gain_step = (gain_tgt - gain) * (1.0f / 882.0f);
    gain_n = 882;
}

void vx_set_pad_level(float g) { pd_lvl = g < 0.0f ? 0.0f : g > 1.0f ? 1.0f : g; }

int vx_active(void)
{
    int i, c = 0;
    for (i = 0; i < NV; i++)
        c += fv[i].on;
    return c + drums_active();
}

int vx_sounding(void)
{
    return vx_active() + fx_active();
}

void vx_set_kit(int kit)
{
    cfg.kit = (uint8_t)kit;
    cp_thr = CP_THR_KIT[kit >= 0 && kit < KIT_N ? kit : 0];
    drums_set_kit(kit);
}

float vx_kick_env(void)
{
    return drums_kick_env();
}

/* a bus's index for a role (any drum voice: the drums' bus), -1 for none */
static int bus_of(int role)
{
    return role >= V_CHORD && role <= V_PAD ? role : role >= V_KICK && role < V_N ? DRB : -1;
}

void vx_set_filter(int role, float amount)
{
    int k = bus_of(role);
    float t = fm_clampf(amount, 0.0f, 1.0f);
    if (k < 0 || t == bus[k].at)
        return;
    bus[k].at = t;
    bus[k].as = fm_fabsf(t - bus[k].a) * FLT_RAMP;
    if (!vx_active()) /* a move nobody hears */
        bus[k].a = t;
}

void vx_set_hp(int role, float amount)
{
    int k = bus_of(role);
    float t = fm_clampf(amount, 0.0f, 1.0f);
    if (k < 0 || t == bus[k].ht)
        return;
    bus[k].ht = t;
    bus[k].hs = fm_fabsf(t - bus[k].h) * (t < bus[k].h ? HP_OPEN : FLT_RAMP);
    if (!vx_active())
        bus[k].h = t;
}

void vx_set_pump(int pump)
{
    pump = pump < 0 ? 0 : pump >= VX_PUMP_N ? VX_PUMP_N - 1 : pump;
    cfg.pump = (uint8_t)pump;
    fx_pump(PUMP_DB[pump]);
}

void vx_fx_off(unsigned mask)
{
    fx_off = mask;
    skits_half(!(mask & VX_OFF_HALF));
    fx_wet_off((mask & VX_OFF_DELAY) != 0);
    fx_glue_off((mask & VX_OFF_GLUE) != 0);
    vx_assign(V_CHORD, cfg.chords); /* the sends and the chorus's depth again */
    vx_assign(V_BASS, cfg.bass);
    vx_assign(V_LEAD, cfg.lead);
    vx_assign(V_CANDY, cfg.candy);
    vx_assign(V_PAD, cfg.pad);
}

void vx_solo(int role)
{
    solo_bus = role < 0 ? -1 : bus_of(role);
}

void vx_mute(unsigned mask)
{
    unsigned back = mute_was & ~mask; /* buses silent now, back in the mix */
    int j;
    for (j = 0; j < NV; j++) /* their notes struck while muted end here, unheard: only new ones play */
        if (fv[j].on && (back & role_bit(j))) {
            fv[j].on = fv[j].held = fv[j].pend = 0;
            fv[j].g = 0.0f;
        }
    for (j = 0; j < NBUS; j++) /* and what they left in its filters and upsampler (it is silent: nothing jumps) */
        if (back & (1u << j)) {
            bus[j].y = bus[j].z = 0.0f;
            if (j <= V_PAD) {
                memset(mud_z[j], 0, sizeof mud_z[j]);
                memset(mud_zh[j], 0, sizeof mud_zh[j]);
                memset(hx[j], 0, sizeof hx[j]);
                hr_tail &= ~(1u << j);
            }
            if (j == V_PAD)
                pq0 = pq1 = pq_mz[0] = pq_mz[1] = pb_y = pb_z = 0.0f;
        }
    if (back & (1u << DRB))
        drums_release();
    mute_mask = mask & ((1u << NBUS) - 1u) & ~(1u << V_CANDY);
}

float vx_chip_out(int role, float *raw, float *step)
{
    int j;
    for (j = 0; j < NV; j++)
        if (fv[j].on && fv[j].pt && fv[j].pt->model == M_CHIP && role_bit(j) == 1u << role) {
            *raw = fv[j].m.ch.raw;
            *step = fv[j].m.ch.qi;
            return fv[j].m.ch.out;
        }
    *raw = *step = 0.0f;
    return 0.0f;
}

float vx_pad_lane(int lane, int *step, int *held)
{
    const voice_t *v = lane >= 1 && lane <= NPD ? &fv[PD + lane - 1] : NULL;
    if (!v || !v->on) {
        *step = *held = 0;
        return 0.0f;
    }
    *step = (int)step_len(v);
    *held = v->held;
    switch (v->pt->model) { /* (the solo lane's own voice: its model's pitch) */
    case M_WIND: return v->m.wd.f0 / step_len(v) * SR;
    case M_TW: return note_inc(v->m.tw.nt) * SR;
    default: return v->m.en.i1 / step_len(v) * SR;
    }
}

void vx_assign_pad_solo(int ins)
{
    int ok = ins == INS_FLUTE || ins == INS_ORGAN || ins == INS_STRINGS || ins == INS_CHOIR;
    solo_pt = !ok ? NULL : ins == INS_ORGAN ? vx_organ_pad() : vx_patch(ins);
    solo_trim = ok ? SOLO_TRIM[ins] : 1.0f;
}

float vx_choir_formant(int k)
{
    return k >= 0 && k < 3 && cho_cell != 0xFFFFFFFFu ? cho_f[k] : 0.0f;
}

const vx_patch_t *vx_pad_lane_patch(int lane)
{
    const voice_t *v = lane >= 1 && lane <= NPD ? &fv[PD + lane - 1] : NULL;
    return v && v->on ? v->pt : NULL;
}

uint32_t vx_steals(int role)
{
    return role == V_CHORD ? n_steal : 0u;
}

float vx_duck_db(void)
{
    return 6.0206f * fm_log2f(1.0f - DUCK_DEPTH * dk_env);
}

float vx_comp_db(void)
{
    return -6.0206f * fm_log2f(cp_g * (1.0f / CP_MAKEUP));
}

static int vx_tiny_of(const float *x, int n)
{
    int i, c = 0;
    for (i = 0; i < n; i++)
        c += x[i] != 0.0f && fm_fabsf(x[i]) < 1e-20f;
    return c;
}

int vx_tiny(void)
{
    int r, c = vx_tiny_of(&mud_z[0][0], (int)(sizeof mud_z / sizeof(float))) +
               vx_tiny_of(&mud_zh[0][0], (int)(sizeof mud_zh / sizeof(float))) + vx_tiny_of(&cp_env, 1) +
               vx_tiny_of(&cp_g, 1) + vx_tiny_of(&dk_env, 1) + skits_tiny();
    for (r = 0; r < NBUS; r++)
        c += vx_tiny_of(&bus[r].y, 1) + vx_tiny_of(&bus[r].z, 1) + vx_tiny_of(&bus[r].hm, 1);
    c += vx_tiny_of(pq_mz, 2) + vx_tiny_of(&pq0, 1) + vx_tiny_of(&pq1, 1) + vx_tiny_of(&pb_y, 1) + vx_tiny_of(&pb_z, 1);
    c += vx_tiny_of(&cho_s[0][0], 6) + vx_tiny_of(&cho_pi, 1) + vx_tiny_of(&cho_po, 1);
    for (r = 0; r < NV; r++) /* an ensemble voice's low-pass; a sampler's tone and envelope; a vibraphone's modes */
        if (fv[r].on && fv[r].pt && fv[r].pt->model == M_ENS)
            c += vx_tiny_of(&fv[r].m.en.y1, 2) + vx_tiny_of(fv[r].m.en.w, 2);
        else if (fv[r].on && fv[r].pt && fv[r].pt->model == M_SAMP)
            c += vx_tiny_of(&fv[r].m.sp.lp, 1) + vx_tiny_of(&fv[r].m.sp.env, 1);
        else if (fv[r].on && fv[r].pt && fv[r].pt->model == M_MODAL)
            c += vx_tiny_of(fv[r].m.md.s1, MD_N) + vx_tiny_of(fv[r].m.md.s2, MD_N) + vx_tiny_of(&fv[r].m.md.bl, 1);
    return c;
}

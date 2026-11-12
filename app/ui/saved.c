/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The saved songs (saved.h): the records, and the slots in the flash (one store object, written only while all is
   silent). Main loop only. */
#include <string.h>
#include "app.h"
#include "saved.h"
#include "choices_gen.h"
#include "screens_gen.h"
#include "library.h"
#include "override.h"
#include "song.h"
#include "groove.h"

const int8_t SV_ROWS[SV_NROWS] = {
    ROW_KEYS, ROW_COMP, ROW_LEAD, ROW_BASS, ROW_PAD, ROW_PAD_ON, ROW_PHRASE, ROW_LEAD_STYLE, ROW_LEAD_LOOP,
    ROW_LEAD_OCT, ROW_BUSY_THIN, ROW_KEYS_WITH, ROW_VOICINGS, ROW_KEYS_OCT, ROW_STRUM_PAT, ROW_STRUM_FILL,
    ROW_PAD_PLACE, ROW_BASS_LINE, ROW_PAD_NOTES, ROW_DRUMS_VAR, ROW_KIT, ROW_KICK, ROW_RIDE,
};

static ui_slot_t slots[UI_SLOTS];
static uint8_t pend_rec[SV_BYTES];      /* the save waiting for silence (one), its slot (-1 none) */
static int8_t pend_slot = -1;
static uint16_t pend_del;               /* the slots whose delete waits for silence (bit k slot k) */
static uint8_t failed;                  /* the last write failed: tried again at the next change */

/* ------------------------------------------------------------------------------------------- the record --- */
int sv_capture(const snap_t *s, const ui_nav_t *n, sv_song_t *out)
{
    int16_t rows[ROW_N];
    int f, i;
    if (!(s->has_song || s->held || s->ready)) return -1;
    memcpy(rows, s->rows, sizeof rows);
    out->roll = s->roll;
    if (out->roll.v[RF_SET] == LIB_SET_WB && s->set_base) out->roll.v[RF_SET] = s->set_base;
    for (f = 0; n && f < UI_N_FIELDS; f++) {             /* a change waiting: as if landed */
        int row = UI_FIELDS[f].row, sr, v = n->req_val[f];
        if (n->req_at[f] < 0) continue;
        if (row == ROW_RHYTHM) row = ch_rhythm_row(rows[ROW_KEYS], rows[ROW_COMP]);
        if (row < 0 || row >= ROW_N) continue;
        sr = CH_SONG_ROW[row];
        if (sr >= SR_ROLL && sr < SR_ROLL + RF_N) {    /* (song.c's song_request: the length stays, the motion's move
                                                          kept apart) */
            int bars = out->roll.v[RF_BARS], was = out->roll.v[RF_MOTION];
            roll_set(&out->roll, sr - SR_ROLL, v);
            out->roll.v[RF_BARS] = (int16_t)bars;
            if (sr - SR_ROLL == RF_MOTION)
                out->roll.motion_moved = (int16_t)(out->roll.motion_moved + out->roll.v[RF_MOTION] - was);
        } else
            rows[row] = (int16_t)v;
    }
    for (i = 0; i < SV_NROWS; i++)
        out->rows[i] = rows[SV_ROWS[i]];
    return 0;
}

int sv_rolled(const sv_song_t *sg, int i, int kit_rolled)
{
    const song_roll_t *r = &sg->roll;
    const lib_set_t *set = lib_set_by_id((uint8_t)r->v[RF_SET]);
    int row = SV_ROWS[i], sr = CH_SONG_ROW[row];
    if (row == ROW_KIT) return kit_rolled;
    if (row == ROW_KICK) return lib_song_kick(r) + 1;   /* (the row shows the phrase the song plays) */
    if (row == ROW_COMP) {                               /* (a comp rhythm: 100 + its index) */
        int pat = song_ov_get(song_ov_none(), OV_COMP_PAT);
        return pat >= 0 ? 100 + pat : song_ov_get(song_ov_none(), OV_COMP);
    }
    if (sr >= SR_OV && sr < SR_OV + OV_N) return song_ov_get(song_ov_none(), sr - SR_OV);
    if (!set) return -32768;
    switch (sr - SR_ROLE) {
    case V_CHORD: return set->chords;
    case V_BASS: return set->bass;
    case V_LEAD: return set->lead;
    case V_PAD: return lib_song_pad(r);
    default: return -32768;
    }
}

static void put16(uint8_t *b, int v) { b[0] = (uint8_t)(v & 0xFF); b[1] = (uint8_t)((v >> 8) & 0xFF); }
static int get16(const uint8_t *b) { return (int16_t)(b[0] | b[1] << 8); }

void sv_pack(const sv_song_t *sg, uint8_t *b)
{
    int i;
    b[0] = SV_VERSION;
    b[1] = SV_NROWS;
    for (i = 0; i < 4; i++) b[2 + i] = (uint8_t)(sg->roll.seed >> (8 * i));
    for (i = 0; i < RF_N; i++) put16(b + 6 + 2 * i, sg->roll.v[i]);
    put16(b + 6 + 2 * RF_N, sg->roll.motion_moved);
    for (i = 0; i < SV_NROWS; i++) put16(b + 8 + 2 * RF_N + 2 * i, sg->rows[i]);
}

int sv_unpack(const uint8_t *b, sv_song_t *sg)
{
    int i;
    if (b[0] != SV_VERSION || b[1] != SV_NROWS) return -1;
    memset(sg, 0, sizeof *sg);
    for (i = 0; i < 4; i++) sg->roll.seed |= (uint32_t)b[2 + i] << (8 * i);
    for (i = 0; i < RF_N; i++) sg->roll.v[i] = (int16_t)get16(b + 6 + 2 * i);
    sg->roll.motion_moved = (int16_t)get16(b + 6 + 2 * RF_N);
    for (i = 0; i < SV_NROWS; i++) sg->rows[i] = (int16_t)get16(b + 8 + 2 * RF_N + 2 * i);
    return 0;
}

/* ------------------------------------------------------------------------------------------- the slots --- */
static uint16_t hash(const uint8_t *b)
{
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < SV_BYTES; i++) h = (h ^ b[i]) * 16777619u;
    return (uint16_t)(h ^ h >> 16);
}

static void summarize(int k, const uint8_t *b, int state)
{
    sv_song_t sg;
    ui_slot_t *x = &slots[k];
    memset(x, 0, sizeof *x);
    if (sv_unpack(b, &sg)) return;
    x->state = (uint8_t)state;
    x->map = (uint8_t)sg.roll.v[RF_MAP];
    x->key = (uint8_t)sg.roll.v[RF_KEY];
    x->mode = (uint8_t)sg.roll.v[RF_MODE];
    x->tempo = (uint8_t)sg.roll.v[RF_TEMPO];
    x->hash = hash(b);
}

/* the object from the flash into o (SV_OBJ_BYTES): a missing or foreign one reads as every slot empty */
static void read_all(uint8_t *o)
{
    if (plat_store_load(OBJ_SONGS, o, SV_OBJ_BYTES) != (int)SV_OBJ_BYTES ||
        (uint32_t)(o[0] | o[1] << 8 | o[2] << 16 | (uint32_t)o[3] << 24) != SV_MAGIC) {
        memset(o, 0, SV_OBJ_BYTES);
        o[0] = (uint8_t)SV_MAGIC;
        o[1] = (uint8_t)(SV_MAGIC >> 8);
        o[2] = (uint8_t)(SV_MAGIC >> 16);
        o[3] = (uint8_t)(SV_MAGIC >> 24);
    }
}

void sv_boot(void)
{
    uint8_t o[SV_OBJ_BYTES];             /* (the main loop's stack, for the call: no RAM kept) */
    int k;
    read_all(o);
    pend_slot = -1;
    pend_del = 0;
    failed = 0;
    for (k = 0; k < UI_SLOTS; k++)
        summarize(k, o + SV_HEAD + k * SV_BYTES, UI_SLOT_KEPT);
}

const ui_slot_t *sv_slots(void) { return slots; }
int sv_waiting(void) { return pend_slot >= 0 || pend_del; }

int sv_get_bytes(int slot, uint8_t *b)
{
    uint8_t o[SV_OBJ_BYTES];
    memset(b, 0, SV_BYTES);
    if (slot < 0 || slot >= UI_SLOTS || slots[slot].state == UI_SLOT_EMPTY) return UI_SAVE_EMPTY;
    if (slot == pend_slot) {
        memcpy(b, pend_rec, SV_BYTES);
        return 0;
    }
    read_all(o);
    memcpy(b, o + SV_HEAD + slot * SV_BYTES, SV_BYTES);
    return b[0] == SV_VERSION ? 0 : UI_SAVE_EMPTY;
}

int sv_load(int slot, sv_song_t *out)
{
    uint8_t b[SV_BYTES];
    if (sv_get_bytes(slot, b) || sv_unpack(b, out)) return UI_SAVE_EMPTY;
    return 0;
}

int sv_flush(int silent)
{
    uint8_t o[SV_OBJ_BYTES];
    int k;
    if (!silent || failed || !sv_waiting()) return 0;
    read_all(o);
    for (k = 0; k < UI_SLOTS; k++)
        if (pend_del >> k & 1u) memset(o + SV_HEAD + k * SV_BYTES, 0, SV_BYTES);
    if (pend_slot >= 0) memcpy(o + SV_HEAD + pend_slot * SV_BYTES, pend_rec, SV_BYTES);
    if (plat_store_save(OBJ_SONGS, o, SV_OBJ_BYTES)) {
        failed = 1;
        return -1;
    }
    if (pend_slot >= 0) slots[pend_slot].state = UI_SLOT_KEPT;
    pend_slot = -1;
    pend_del = 0;
    return 1;
}

int sv_put_bytes(int slot, const uint8_t *b, int silent)
{
    sv_song_t sg;
    uint16_t h;
    int k;
    if (b[0] == 0) return sv_erase(slot, silent) == UI_SAVE_BUSY ? UI_SAVE_BUSY : slot;
    if (sv_unpack(b, &sg)) return -5;
    h = hash(b);
    if (slot < 0) {                                      /* the same song saved already: its slot */
        for (k = 0; k < UI_SLOTS; k++)
            if (slots[k].state != UI_SLOT_EMPTY && slots[k].hash == h) {
                uint8_t c[SV_BYTES];
                if (!sv_get_bytes(k, c) && !memcmp(c, b, SV_BYTES)) return k;
            }
        for (k = 0; k < UI_SLOTS && slots[k].state != UI_SLOT_EMPTY; k++) {}
        if (k == UI_SLOTS) return UI_SAVE_FULL;
        slot = k;
    }
    if (slot >= UI_SLOTS) return UI_SAVE_FULL;
    if (pend_slot >= 0 && pend_slot != slot && !silent) return UI_SAVE_BUSY;
    if (pend_slot >= 0 && pend_slot != slot) {          /* (silent: the one waiting goes first) */
        failed = 0;
        if (sv_flush(1) < 0) return UI_SAVE_BUSY;
    }
    memcpy(pend_rec, b, SV_BYTES);
    pend_slot = (int8_t)slot;
    pend_del &= (uint16_t)~(1u << slot);
    failed = 0;
    summarize(slot, b, UI_SLOT_WAITING);
    sv_flush(silent);
    return slot;
}

int sv_put(int slot, const sv_song_t *sg, int silent)
{
    uint8_t b[SV_BYTES];
    sv_pack(sg, b);
    return sv_put_bytes(slot, b, silent);
}

int sv_erase(int slot, int silent)
{
    if (slot < 0 || slot >= UI_SLOTS || slots[slot].state == UI_SLOT_EMPTY) return UI_SAVE_EMPTY;
    if (slot == pend_slot && slots[slot].state == UI_SLOT_WAITING) {
        pend_slot = -1;                                  /* (never written: was it in the flash before? deleted too) */
    }
    pend_del |= (uint16_t)(1u << slot);
    memset(&slots[slot], 0, sizeof slots[slot]);
    failed = 0;
    sv_flush(silent);
    return 0;
}

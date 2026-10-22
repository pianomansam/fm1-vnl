/* SPDX-License-Identifier: MPL-2.0 */
/* What the engine played, written out for a musician (score.h). Host simulator only: on the FM-1 and in the browser
   build this file is empty. Note names use the simplest spelling (Sam: Bb, not A#). */
#if defined(PLAT_HOST) && !defined(PLAT_WEB)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "score.h"
#include "groove.h"
#include "harmony.h"
#include "theory.h"
#include "library.h"
#include "plan.h"

#define SR 44100.0f
#define PPQ 480                                          /* MIDI ticks a quarter; a 16th is 120 */
#define MAXBARS 96
#define MAXREC 40000

typedef struct { int bar, st, voice, note, vel; float delay, len, ss; } rec_t;   /* delay, len in samples */

static const char *const FLATS[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
static const char *const SHARPS[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const MODE[MODE_N] = {"major", "Dorian", "Phrygian", "Lydian", "Mixolydian", "minor"};
static const char *const STYLE[4] = {"held", "stabs", "offbeats", "arpeggio"};
static const char *const BASS[4] = {"roots", "root-fifth", "walking", "pedal"};
static const char *const MOOD[4] = {"sad", "wistful", "warm", "sunny"};
static const char *const COLOR[3] = {"7ths", "9ths", "rich"};

static const char *prefix;
static int opened, count, live;                         /* a song is being recorded */
static FILE *logf;
static song_roll_t roll;
static song_plan_t plan;
static rec_t *recs;
static int nrec, bars, fifths;
static float step_ss[MAXBARS * 16];                     /* samples a 16th, per step (tempo glides) */
static char chord[MAXBARS][16];
static int section[MAXBARS], newblock[MAXBARS];
/* the log's line for the bar in progress */
static int lbar = -1, lnn, llead, lcandy, lnotes[4];
static char lline[160];

/* ---- spelling ---- */

/* the key's simplest spelling: the parent major scale with the fewest accidentals, flats on a tie; fifths < 0 flats */
static int key_fifths(int key, int mode)
{
    static const int8_t OFF[MODE_N] = {0, 2, 4, 5, 7, 9};
    static const int8_t SH[12] = {0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5}, FL[12] = {0, 5, 10, 3, 8, 1, 6, 11, 4, 9, 2, 7};
    int parent = (key - OFF[mode] + 12) % 12;
    return SH[parent] < FL[parent] ? SH[parent] : -FL[parent];
}

static const char *pc_name(int pc) { return fifths > 0 ? SHARPS[pc % 12] : FLATS[pc % 12]; }

/* ---- the log ---- */

static void log_flush(void)
{
    int i;
    if (!logf || lbar < 0)
        return;
    fputs(lline, logf);
    for (i = 0; i < lnn; i++)
        fprintf(logf, "%s%s%d", i ? " " : "  ", pc_name(lnotes[i]), lnotes[i] / 12 - 1);
    fprintf(logf, "%s%s\n", llead ? "  +lead" : "", lcandy ? "  +candy" : "");
    lbar = -1;
}

/* ---- MIDI ---- */

typedef struct { unsigned char *b; int n, cap; } buf_t;
typedef struct { long tick; int order; unsigned char d[4]; int nd; const char *text; int meta; } mev_t;

static void put(buf_t *b, int c)
{
    if (b->n == b->cap) {
        b->cap = b->cap ? 2 * b->cap : 4096;
        b->b = realloc(b->b, (size_t)b->cap);
    }
    b->b[b->n++] = (unsigned char)c;
}

static void vlq(buf_t *b, unsigned long v)
{
    unsigned char t[5];
    int k = 0;
    t[k++] = v & 0x7F;
    while (v >>= 7)
        t[k++] = (unsigned char)(0x80 | (v & 0x7F));
    while (k--)
        put(b, t[k]);
}

static int mev_cmp(const void *a, const void *b)
{
    const mev_t *x = a, *y = b;
    if (x->tick != y->tick) return x->tick < y->tick ? -1 : 1;
    return x->order - y->order;
}

static void track(FILE *f, mev_t *ev, int n)
{
    buf_t b = {0, 0, 0};
    long now = 0;
    int i, k;
    qsort(ev, (size_t)n, sizeof *ev, mev_cmp);
    for (i = 0; i < n; i++) {
        vlq(&b, (unsigned long)(ev[i].tick - now));
        now = ev[i].tick;
        if (ev[i].meta) {
            int len = ev[i].text ? (int)strlen(ev[i].text) : ev[i].nd;
            put(&b, 0xFF);
            put(&b, ev[i].meta);
            vlq(&b, (unsigned long)len);
            for (k = 0; k < len; k++)
                put(&b, ev[i].text ? ev[i].text[k] : ev[i].d[k]);
        } else {
            for (k = 0; k < ev[i].nd; k++)
                put(&b, ev[i].d[k]);
        }
    }
    put(&b, 0); put(&b, 0xFF); put(&b, 0x2F); put(&b, 0);
    fwrite("MTrk", 1, 4, f);
    fputc(b.n >> 24, f); fputc(b.n >> 16 & 0xFF, f); fputc(b.n >> 8 & 0xFF, f); fputc(b.n & 0xFF, f);
    fwrite(b.b, 1, (size_t)b.n, f);
    free(b.b);
}

static long tick_of(const rec_t *r) { return (long)(r->bar * 16 + r->st) * 120 + (long)(r->delay / r->ss * 120.0f + 0.5f); }

static void write_midi(const char *path)
{
    static const char *const NAME[5] = {"Keys", "Bass", "Lead", "Ear candy", "Drums"};
    static const int PROG[4] = {4, 32, 73, 11};          /* GM: electric piano 1, acoustic bass, flute, vibraphone */
    static const int DRUM[V_N] = {0, 0, 0, 0, 36, 38, 42, 46, 70, 37};
    static char texts[MAXBARS * 2][48];
    FILE *f = fopen(path, "wb");
    mev_t *ev = malloc(sizeof *ev * (size_t)(2 * nrec + 4 * MAXBARS * 16 + 16));
    int t, i, n, nt = 0;
    float last = -1.0f;
    if (!f || !ev) {
        if (f) fclose(f);
        free(ev);
        return;
    }
    fwrite("MThd\0\0\0\6\0\1\0\6", 1, 12, f);
    fputc(PPQ >> 8, f); fputc(PPQ & 0xFF, f);
    n = 0;                                               /* the conductor: tempo, meter, sections, chords */
    ev[n] = (mev_t){0, 0, {4, 2, 24, 8}, 4, NULL, 0x58};
    n++;
    for (i = 0; i < bars * 16; i++)
        if (step_ss[i] > 0.0f && (last < 0.0f || step_ss[i] - last > 0.5f || last - step_ss[i] > 0.5f)) {
            unsigned long us = (unsigned long)(step_ss[i] * 4.0f / SR * 1e6f + 0.5f);
            ev[n] = (mev_t){(long)i * 120, 1, {(unsigned char)(us >> 16), (unsigned char)(us >> 8), (unsigned char)us}, 3,
                            NULL, 0x51};
            n++;
            last = step_ss[i];
        }
    for (i = 0; i < bars && nt + 2 <= MAXBARS * 2; i++) {
        if (newblock[i]) {
            snprintf(texts[nt], sizeof texts[nt], "%s", plan_section_name(section[i]));
            ev[n++] = (mev_t){(long)i * 1920, 2, {0}, 0, texts[nt++], 0x06};
        }
        if (!i || strcmp(chord[i], chord[i - 1])) {
            snprintf(texts[nt], sizeof texts[nt], "%s", chord[i]);
            ev[n++] = (mev_t){(long)i * 1920, 3, {0}, 0, texts[nt++], 0x01};
        }
    }
    track(f, ev, n);
    for (t = 0; t < 5; t++) {                            /* keys, bass, lead, candy (channels 1-4), drums (10) */
        int ch = t < 4 ? t : 9;
        n = 0;
        ev[n++] = (mev_t){0, 0, {0}, 0, NAME[t], 0x03};
        if (t < 4)
            ev[n++] = (mev_t){0, 1, {(unsigned char)(0xC0 | ch), (unsigned char)PROG[t]}, 2, NULL, 0};
        for (i = 0; i < nrec; i++) {
            const rec_t *r = &recs[i];
            int is = t == 4 ? r->voice >= V_KICK : r->voice == (t == 0 ? V_CHORD : t == 1 ? V_BASS : t == 2 ? V_LEAD : V_CANDY);
            int note = t == 4 ? DRUM[r->voice] : r->note;
            long on = tick_of(r), len = t == 4 ? 60 : (long)(r->len / r->ss * 120.0f + 0.5f);
            if (!is || !note) continue;
            if (len < 1) len = 1;
            ev[n++] = (mev_t){on, 3, {(unsigned char)(0x90 | ch), (unsigned char)note, (unsigned char)r->vel}, 3, NULL, 0};
            ev[n++] = (mev_t){on + len, 2, {(unsigned char)(0x80 | ch), (unsigned char)note, 0}, 3, NULL, 0};
        }
        track(f, ev, n);
    }
    fclose(f);
    free(ev);
}

/* ---- MusicXML ---- */

typedef struct { int start, end, np, p[8]; } grp_t;     /* in 16ths from the song's start */

/* one staff's notes as onset groups: notes struck on the same 16th sound together; each group lasts until its
   longest note ends or the next group starts */
static int groups(int voice, int lo, int hi, grp_t *g, int max)
{
    int i, n = 0, k;
    for (i = 0; i < nrec; i++) {
        const rec_t *r = &recs[i];
        int at = r->bar * 16 + r->st, dur = (int)(r->len / r->ss + 0.5f);
        if (r->voice != voice || r->note < lo || r->note > hi)
            continue;
        if (dur < 1) dur = 1;
        for (k = 0; k < n && g[k].start != at; k++)
            ;
        if (k == n) {
            if (n == max) continue;
            g[n].start = at;
            g[n].end = at + dur;
            g[n].np = 0;
            n++;
        }
        if (g[k].end < at + dur) g[k].end = at + dur;
        if (g[k].np < 8) {
            int j, dup = 0;
            for (j = 0; j < g[k].np; j++) dup |= g[k].p[j] == r->note;
            if (!dup) g[k].p[g[k].np++] = r->note;
        }
    }
    for (i = 1; i < n; i++)                              /* by onset */
        for (k = i; k > 0 && g[k].start < g[k - 1].start; k--) {
            grp_t t = g[k];
            g[k] = g[k - 1];
            g[k - 1] = t;
        }
    for (i = 0; i + 1 < n; i++)
        if (g[i].end > g[i + 1].start) g[i].end = g[i + 1].start;
    return n;
}

static const char *type_of(int d, int *dot)
{
    *dot = d == 12 || d == 6 || d == 3;
    switch (d) {
    case 16: return "whole";
    case 12: case 8: return "half";
    case 6: case 4: return "quarter";
    case 3: case 2: return "eighth";
    default: return "16th";
    }
}

/* the longest written value that fits `left` 16ths at bar position pos, keeping long notes on the beat */
static int piece(int pos, int left)
{
    static const int V[8] = {16, 12, 8, 6, 4, 3, 2, 1};
    int i;
    for (i = 0; i < 8; i++)
        if (V[i] <= left && (V[i] < 4 ? 1 : pos % 4 == 0) && (pos % 8 + V[i] <= 8 || pos % 8 == 0))
            return V[i];
    return 1;
}

static void xml_pitch(FILE *f, int note)
{
    const char *nm = pc_name(note % 12);
    fprintf(f, "<pitch><step>%c</step>", nm[0]);
    if (nm[1]) fprintf(f, "<alter>%d</alter>", nm[1] == 'b' ? -1 : 1);
    fprintf(f, "<octave>%d</octave></pitch>", note / 12 - 1);
}

/* one staff's measure: rests and notes (ties across the bar line and within it) */
static void xml_measure(FILE *f, const grp_t *g, int n, int bar, int staff, int voice)
{
    int pos = 0, i;
    for (i = 0; i < n && pos < 16; i++) {
        int s = g[i].start - bar * 16, e = g[i].end - bar * 16, k;
        if (e <= 0 || s >= 16) continue;
        if (s < 0) s = 0;
        if (e > 16) e = 16;
        while (pos < s) {                                /* the rest before it */
            int d = piece(pos, s - pos), dot;
            const char *ty = type_of(d, &dot);
            fprintf(f, "<note><rest/><duration>%d</duration><voice>%d</voice><type>%s</type>%s", d, voice, ty,
                    dot ? "<dot/>" : "");
            if (staff) fprintf(f, "<staff>%d</staff>", staff);
            fputs("</note>\n", f);
            pos += d;
        }
        while (pos < e) {
            int d = piece(pos, e - pos), dot, tie_in = pos > s || g[i].start < bar * 16;
            int tie_out = pos + d < e || g[i].end > bar * 16 + 16;
            const char *ty = type_of(d, &dot);
            for (k = 0; k < g[i].np; k++) {
                fputs("<note>", f);
                if (k) fputs("<chord/>", f);
                xml_pitch(f, g[i].p[k]);
                fprintf(f, "<duration>%d</duration>", d);
                if (tie_in) fputs("<tie type=\"stop\"/>", f);
                if (tie_out) fputs("<tie type=\"start\"/>", f);
                fprintf(f, "<voice>%d</voice><type>%s</type>%s", voice, ty, dot ? "<dot/>" : "");
                if (staff) fprintf(f, "<staff>%d</staff>", staff);
                if (tie_in || tie_out)
                    fprintf(f, "<notations>%s%s</notations>", tie_in ? "<tied type=\"stop\"/>" : "",
                            tie_out ? "<tied type=\"start\"/>" : "");
                fputs("</note>\n", f);
            }
            pos += d;
        }
    }
    while (pos < 16) {
        int d = piece(pos, 16 - pos), dot;
        const char *ty = type_of(d, &dot);
        fprintf(f, "<note><rest/><duration>%d</duration><voice>%d</voice><type>%s</type>%s", d, voice, ty,
                dot ? "<dot/>" : "");
        if (staff) fprintf(f, "<staff>%d</staff>", staff);
        fputs("</note>\n", f);
        pos += d;
    }
}

static void xml_harmony(FILE *f, const char *name)
{
    static const struct { const char *suf, *kind; } K[] = {
        {"", "major"}, {"m", "minor"}, {"7", "dominant"}, {"maj7", "major-seventh"}, {"m7", "minor-seventh"},
        {"9", "dominant-ninth"}, {"maj9", "major-ninth"}, {"m9", "minor-ninth"}, {"11", "dominant-11th"},
        {"m11", "minor-11th"}, {"13", "dominant-13th"}, {"6", "major-sixth"}, {"m6", "minor-sixth"},
        {"m7b5", "half-diminished"}, {"dim7", "diminished-seventh"}, {"dim", "diminished"}};
    const char *suf = name + 1 + (name[1] == 'b' || name[1] == '#');
    const char *kind = "other";
    unsigned i;
    for (i = 0; i < sizeof K / sizeof K[0]; i++)
        if (!strcmp(suf, K[i].suf)) kind = K[i].kind;
    fprintf(f, "<harmony><root><root-step>%c</root-step>", name[0]);
    if (name[1] == 'b' || name[1] == '#') fprintf(f, "<root-alter>%d</root-alter>", name[1] == 'b' ? -1 : 1);
    fprintf(f, "</root><kind text=\"%s\">%s</kind></harmony>\n", suf, kind);
}

static void write_xml(const char *path)
{
    static grp_t lead[4096], keyu[4096], keyl[4096], bass[4096];
    FILE *f = fopen(path, "w");
    int nl, nu, nk, nb, b, tempo = roll.v[RF_TEMPO];
    if (!f)
        return;
    nl = groups(V_LEAD, 0, 127, lead, 4096);
    nu = groups(V_CHORD, 60, 127, keyu, 4096);
    nk = groups(V_CHORD, 0, 59, keyl, 4096);
    nb = groups(V_BASS, 0, 127, bass, 4096);
    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 4.0 "
          "Partwise//EN\" \"http://www.musicxml.org/dtds/partwise.dtd\">\n<score-partwise version=\"4.0\">\n", f);
    fprintf(f, "<work><work-title>VNL-1 song %d</work-title></work>\n", count);
    fprintf(f, "<identification><miscellaneous><miscellaneous-field name=\"vnl\">%s %s, %d BPM, seed %u"
               "</miscellaneous-field></miscellaneous></identification>\n",
            pc_name(roll.v[RF_KEY]), MODE[roll.v[RF_MODE]], tempo, (unsigned)roll.seed);
    fputs("<part-list><score-part id=\"P1\"><part-name>Lead</part-name></score-part><score-part id=\"P2\"><part-name>"
          "Keys</part-name></score-part><score-part id=\"P3\"><part-name>Bass</part-name></score-part></part-list>\n", f);
    fputs("<part id=\"P1\">\n", f);                     /* the lead, chord symbols and sections above it */
    for (b = 0; b < bars; b++) {
        fprintf(f, "<measure number=\"%d\">\n", b + 1);
        if (!b)
            fprintf(f, "<attributes><divisions>4</divisions><key><fifths>%d</fifths></key><time><beats>4</beats>"
                       "<beat-type>4</beat-type></time><clef><sign>G</sign><line>2</line></clef></attributes>\n"
                       "<direction placement=\"above\"><direction-type><metronome><beat-unit>quarter</beat-unit>"
                       "<per-minute>%d</per-minute></metronome></direction-type><sound tempo=\"%d\"/></direction>\n",
                    fifths, tempo, tempo);
        if (newblock[b])
            fprintf(f, "<direction placement=\"above\"><direction-type><rehearsal>%s</rehearsal></direction-type>"
                       "</direction>\n", plan_section_name(section[b]));
        if (!b || strcmp(chord[b], chord[b - 1])) xml_harmony(f, chord[b]);
        xml_measure(f, lead, nl, b, 0, 1);
        fputs("</measure>\n", f);
    }
    fputs("</part>\n<part id=\"P2\">\n", f);            /* the keys on a grand staff, split at middle C */
    for (b = 0; b < bars; b++) {
        fprintf(f, "<measure number=\"%d\">\n", b + 1);
        if (!b)
            fprintf(f, "<attributes><divisions>4</divisions><key><fifths>%d</fifths></key><time><beats>4</beats>"
                       "<beat-type>4</beat-type></time><staves>2</staves><clef number=\"1\"><sign>G</sign><line>2</line>"
                       "</clef><clef number=\"2\"><sign>F</sign><line>4</line></clef></attributes>\n", fifths);
        xml_measure(f, keyu, nu, b, 1, 1);
        fputs("<backup><duration>16</duration></backup>\n", f);
        xml_measure(f, keyl, nk, b, 2, 5);
        fputs("</measure>\n", f);
    }
    fputs("</part>\n<part id=\"P3\">\n", f);            /* the bass, in the 8vb bass clef it is read in */
    for (b = 0; b < bars; b++) {
        fprintf(f, "<measure number=\"%d\">\n", b + 1);
        if (!b)
            fprintf(f, "<attributes><divisions>4</divisions><key><fifths>%d</fifths></key><time><beats>4</beats>"
                       "<beat-type>4</beat-type></time><clef><sign>F</sign><line>4</line><clef-octave-change>-1"
                       "</clef-octave-change></clef></attributes>\n", fifths);
        xml_measure(f, bass, nb, b, 0, 1);
        fputs("</measure>\n", f);
    }
    fputs("</part>\n</score-partwise>\n", f);
    fclose(f);
}

/* ---- the song's files ---- */

static void finish(void)
{
    char path[512];
    log_flush();
    if (!live)
        return;
    live = 0;
    snprintf(path, sizeof path, "%s-%d.mid", prefix, count);
    write_midi(path);
    snprintf(path, sizeof path, "%s-%d.musicxml", prefix, count);
    write_xml(path);
    if (logf) fflush(logf);
}

static int ready(void)
{
    if (!opened) {
        char path[512];
        opened = 1;
        prefix = getenv("VNL_SCORE");
        if (prefix) {
            snprintf(path, sizeof path, "%s.log", prefix);
            logf = fopen(path, "w");
            recs = malloc(sizeof *recs * MAXREC);
            atexit(finish);                              /* a song the run stops in the middle of */
        }
    }
    return prefix && recs;
}

void score_song(const song_t *s, float now)
{
    const song_roll_t *r = &s->roll;
    const lib_prog_t *p = lib_prog_by_id((uint8_t)r->v[RF_PROG]);
    const lib_set_t *set = lib_set_by_id((uint8_t)r->v[RF_SET]);
    const lib_map_t *m = lib_map_by_id((uint8_t)r->v[RF_MAP]);
    int i, secs = r->v[RF_BARS] * 240 / r->v[RF_TEMPO];
    if (!ready())
        return;
    finish();
    roll = *r;
    plan = s->plan;
    bars = r->v[RF_BARS] < MAXBARS ? r->v[RF_BARS] : MAXBARS;
    nrec = 0;
    memset(step_ss, 0, sizeof step_ss);
    memset(newblock, 0, sizeof newblock);
    fifths = key_fifths(r->v[RF_KEY], r->v[RF_MODE]);
    count++;
    live = 1;
    for (i = 0; i < plan.n; i++)
        if (plan.b[i].start < MAXBARS) newblock[plan.b[i].start] = 1;
    if (!logf)
        return;
    fprintf(logf, "\n=== song %d at %.2fs (seed %u): %s-%d.mid, %s-%d.musicxml\n", count, (double)now, (unsigned)r->seed,
            prefix, count, prefix, count);
    fprintf(logf, "key %s %s, %d BPM, %d bars (%d:%02d), mood %s, motion %d, swing %.1f%%\n", pc_name(r->v[RF_KEY]),
            MODE[r->v[RF_MODE]], r->v[RF_TEMPO], r->v[RF_BARS], secs / 60, secs % 60, MOOD[r->v[RF_MOOD] & 3],
            r->v[RF_MOTION], (double)r->v[RF_SWING] / 10.0);
    fprintf(logf, "map %s; progression %d (%s); set %s; color %s; comping %s; bass %s\n", m ? m->name : "?",
            p ? p->id : 0, p ? p->src : "?", set ? set->name : "?", COLOR[r->v[RF_COLOR] % 3],
            STYLE[harmony_style(r) & 3], BASS[groove_bass_style(r) & 3]);
    fprintf(logf, "blocks:");
    for (i = 0; i < plan.n; i++) {
        const plan_block_t *b = &plan.b[i];
        fprintf(logf, " %d-%d %s%s%s%c", b->start + 1, b->start + b->bars, plan_section_name(b->section),
                (b->flags & PB_DARK) ? " (dark)" : "", (b->flags & PB_FADE) ? " (fade)" : "", i + 1 < plan.n ? ',' : '\n');
    }
}

void score_step(const song_t *s, int bar, int st, const song_ev_t *ev, int n, float now)
{
    int i;
    float ss = song_step_samples(s);
    if (!ready() || !live || bar >= MAXBARS)
        return;
    step_ss[bar * 16 + st] = ss;
    if (st == 0) {
        const plan_block_t *b = plan_at(&s->plan, bar);
        const lib_prog_t *p = lib_prog_by_id((uint8_t)s->roll.v[RF_PROG]);
        int loop = p && p->n * p->bars_per_chord ? p->n * p->bars_per_chord : 4, pcs[5], np;
        np = harmony_chord_at(&s->roll, bar, bar / loop + 1, b && (b->flags & PB_DARK), pcs);
        th_chord_name(pcs, np, chord[bar]);
        section[bar] = b ? b->section : 0;
        log_flush();
        snprintf(lline, sizeof lline, "%6.2fs  bar %2d  %-6s  %-8s", (double)now, bar + 1,
                 b ? plan_section_name(b->section) : "?", chord[bar]);
        lbar = bar;
        lnn = llead = lcandy = 0;
    }
    for (i = 0; i < n; i++) {
        if (nrec < MAXREC)
            recs[nrec++] = (rec_t){bar, st, ev[i].voice, ev[i].note, ev[i].vel, (float)ev[i].delay, (float)ev[i].len, ss};
        if (ev[i].voice == V_CHORD && lnn < 4) {         /* the bar's voicing, as first struck */
            int k, dup = 0;
            for (k = 0; k < lnn; k++) dup |= lnotes[k] == ev[i].note;
            if (!dup) lnotes[lnn++] = ev[i].note;
        }
        llead |= ev[i].voice == V_LEAD;
        lcandy |= ev[i].voice == V_CANDY;
    }
    if (s->state != SS_PLAYING)
        log_flush();
}

void score_gap(float now, float secs)
{
    if (!ready())
        return;
    finish();
    if (logf) {
        fprintf(logf, "%6.2fs  end; %.2fs of silence\n", (double)now, (double)secs);
        fflush(logf);
    }
}
#else
typedef int vnl_score_unused;                            /* (empty on the device and in the browser build) */
#endif

/* SPDX-License-Identifier: MPL-2.0 */
/* The seed library tables. Plain C on purpose: curate by editing entries; never reuse an id. */
#include <string.h>
#include "library.h"

/* Degrees are 0-based in the song's mode; {degree, alter, quality}. A progression lists only the modes where its
   degrees give the chords named in src (Task 4 filters by mode). */
const lib_prog_t LIB_PROGS[] = {
    {1, PK_VAMP, 2, 2, {{1, 0, Q_MIN7}, {4, 0, Q_DOM7}, {0, 0, 0}, {0, 0, 0}}, MOOD_WISTFUL | MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_IONIAN) | (1 << MODE_DORIAN) | (1 << MODE_LYDIAN) | (1 << MODE_MIXOLYDIAN) | (1 << MODE_AEOLIAN), "lofi-hip-hop.md 2, ii9-V13 vamp"},
    {2, PK_VAMP, 2, 2, {{3, 0, Q_MAJ7}, {2, 0, Q_MIN7}, {0, 0, 0}, {0, 0, 0}}, MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_IONIAN) | (1 << MODE_MIXOLYDIAN), "lofi-hip-hop.md 2, IVmaj7-iii7 vamp"},
    {3, PK_VAMP, 2, 2, {{0, 0, Q_MIN7}, {3, 0, Q_MIN7}, {0, 0, 0}, {0, 0, 0}}, MOOD_SAD | MOOD_WISTFUL,
     (1 << MODE_IONIAN) | (1 << MODE_DORIAN) | (1 << MODE_MIXOLYDIAN) | (1 << MODE_AEOLIAN), "lofi-hip-hop.md 2, i9-iv9 vamp"},
    {4, PK_VAMP, 2, 2, {{0, 0, Q_MAJ7}, {3, 0, Q_MAJ7}, {0, 0, 0}, {0, 0, 0}}, MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_IONIAN) | (1 << MODE_MIXOLYDIAN), "lofi-hip-hop.md 2, Imaj7-IVmaj7 vamp"},
    {5, PK_VAMP, 2, 2, {{5, 0, Q_MIN7}, {1, 0, Q_MIN7}, {0, 0, 0}, {0, 0, 0}}, MOOD_SAD | MOOD_WISTFUL,
     (1 << MODE_IONIAN) | (1 << MODE_DORIAN) | (1 << MODE_LYDIAN) | (1 << MODE_MIXOLYDIAN), "brainstorm: vi7-ii7 minor vamp"},
    {6, PK_VAMP, 2, 2, {{0, 0, Q_MIN7}, {6, 0, Q_DOM7}, {0, 0, 0}, {0, 0, 0}}, MOOD_SAD | MOOD_WISTFUL | MOOD_WARM,
     (1 << MODE_DORIAN) | (1 << MODE_AEOLIAN), "brainstorm: i7-bVII7 modal vamp (Cm7 Bb7 in C)"},
    {7, PK_LOOP, 4, 1, {{3, 0, Q_MAJ7}, {2, 0, Q_MIN7}, {1, 0, Q_MIN7}, {0, 0, Q_MAJ7}}, MOOD_WISTFUL | MOOD_WARM,
     (1 << MODE_IONIAN) | (1 << MODE_MIXOLYDIAN), "lofi-hip-hop.md 2, IVmaj7-iii7-ii7-Imaj7 descending loop"},
    {8, PK_LOOP, 4, 1, {{1, 0, Q_MIN7}, {4, 0, Q_DOM7}, {0, 0, Q_MAJ7}, {5, 0, Q_MIN7}}, MOOD_WISTFUL | MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_IONIAN) | (1 << MODE_LYDIAN) | (1 << MODE_MIXOLYDIAN), "lofi-hip-hop.md 2, ii9-V13-Imaj9-vi7 loop"},
    {9, PK_LOOP, 4, 1, {{3, 0, Q_MAJ7}, {4, 0, Q_DOM7}, {2, 0, Q_MIN7}, {5, 0, Q_MIN7}}, MOOD_SAD | MOOD_WISTFUL | MOOD_SUNNY,
     (1 << MODE_IONIAN) | (1 << MODE_MIXOLYDIAN), "lofi-hip-hop.md 2, IVmaj7-V7-iii7-vi7 royal road"},
    {10, PK_LOOP, 4, 1, {{5, 0, Q_MIN7}, {1, 0, Q_MIN7}, {4, 0, Q_DOM7}, {0, 0, Q_MAJ7}}, MOOD_SAD | MOOD_WISTFUL | MOOD_WARM,
     (1 << MODE_IONIAN) | (1 << MODE_LYDIAN) | (1 << MODE_MIXOLYDIAN), "lofi-hip-hop.md 2, vi9-ii9-V13-Imaj9 loop"},
    {11, PK_LOOP, 4, 1, {{0, 0, Q_MAJ7}, {5, 0, Q_MIN7}, {1, 0, Q_MIN7}, {4, 0, Q_DOM7}}, MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_IONIAN) | (1 << MODE_LYDIAN) | (1 << MODE_MIXOLYDIAN), "lofi-hip-hop.md 2, Imaj9-vi9-ii9-V13 turnaround loop"},
    {12, PK_LOOP, 4, 1, {{1, 0, Q_MIN7}, {4, 0, Q_DOM7}, {0, 0, Q_MAJ7}, {5, 0, Q_DOM7}}, MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_IONIAN) | (1 << MODE_LYDIAN) | (1 << MODE_MIXOLYDIAN), "lofi-hip-hop.md 2, bossa ii7-V7-Imaj7-VI7 (bossa flavor)"},
    {13, PK_LOOP, 4, 1, {{3, 0, Q_MAJ7}, {2, 0, Q_DOM7}, {5, 0, Q_MIN7}, {0, 0, Q_DOM7}}, MOOD_WISTFUL | MOOD_SUNNY,
     (1 << MODE_IONIAN) | (1 << MODE_MIXOLYDIAN), "brainstorm: city pop IVmaj7-III7-vi7-I7 (city pop flavor)"},
    {14, PK_JAZZY, 4, 1, {{3, 0, Q_MAJ7}, {3, 0, Q_MIN7}, {2, 0, Q_MIN7}, {5, 0, Q_DOM7}}, MOOD_SAD | MOOD_WISTFUL,
     (1 << MODE_IONIAN) | (1 << MODE_MIXOLYDIAN), "lofi-hip-hop.md 2, IVmaj7-iv7-iii7-VI7 borrowed iv"},
    {15, PK_JAZZY, 4, 1, {{1, 0, Q_MIN7}, {1, -1, Q_DOM7}, {0, 0, Q_MAJ7}, {0, 0, Q_MAJ7}}, MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_IONIAN) | (1 << MODE_LYDIAN) | (1 << MODE_MIXOLYDIAN), "lofi-hip-hop.md 2, ii9-bII7-Imaj9 tritone sub (held a bar to fit 4 bars)"},
    {16, PK_JAZZY, 4, 1, {{0, 0, Q_MAJ7}, {0, 0, Q_DOM7}, {3, 0, Q_MAJ7}, {3, 0, Q_MIN7}}, MOOD_WISTFUL | MOOD_WARM,
     (1 << MODE_IONIAN) | (1 << MODE_MIXOLYDIAN), "lofi-hip-hop.md 2, Imaj7-I7-IVmaj7-iv7"},
    {17, PK_VAMP, 2, 2, {{0, 0, Q_MIN7}, {3, 0, Q_DOM7}, {0, 0, 0}, {0, 0, 0}}, MOOD_WISTFUL | MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_DORIAN) | (1 << MODE_AEOLIAN), "brainstorm: Dorian i7-IV7 vamp (Cm7 F7 in C)"},
    {18, PK_LOOP, 4, 1, {{0, 0, Q_MIN7}, {6, 0, Q_DOM7}, {5, 0, Q_MAJ7}, {4, 0, Q_DOM7}}, MOOD_SAD | MOOD_WISTFUL,
     (1 << MODE_AEOLIAN), "brainstorm: Aeolian i7-bVII7-bVImaj7-V7 (Cm7 Bb7 Abmaj7 G7)"},
    {19, PK_LOOP, 4, 1, {{0, 0, Q_MIN7}, {6, 0, Q_DOM7}, {5, -1, Q_MAJ7}, {4, 0, Q_DOM7}}, MOOD_SAD | MOOD_WISTFUL,
     (1 << MODE_DORIAN), "brainstorm: Dorian-key i7-bVII7-bVImaj7-V7 (borrowed bVI, Abmaj7)"},
    {20, PK_LOOP, 4, 1, {{0, 0, Q_MIN7}, {3, 0, Q_MIN7}, {6, 0, Q_DOM7}, {2, 0, Q_MAJ7}}, MOOD_SAD | MOOD_WISTFUL,
     (1 << MODE_DORIAN) | (1 << MODE_AEOLIAN), "brainstorm: i7-iv7-bVII7-bIIImaj7 (Cm7 Fm7 Bb7 Ebmaj7)"},
    {21, PK_VAMP, 2, 2, {{3, 0, Q_DOM7}, {6, 0, Q_DOM7}, {0, 0, 0}, {0, 0, 0}}, MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_DORIAN) | (1 << MODE_AEOLIAN), "brainstorm: Dorian IV7-bVII7 funk vamp (F7 Bb7 in C)"},
    {22, PK_JAZZY, 4, 1, {{0, 0, Q_MIN7}, {3, 0, Q_MIN7}, {5, 0, Q_MAJ7}, {4, 0, Q_DOM7}}, MOOD_SAD | MOOD_WISTFUL,
     (1 << MODE_AEOLIAN), "brainstorm R8: im9-iv9-bVImaj7-V7, major V forced (Cm7 Fm7 Abmaj7 G7)"},
    {23, PK_JAZZY, 4, 1, {{0, 0, Q_MIN7}, {5, 0, Q_DOM7}, {4, 0, Q_DOM7}, {0, 0, Q_MIN7}}, MOOD_SAD | MOOD_WISTFUL,
     (1 << MODE_AEOLIAN), "brainstorm R8: im9-bVI7-V7-im9 (Cm7 Ab7 G7 Cm7)"},
    {24, PK_JAZZY, 2, 2, {{0, 0, Q_MIN7}, {1, -1, Q_MAJ7}, {0, 0, 0}, {0, 0, 0}}, MOOD_SAD | MOOD_WISTFUL | MOOD_WARM,
     (1 << MODE_DORIAN) | (1 << MODE_AEOLIAN), "brainstorm R8: im9-bIImaj7 Neapolitan color (Cm7 Dbmaj7)"},
    {25, PK_LOOP, 4, 1, {{0, 0, Q_MIN7}, {3, 0, Q_DOM7}, {6, 0, Q_MAJ7}, {2, 0, Q_MAJ7}}, MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_DORIAN) | (1 << MODE_AEOLIAN), "brainstorm R8: bright Dorian i7-IV7-bVIImaj7-bIIImaj7 loop (Cm7 F7 Bbmaj7 Ebmaj7)"},
    {26, PK_LOOP, 4, 1, {{0, 0, Q_MIN7}, {1, 0, Q_MIN7}, {2, 0, Q_MAJ7}, {3, 0, Q_DOM7}}, MOOD_WISTFUL | MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_DORIAN) | (1 << MODE_AEOLIAN), "brainstorm R8: rising Dorian i7-ii7-bIIImaj7-IV7 loop (Cm7 Dm7 Ebmaj7 F7)"},
};
const int LIB_NPROGS = (int)(sizeof LIB_PROGS / sizeof LIB_PROGS[0]);

/* Roles are meant to sit in separate registers (curation intent). {id, name, chords, bass, lead, candy, kit, flavor, moods, src} */
const lib_set_t LIB_SETS[] = {
    {1, "Rhodes dusty", INS_RHODES, INS_UPRIGHT, INS_FLUTE, INS_MUSIC_BOX, KIT_DUSTY, FL_LOFI, MOOD_WISTFUL | MOOD_WARM, "task brief: the classic dusty set"},
    {2, "Bossa nylon", INS_NYLON, INS_SUB, INS_VIBES, INS_KALIMBA, KIT_BRUSHES, FL_BOSSA, MOOD_WARM | MOOD_SUNNY, "task brief: bossa set"},
    {3, "Neo-soul Wurli", INS_WURLI, INS_ELEC_BASS, INS_SAX, INS_BELLS, KIT_SOFT, FL_NEOSOUL, MOOD_SAD | MOOD_WISTFUL, "task brief: neo-soul set"},
    {4, "Felt and trumpet", INS_FELT_PIANO, INS_UPRIGHT, INS_MUTED_TPT, INS_NONE, KIT_DUSTY, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "task brief: felt piano with muted trumpet"},
    {5, "Chillwave pad", INS_PAD, INS_SUB, INS_KALIMBA, INS_VOX_PAD, KIT_808, FL_CHILLWAVE, MOOD_WARM | MOOD_SUNNY, "task brief: chillwave set"},
    {6, "City pop Rhodes", INS_RHODES, INS_ELEC_BASS, INS_GUITAR_LICK, INS_BELLS, KIT_SOFT, FL_CITYPOP, MOOD_WARM | MOOD_SUNNY, "task brief: bright Rhodes city pop set"},
    {7, "Felt and flute", INS_FELT_PIANO, INS_SUB, INS_FLUTE, INS_MUSIC_BOX, KIT_SOFT, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "brainstorm: quiet felt piano lofi"},
    {8, "Wurli and vibes", INS_WURLI, INS_UPRIGHT, INS_VIBES, INS_KALIMBA, KIT_DUSTY, FL_LOFI, MOOD_WARM | MOOD_SUNNY, "brainstorm: warm Wurlitzer lofi"},
    {9, "Nylon and sax", INS_NYLON, INS_UPRIGHT, INS_SAX, INS_NONE, KIT_BRUSHES, FL_BOSSA, MOOD_WISTFUL | MOOD_WARM, "brainstorm: late-night bossa"},
    {10, "Pad and guitar", INS_PAD, INS_ELEC_BASS, INS_GUITAR_LICK, INS_VOX_PAD, KIT_808, FL_CHILLWAVE, MOOD_SAD | MOOD_WISTFUL, "brainstorm: hazy chillwave"},
    {11, "Rhodes neo-soul", INS_RHODES, INS_ELEC_BASS, INS_MUTED_TPT, INS_VIBES, KIT_SOFT, FL_NEOSOUL, MOOD_WARM | MOOD_SUNNY, "brainstorm: smooth neo-soul"},
};
const int LIB_NSETS = (int)(sizeof LIB_SETS / sizeof LIB_SETS[0]);

/* Lanes: kick, snare, hat, open hat, shaker, rim. 16 steps, bit i = step i. */
const lib_pattern_t LIB_PATTERNS[] = {
    {1, PF_BOOMBAP, {0x4441, 0x1010, 0x5555, 0x0000, 0x0000, 0x0000}, {0x0004, 0x4008, 0xAAAA, 0x0000, 0x0000, 0x0000}, 0, 0, "lofi-midi-arrangements.md 3.4, Us3 Cantaloop grid"},
    {2, PF_BOOMBAP, {0x8581, 0x1010, 0x5555, 0x8000, 0x0000, 0x0000}, {0x0000, 0x0080, 0xAAAA, 0x0000, 0x0000, 0x0000}, 0, 0, "lofi-midi-arrangements.md 3.4, Fugees Fu-Gee-La grid"},
    {3, PF_BOOMBAP, {0x0409, 0x1010, 0x5555, 0x8000, 0x5555, 0x0000}, {0x0020, 0x0808, 0x0000, 0x0000, 0xAAAA, 0x0000}, 0, 0, "lofi-midi-arrangements.md 3.4, Tupac Changes grid; lofi-hip-hop.md 6 Magenta"},
    {4, PF_HALFTIME, {0x0041, 0x0100, 0x5555, 0x0000, 0x0000, 0x0000}, {0x0400, 0x1000, 0xAAAA, 0x0000, 0x0000, 0x0000}, 1, 0, "lofi-midi-arrangements.md 3.4, Online Sequencer half-time snare + recurring kick"},
    {5, PF_HALFTIME, {0x1011, 0x0100, 0x1111, 0x8000, 0x9249, 0x0000}, {0x0400, 0x0000, 0x4444, 0x0000, 0x0000, 0x0000}, 1, 1, "lofi-midi-arrangements.md 3.4, OS recurring kick; poly3 shaker from lofi-hip-hop.md 3"},
    {6, PF_SPARSE, {0x0041, 0x0100, 0x1111, 0x0000, 0x0000, 0x1000}, {0x0000, 0x0000, 0x4444, 0x0000, 0x0000, 0x0000}, 1, 0, "lofi-midi-arrangements.md 3.4, quarter hats with half-time snare"},
    {7, PF_BRUSHES, {0x0101, 0x1010, 0x5555, 0x0000, 0x0000, 0x0000}, {0x0400, 0x0104, 0xAAAA, 0x0000, 0x0000, 0x0000}, 0, 0, "lofi-hip-hop.md 3 swung brush/ride lane; rim on snare lane"},
    {8, PF_BOSSA, {0x0909, 0x1000, 0x5555, 0x0000, 0x0000, 0x1449}, {0x0000, 0x0010, 0xAAAA, 0x0000, 0x0000, 0x0000}, 0, 0, "lofi-hip-hop.md 3 bossa feel; 3-2 clave on the rim lane"},
};
const int LIB_NPATTERNS = (int)(sizeof LIB_PATTERNS / sizeof LIB_PATTERNS[0]);

const lib_map_t LIB_MAPS[] = {
    {1, "Classic", 4, EN_TOGETHER, MID_BREAK, END_KEYS_OUTRO, 0, "spec: the five maps; lofi-audio-arrangements.md 4/7"},
    {2, "Slow bloom", 8, EN_SNARE_FIRST, MID_NONE, END_FADE, 0, "spec: the five maps; lofi-audio-arrangements.md 4/7"},
    {3, "Straight in", 0, EN_FROM_TOP, MID_HATS_ONLY, END_RING, 0, "spec: the five maps; lofi-audio-arrangements.md 4/7"},
    {4, "Breather", 4, EN_TOGETHER, MID_KICK_OUT, END_KEYS_OUTRO, 0, "spec: the five maps; lofi-audio-arrangements.md 4/7"},
    {5, "Drift", 4, EN_TOGETHER, MID_NONE, END_FADE, 1, "spec: the five maps; lofi-audio-arrangements.md 4/7"},
};
const int LIB_NMAPS = (int)(sizeof LIB_MAPS / sizeof LIB_MAPS[0]);

/* mode_w: ionian dorian phrygian lydian mixolydian aeolian; color_w: 7, 9, rich. */
const lib_mood_t LIB_MOODS[4] = {
    {{1, 4, 0, 0, 1, 6}, {1, 4, 5}, -2, 20},   /* sad */
    {{1, 5, 0, 0, 2, 5}, {2, 5, 3}, -1, 30},   /* wistful */
    {{5, 2, 0, 1, 5, 1}, {3, 5, 2}, 0, 35},    /* warm */
    {{6, 1, 0, 4, 3, 0}, {3, 5, 2}, 0, 50},    /* sunny */
};

const lib_prog_t *lib_prog_by_id(uint8_t id)
{
    int i;
    for (i = 0; i < LIB_NPROGS; i++) if (LIB_PROGS[i].id == id) return &LIB_PROGS[i];
    return NULL;
}

const lib_set_t *lib_set_by_id(uint8_t id)
{
    int i;
    for (i = 0; i < LIB_NSETS; i++) if (LIB_SETS[i].id == id) return &LIB_SETS[i];
    return NULL;
}

const lib_pattern_t *lib_pattern_by_id(uint8_t id)
{
    int i;
    for (i = 0; i < LIB_NPATTERNS; i++) if (LIB_PATTERNS[i].id == id) return &LIB_PATTERNS[i];
    return NULL;
}

const lib_map_t *lib_map_by_id(uint8_t id)
{
    int i;
    for (i = 0; i < LIB_NMAPS; i++) if (LIB_MAPS[i].id == id) return &LIB_MAPS[i];
    return NULL;
}

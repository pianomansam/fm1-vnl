/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The seed library tables. Plain C on purpose: curate by editing entries; never reuse an id. */
#include <string.h>
#include "library.h"

/* Degrees are 0-based in the song's mode; {degree, alter, quality}. A progression lists only the modes where its
   degrees give the chords named in src (Task 4 filters by mode). R25: a vamp's or loop's chords lie in each mode it
   lists at every color (Q_DIATONIC where the mode's own chord is the one named, so its color tones fit too); only
   jazzy progressions borrow. */
const lib_prog_t LIB_PROGS[] = {
    {1, PK_VAMP, 2, 2, {{1, 0, Q_MIN7}, {4, 0, Q_DOM7}, {0, 0, 0}, {0, 0, 0}}, MOOD_WISTFUL | MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_IONIAN), "lofi-hip-hop.md 2, ii9-V13 vamp"},
    {2, PK_VAMP, 2, 2, {{3, 0, Q_MAJ7}, {2, 0, Q_DIATONIC}, {0, 0, 0}, {0, 0, 0}}, MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_IONIAN), "lofi-hip-hop.md 2, IVmaj7-iii7 vamp"},
    {3, PK_VAMP, 2, 2, {{0, 0, Q_MIN7}, {3, 0, Q_MIN7}, {0, 0, 0}, {0, 0, 0}}, MOOD_SAD | MOOD_WISTFUL,
     (1 << MODE_AEOLIAN), "lofi-hip-hop.md 2, i9-iv9 vamp"},
    {4, PK_VAMP, 2, 2, {{0, 0, Q_MAJ7}, {3, 0, Q_MAJ7}, {0, 0, 0}, {0, 0, 0}}, MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_IONIAN), "lofi-hip-hop.md 2, Imaj7-IVmaj7 vamp"},
    {5, PK_VAMP, 2, 2, {{5, 0, Q_DIATONIC}, {1, 0, Q_MIN7}, {0, 0, 0}, {0, 0, 0}}, MOOD_SAD | MOOD_WISTFUL,
     (1 << MODE_IONIAN) | (1 << MODE_MIXOLYDIAN), "brainstorm: vi7-ii7 minor vamp"},
    {6, PK_VAMP, 2, 2, {{0, 0, Q_MIN7}, {6, 0, Q_DOM7}, {0, 0, 0}, {0, 0, 0}}, MOOD_SAD | MOOD_WISTFUL | MOOD_WARM,
     (1 << MODE_AEOLIAN), "brainstorm: i7-bVII7 modal vamp (Cm7 Bb7 in C)"},
    {7, PK_LOOP, 4, 1, {{3, 0, Q_MAJ7}, {2, 0, Q_DIATONIC}, {1, 0, Q_MIN7}, {0, 0, Q_MAJ7}}, MOOD_WISTFUL | MOOD_WARM,
     (1 << MODE_IONIAN), "lofi-hip-hop.md 2, IVmaj7-iii7-ii7-Imaj7 descending loop"},
    {8, PK_LOOP, 4, 1, {{1, 0, Q_MIN7}, {4, 0, Q_DOM7}, {0, 0, Q_MAJ7}, {5, 0, Q_MIN7}}, MOOD_WISTFUL | MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_IONIAN), "lofi-hip-hop.md 2, ii9-V13-Imaj9-vi7 loop"},
    {9, PK_LOOP, 4, 1, {{3, 0, Q_MAJ7}, {4, 0, Q_DOM7}, {2, 0, Q_DIATONIC}, {5, 0, Q_MIN7}}, MOOD_SAD | MOOD_WISTFUL | MOOD_SUNNY,
     (1 << MODE_IONIAN), "lofi-hip-hop.md 2, IVmaj7-V7-iii7-vi7 royal road"},
    {10, PK_LOOP, 4, 1, {{5, 0, Q_MIN7}, {1, 0, Q_MIN7}, {4, 0, Q_DOM7}, {0, 0, Q_MAJ7}}, MOOD_SAD | MOOD_WISTFUL | MOOD_WARM,
     (1 << MODE_IONIAN), "lofi-hip-hop.md 2, vi9-ii9-V13-Imaj9 loop"},
    {11, PK_LOOP, 4, 1, {{0, 0, Q_MAJ7}, {5, 0, Q_MIN7}, {1, 0, Q_MIN7}, {4, 0, Q_DOM7}}, MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_IONIAN), "lofi-hip-hop.md 2, Imaj9-vi9-ii9-V13 turnaround loop"},
    {12, PK_JAZZY, 4, 1, {{1, 0, Q_MIN7}, {4, 0, Q_DOM7}, {0, 0, Q_MAJ7}, {5, 0, Q_DOM7}}, MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_IONIAN), "lofi-hip-hop.md 2, bossa ii7-V7-Imaj7-VI7 (bossa flavor); R25: borrows, so jazzy"},
    {13, PK_JAZZY, 4, 1, {{3, 0, Q_MAJ7}, {2, 0, Q_DOM7}, {5, 0, Q_MIN7}, {0, 0, Q_DOM7}}, MOOD_WISTFUL | MOOD_SUNNY,
     (1 << MODE_IONIAN), "brainstorm: city pop IVmaj7-III7-vi7-I7 (city pop flavor); R25: borrows, so jazzy"},
    {14, PK_JAZZY, 4, 1, {{3, 0, Q_MAJ7}, {3, 0, Q_MIN7}, {2, 0, Q_MIN7}, {5, 0, Q_DOM7}}, MOOD_SAD | MOOD_WISTFUL,
     (1 << MODE_IONIAN) | (1 << MODE_MIXOLYDIAN), "lofi-hip-hop.md 2, IVmaj7-iv7-iii7-VI7 borrowed iv"},
    {15, PK_JAZZY, 4, 1, {{1, 0, Q_MIN7}, {4, 0, Q_DOM7}, {0, 0, Q_MAJ7}, {5, 0, Q_DOM7}}, MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_IONIAN), "lofi-hip-hop.md 2, ii9-V13-Imaj9-VI7 turnaround (was a ii-bII7-I tritone sub: none, Sam)"},
    {16, PK_JAZZY, 4, 1, {{0, 0, Q_MAJ7}, {0, 0, Q_DOM7}, {3, 0, Q_MAJ7}, {3, 0, Q_MIN7}}, MOOD_WISTFUL | MOOD_WARM,
     (1 << MODE_IONIAN) | (1 << MODE_MIXOLYDIAN), "lofi-hip-hop.md 2, Imaj7-I7-IVmaj7-iv7"},
    {17, PK_VAMP, 2, 2, {{0, 0, Q_MIN7}, {3, 0, Q_DOM7}, {0, 0, 0}, {0, 0, 0}}, MOOD_WISTFUL | MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_DORIAN), "brainstorm: Dorian i7-IV7 vamp (Cm7 F7 in C)"},
    {18, PK_JAZZY, 4, 1, {{0, 0, Q_MIN7}, {6, 0, Q_DOM7}, {5, 0, Q_MAJ7}, {4, 0, Q_DOM7}}, MOOD_SAD | MOOD_WISTFUL,
     (1 << MODE_AEOLIAN), "brainstorm: Aeolian i7-bVII7-bVImaj7-V7 (Cm7 Bb7 Abmaj7 G7); R25: borrows, so jazzy"},
    {19, PK_JAZZY, 4, 1, {{0, 0, Q_MIN7}, {6, 0, Q_DOM7}, {5, -1, Q_MAJ7}, {4, 0, Q_DOM7}}, MOOD_SAD | MOOD_WISTFUL,
     (1 << MODE_DORIAN), "brainstorm: Dorian-key i7-bVII7-bVImaj7-V7 (borrowed bVI, Abmaj7); R25: borrows, so jazzy"},
    {20, PK_LOOP, 4, 1, {{0, 0, Q_MIN7}, {3, 0, Q_MIN7}, {6, 0, Q_DOM7}, {2, 0, Q_MAJ7}}, MOOD_SAD | MOOD_WISTFUL,
     (1 << MODE_AEOLIAN), "brainstorm: i7-iv7-bVII7-bIIImaj7 (Cm7 Fm7 Bb7 Ebmaj7)"},
    {21, PK_JAZZY, 2, 2, {{3, 0, Q_DOM7}, {6, 0, Q_DOM7}, {0, 0, 0}, {0, 0, 0}}, MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_DORIAN), "brainstorm: Dorian IV7-bVII7 funk vamp (F7 Bb7 in C); R25: borrows, so jazzy"},
    {22, PK_JAZZY, 4, 1, {{0, 0, Q_MIN7}, {3, 0, Q_MIN7}, {5, 0, Q_MAJ7}, {4, 0, Q_DOM7}}, MOOD_SAD | MOOD_WISTFUL,
     (1 << MODE_AEOLIAN), "brainstorm R8: im9-iv9-bVImaj7-V7, major V forced (Cm7 Fm7 Abmaj7 G7)"},
    {23, PK_JAZZY, 4, 1, {{0, 0, Q_MIN7}, {3, 0, Q_MIN7}, {4, 0, Q_DOM7}, {0, 0, Q_MIN7}}, MOOD_SAD | MOOD_WISTFUL,
     (1 << MODE_AEOLIAN), "brainstorm R8: im9-ivm9-V7-im9, major V forced (Cm7 Fm7 G7 Cm7; was bVI7, a tritone sub: none, Sam)"},
    {24, PK_JAZZY, 2, 2, {{0, 0, Q_MIN7}, {1, -1, Q_MAJ7}, {0, 0, 0}, {0, 0, 0}}, MOOD_SAD | MOOD_WISTFUL | MOOD_WARM,
     (1 << MODE_DORIAN) | (1 << MODE_AEOLIAN), "brainstorm R8: im9-bIImaj7 Neapolitan color (Cm7 Dbmaj7)"},
    {25, PK_LOOP, 4, 1, {{0, 0, Q_MIN7}, {3, 0, Q_DOM7}, {6, 0, Q_MAJ7}, {2, 0, Q_MAJ7}}, MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_DORIAN), "brainstorm R8: bright Dorian i7-IV7-bVIImaj7-bIIImaj7 loop (Cm7 F7 Bbmaj7 Ebmaj7)"},
    {26, PK_LOOP, 4, 1, {{0, 0, Q_MIN7}, {1, 0, Q_DIATONIC}, {2, 0, Q_MAJ7}, {3, 0, Q_DOM7}}, MOOD_WISTFUL | MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_DORIAN), "brainstorm R8: rising Dorian i7-ii7-bIIImaj7-IV7 loop (Cm7 Dm7 Ebmaj7 F7)"},
    {27, PK_VAMP, 2, 2, {{0, 0, Q_DIATONIC}, {1, 0, Q_DIATONIC}, {0, 0, 0}, {0, 0, 0}}, MOOD_WISTFUL | MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_LYDIAN), "R25: Lydian Imaj7-II7 vamp (Cmaj7 D7)"},
    {28, PK_VAMP, 2, 2, {{0, 0, Q_DIATONIC}, {4, 0, Q_DIATONIC}, {0, 0, 0}, {0, 0, 0}}, MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_LYDIAN), "R25: Lydian Imaj7-Vmaj7 vamp (Cmaj7 Gmaj7)"},
    {29, PK_VAMP, 2, 2, {{0, 0, Q_DIATONIC}, {6, 0, Q_DIATONIC}, {0, 0, 0}, {0, 0, 0}}, MOOD_WISTFUL | MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_MIXOLYDIAN), "R25: Mixolydian I7-bVIImaj7 vamp (C7 Bbmaj7)"},
    {30, PK_VAMP, 2, 2, {{0, 0, Q_DIATONIC}, {3, 0, Q_DIATONIC}, {0, 0, 0}, {0, 0, 0}}, MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_MIXOLYDIAN), "R25: Mixolydian I7-IVmaj7 vamp (C7 Fmaj7)"},
    {31, PK_VAMP, 2, 2, {{4, 0, Q_DIATONIC}, {0, 0, Q_DIATONIC}, {0, 0, 0}, {0, 0, 0}}, MOOD_SAD | MOOD_WISTFUL,
     (1 << MODE_DORIAN) | (1 << MODE_MIXOLYDIAN), "R25: v7-i vamp (Dorian Gm7 Cm7; Mixolydian Gm7 C7)"},
    {32, PK_VAMP, 2, 2, {{0, 0, Q_DIATONIC}, {1, 0, Q_DIATONIC}, {0, 0, 0}, {0, 0, 0}}, MOOD_SAD | MOOD_WISTFUL | MOOD_SUNNY,
     (1 << MODE_DORIAN), "R25: Dorian i7-ii7 vamp (Cm7 Dm7)"},
    {33, PK_VAMP, 2, 2, {{0, 0, Q_DIATONIC}, {2, 0, Q_DIATONIC}, {0, 0, 0}, {0, 0, 0}}, MOOD_SAD | MOOD_WISTFUL | MOOD_WARM,
     (1 << MODE_DORIAN) | (1 << MODE_AEOLIAN), "R25: i7-bIIImaj7 vamp (Cm7 Ebmaj7)"},
    {34, PK_VAMP, 2, 2, {{5, 0, Q_DIATONIC}, {4, 0, Q_DIATONIC}, {0, 0, 0}, {0, 0, 0}}, MOOD_SAD | MOOD_WISTFUL,
     (1 << MODE_MIXOLYDIAN), "R25: Mixolydian vi7-v7 vamp (Am7 Gm7)"},
    {35, PK_LOOP, 4, 1, {{0, 0, Q_DIATONIC}, {1, 0, Q_DIATONIC}, {6, 0, Q_DIATONIC}, {2, 0, Q_DIATONIC}}, MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_LYDIAN), "R25: Lydian Imaj7-II7-vii7-iii7 loop (Cmaj7 D7 Bm7 Em7)"},
    {36, PK_LOOP, 4, 1, {{0, 0, Q_DIATONIC}, {5, 0, Q_DIATONIC}, {1, 0, Q_DIATONIC}, {4, 0, Q_DIATONIC}}, MOOD_WISTFUL | MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_LYDIAN), "R25: Lydian Imaj7-vi7-II7-Vmaj7 loop (Cmaj7 Am7 D7 Gmaj7)"},
    {37, PK_LOOP, 4, 1, {{0, 0, Q_DIATONIC}, {4, 0, Q_DIATONIC}, {6, 0, Q_DIATONIC}, {3, 0, Q_DIATONIC}}, MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_MIXOLYDIAN), "R25: Mixolydian I7-v7-bVIImaj7-IVmaj7 loop (C7 Gm7 Bbmaj7 Fmaj7)"},
    {38, PK_LOOP, 4, 1, {{5, 0, Q_DIATONIC}, {4, 0, Q_DIATONIC}, {3, 0, Q_DIATONIC}, {0, 0, Q_DIATONIC}}, MOOD_SAD | MOOD_WISTFUL,
     (1 << MODE_MIXOLYDIAN), "R25: Mixolydian vi7-v7-IVmaj7-I7 loop (Am7 Gm7 Fmaj7 C7)"},
    {39, PK_LOOP, 4, 1, {{1, 0, Q_DIATONIC}, {4, 0, Q_DIATONIC}, {0, 0, Q_DIATONIC}, {3, 0, Q_DIATONIC}}, MOOD_WISTFUL | MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_MIXOLYDIAN), "R25: Mixolydian ii7-v7-I7-IVmaj7 loop (Dm7 Gm7 C7 Fmaj7)"},
    {40, PK_LOOP, 4, 1, {{0, 0, Q_DIATONIC}, {4, 0, Q_DIATONIC}, {6, 0, Q_DIATONIC}, {3, 0, Q_DIATONIC}}, MOOD_SAD | MOOD_WISTFUL,
     (1 << MODE_DORIAN), "R25: Dorian i7-v7-bVIImaj7-IV7 loop (Cm7 Gm7 Bbmaj7 F7)"},
    {41, PK_LOOP, 4, 1, {{0, 0, Q_DIATONIC}, {4, 0, Q_DIATONIC}, {2, 0, Q_DIATONIC}, {6, 0, Q_DIATONIC}}, MOOD_SAD | MOOD_WISTFUL | MOOD_WARM,
     (1 << MODE_DORIAN) | (1 << MODE_AEOLIAN), "R25: i7-v7-bIIImaj7-bVII loop (Dorian Cm7 Gm7 Ebmaj7 Bbmaj7; Aeolian ... Bb7)"},
    {42, PK_LOOP, 4, 1, {{5, 0, Q_DIATONIC}, {6, 0, Q_DIATONIC}, {0, 0, Q_DIATONIC}, {2, 0, Q_DIATONIC}}, MOOD_WISTFUL | MOOD_WARM,
     (1 << MODE_AEOLIAN), "R25: Aeolian bVImaj7-bVII7-i7-bIIImaj7 loop (Abmaj7 Bb7 Cm7 Ebmaj7)"},
    {43, PK_LOOP, 4, 1, {{0, 0, Q_DIATONIC}, {2, 0, Q_DIATONIC}, {3, 0, Q_DIATONIC}, {4, 0, Q_DIATONIC}}, MOOD_WARM | MOOD_SUNNY,
     (1 << MODE_IONIAN), "R25: Ionian Imaj7-iii7-IVmaj7-V7 loop (Cmaj7 Em7 Fmaj7 G7)"},
    {44, PK_LOOP, 4, 1, {{0, 0, Q_DIATONIC}, {3, 0, Q_DIATONIC}, {2, 0, Q_DIATONIC}, {1, 0, Q_DIATONIC}}, MOOD_SAD | MOOD_WISTFUL,
     (1 << MODE_DORIAN), "R25: Dorian i7-IV7-bIIImaj7-ii7 loop (Cm7 F7 Ebmaj7 Dm7)"},
};
const int LIB_NPROGS = (int)(sizeof LIB_PROGS / sizeof LIB_PROGS[0]);

/* Roles are meant to sit in separate registers (curation intent). {id, name, chords, bass, lead, candy, pad, kit, skit, flavor, moods, src}.
   The pad layer: the warm synth pad under the synth sets (13, 18, 19, 20), strings under the rest.
   skit: the set's sampled kit; a song plays it or the synth kit, about half each (lib_song_kit). By mood and flavor:
   the dusty Rhodes and Knight sets warm or dark, the Wurli and quiet acoustic sets gentle, city pop clean, the synth
   and guitar-led sets rusty. KIT_N on the 808 and Brushes sets: no sampled equivalent, the synth kit always. */
const lib_set_t LIB_SETS[] = {
    {1, "Rhodes dusty", INS_RHODES, INS_ELEC_BASS, INS_FLUTE, INS_MUSIC_BOX, INS_STRINGS, KIT_DUSTY, KIT_S_WARM, FL_LOFI, MOOD_WISTFUL | MOOD_WARM, "task brief: the classic dusty set"},
    {2, "Bossa nylon", INS_NYLON, INS_SUB, INS_VIBES, INS_KALIMBA, INS_STRINGS, KIT_BRUSHES, KIT_N, FL_BOSSA, MOOD_WARM | MOOD_SUNNY, "task brief: bossa set"},
    {3, "Neo-soul Wurli", INS_WURLI, INS_ELEC_BASS, INS_SAX, INS_BELLS, INS_STRINGS, KIT_SOFT, KIT_S_GENTLE, FL_NEOSOUL, MOOD_SAD | MOOD_WISTFUL, "task brief: neo-soul set"},
    {4, "Felt and trumpet", INS_FELT_PIANO, INS_ELEC_BASS, INS_MUTED_TPT, INS_NONE, INS_STRINGS, KIT_DUSTY, KIT_S_DARK, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "task brief: felt piano with muted trumpet"},
    {5, "Chillwave pad", INS_PAD, INS_SUB, INS_KALIMBA, INS_VOX_PAD, INS_STRINGS, KIT_808, KIT_N, FL_CHILLWAVE, MOOD_WARM | MOOD_SUNNY, "task brief: chillwave set"},
    {6, "City pop Rhodes", INS_RHODES, INS_ELEC_BASS, INS_GUITAR_LICK, INS_BELLS, INS_STRINGS, KIT_SOFT, KIT_S_CLEAN, FL_CITYPOP, MOOD_WARM | MOOD_SUNNY, "task brief: bright Rhodes city pop set"},
    {7, "Felt and flute", INS_FELT_PIANO, INS_SUB, INS_FLUTE, INS_MUSIC_BOX, INS_STRINGS, KIT_SOFT, KIT_S_GENTLE, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "brainstorm: quiet felt piano lofi"},
    {8, "Wurli and vibes", INS_WURLI, INS_ELEC_BASS, INS_VIBES, INS_KALIMBA, INS_STRINGS, KIT_DUSTY, KIT_S_GENTLE, FL_LOFI, MOOD_WARM | MOOD_SUNNY, "brainstorm: warm Wurlitzer lofi"},
    {9, "Nylon and sax", INS_NYLON, INS_ELEC_BASS, INS_SAX, INS_NONE, INS_STRINGS, KIT_BRUSHES, KIT_N, FL_BOSSA, MOOD_WISTFUL | MOOD_WARM, "brainstorm: late-night bossa"},
    {10, "Pad and guitar", INS_PAD, INS_ELEC_BASS, INS_GUITAR_LICK, INS_VOX_PAD, INS_STRINGS, KIT_808, KIT_N, FL_CHILLWAVE, MOOD_SAD | MOOD_WISTFUL, "brainstorm: hazy chillwave"},
    {11, "Rhodes neo-soul", INS_RHODES, INS_ELEC_BASS, INS_MUTED_TPT, INS_VIBES, INS_STRINGS, KIT_SOFT, KIT_S_WARM, FL_NEOSOUL, MOOD_WARM | MOOD_SUNNY, "brainstorm: smooth neo-soul"},
    /* the synth sets (synths plan Task 4; Sam's cut, 2026-10-24: only the DX EP, the FM slap bass, the triangle bass,
       the organ, the saw lead and the synth pluck kept). 13, 18 and 20 all synth (keys, bass and lead), so every mood
       has one; the ear candy acoustic or none. Ids 12, 14-16 retired (the Juno, chip, organ and sine, Rhodes and Moog
       sets). */
    {13, "DX keys and FM bass", INS_DX_EP, INS_FM_BASS, INS_SAW_LEAD, INS_VIBES, INS_WARM_PAD, KIT_SOFT, KIT_S_CLEAN, FL_CITYPOP, MOOD_WARM | MOOD_SUNNY, "synths spec: 80s FM keys and bass, city pop; Sam's cut: the saw lead"},
    {18, "Organ and saw lead", INS_ORGAN, INS_CHIP_BASS, INS_SAW_LEAD, INS_MUSIC_BOX, INS_WARM_PAD, KIT_SOFT, KIT_S_RUSTY, FL_NEOSOUL, MOOD_SAD | MOOD_WISTFUL, "Sam's cut: an organ over the triangle bass under a saw lead"},
    {19, "Rhodes and saw lead", INS_RHODES, INS_FM_BASS, INS_SAW_LEAD, INS_NONE, INS_WARM_PAD, KIT_DUSTY, KIT_S_DARK, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "Sam's cut: a dusty Rhodes, the FM slap bass, a saw lead (was Rhodes and Moog)"},
    {20, "DX and pluck", INS_DX_EP, INS_CHIP_BASS, INS_SYNTH_PLUCK, INS_KALIMBA, INS_WARM_PAD, KIT_SOFT, KIT_S_RUSTY, FL_CHILLWAVE, MOOD_SAD | MOOD_WARM, "Sam's cut: DX keys, the triangle bass, a synth pluck, chillwave"},
    {17, "Nylon and synth pluck", INS_NYLON, INS_SUB, INS_SYNTH_PLUCK, INS_KALIMBA, INS_STRINGS, KIT_BRUSHES, KIT_N, FL_BOSSA, MOOD_WISTFUL | MOOD_SUNNY, "synths spec: a nylon guitar under a synth pluck"},
    /* the sampled sets (sampled instruments spec, Task 5): the Knight upright and the Emilyguitar each on the keys
       and on the lead, never both in one set (Sam: the one acoustic piano is the keys or the lead in a song; the
       Emilyguitar the same). */
    {21, "Knight and flute", INS_KNIGHT, INS_ELEC_BASS, INS_FLUTE, INS_MUSIC_BOX, INS_STRINGS, KIT_DUSTY, KIT_S_DARK, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "sampled spec: the Knight's keys under the flute, dusty lofi"},
    {22, "Knight and sax", INS_KNIGHT, INS_ELEC_BASS, INS_SAX, INS_VIBES, INS_STRINGS, KIT_SOFT, KIT_S_WARM, FL_NEOSOUL, MOOD_WISTFUL | MOOD_WARM, "sampled spec: the Knight's keys under the sax, neo-soul"},
    {23, "Rhodes and Knight", INS_RHODES, INS_ELEC_BASS, INS_KNIGHT, INS_NONE, INS_STRINGS, KIT_DUSTY, KIT_S_WARM, FL_LOFI, MOOD_WARM | MOOD_SUNNY, "sampled spec: a Knight lead over the Rhodes"},
    {24, "Wurli and Knight", INS_WURLI, INS_ELEC_BASS, INS_KNIGHT, INS_KALIMBA, INS_STRINGS, KIT_SOFT, KIT_S_GENTLE, FL_NEOSOUL, MOOD_SAD | MOOD_WISTFUL, "sampled spec: a Knight lead over the Wurli"},
    {25, "DX and Knight", INS_DX_EP, INS_FM_BASS, INS_KNIGHT, INS_BELLS, INS_WARM_PAD, KIT_SOFT, KIT_S_CLEAN, FL_CITYPOP, MOOD_WARM | MOOD_SUNNY, "sampled spec: a Knight lead over the DX keys and the FM bass, city pop"},
    {26, "Emilyguitar and vibes", INS_EMILY, INS_ELEC_BASS, INS_VIBES, INS_KALIMBA, INS_STRINGS, KIT_SOFT, KIT_S_RUSTY, FL_CITYPOP, MOOD_WARM | MOOD_SUNNY, "sampled spec: the Emilyguitar comping (strums, stabs) under the vibes"},
    {27, "Emilyguitar and flute", INS_EMILY, INS_SUB, INS_FLUTE, INS_MUSIC_BOX, INS_STRINGS, KIT_DUSTY, KIT_S_GENTLE, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "sampled spec: the Emilyguitar comping under the flute"},
    {28, "Rhodes and Emilyguitar", INS_RHODES, INS_ELEC_BASS, INS_EMILY, INS_VIBES, INS_STRINGS, KIT_SOFT, KIT_S_RUSTY, FL_NEOSOUL, MOOD_WARM | MOOD_SUNNY, "sampled spec: an Emilyguitar lead over the Rhodes"},
    {29, "Knight and Emilyguitar", INS_KNIGHT, INS_ELEC_BASS, INS_EMILY, INS_NONE, INS_STRINGS, KIT_BRUSHES, KIT_N, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "sampled spec: an Emilyguitar lead over the Knight, brushes"},
    /* the string synth's leads (sampled spec, Task 6): the high strings (2a) and the cello (2c low-dark, B3-A4) over
       percussive keys, the warm synth pad under them (the pad layer's strings would double the lead) */
    {30, "Knight and strings", INS_KNIGHT, INS_ELEC_BASS, INS_STR_LEAD, INS_MUSIC_BOX, INS_WARM_PAD, KIT_SOFT, KIT_S_GENTLE, FL_LOFI, MOOD_WISTFUL | MOOD_WARM, "sampled spec: a high string lead over the Knight (Task 5's deferred set)"},
    {31, "Rhodes and cello", INS_RHODES, INS_FM_BASS, INS_CELLO, INS_BELLS, INS_WARM_PAD, KIT_DUSTY, KIT_S_DARK, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "sampled spec: the cello lead inside a dusty Rhodes, the FM slap bass under them"},
    {32, "Knight and cello", INS_KNIGHT, INS_SUB, INS_CELLO, INS_NONE, INS_WARM_PAD, KIT_BRUSHES, KIT_N, FL_LOFI, MOOD_SAD | MOOD_WISTFUL, "sampled spec: the cello lead inside the Knight, brushes"},
    {33, "DX and strings", INS_DX_EP, INS_ELEC_BASS, INS_STR_LEAD, INS_VIBES, INS_WARM_PAD, KIT_SOFT, KIT_S_CLEAN, FL_CITYPOP, MOOD_WARM | MOOD_SUNNY, "sampled spec: a high string lead over the DX keys, city pop"},
};
const int LIB_NSETS = (int)(sizeof LIB_SETS / sizeof LIB_SETS[0]);

/* Lanes: kick, snare, hat, open hat, shaker, rim. 16 steps, bit i = step i. */
const lib_pattern_t LIB_PATTERNS[] = {
    {1, PF_BOOMBAP, {0x4441, 0x1010, 0x5555, 0x0000, 0x0000, 0x0000}, {0x0004, 0x4008, 0xAAAA, 0x0000, 0x0000, 0x0000}, 0, 0, "lofi-midi-arrangements.md 3.4, Us3 Cantaloop grid", 0, {0}, {0}},
    {2, PF_BOOMBAP, {0x8581, 0x1010, 0x5555, 0x8000, 0x0000, 0x0000}, {0x0000, 0x0080, 0xAAAA, 0x0000, 0x0000, 0x0000}, 0, 0, "lofi-midi-arrangements.md 3.4, Fugees Fu-Gee-La grid", 0, {0}, {0}},
    {3, PF_BOOMBAP, {0x0409, 0x1010, 0x5555, 0x8000, 0x5555, 0x0000}, {0x0020, 0x0808, 0x0000, 0x0000, 0xAAAA, 0x0000}, 0, 0, "lofi-midi-arrangements.md 3.4, Tupac Changes grid; lofi-hip-hop.md 6 Magenta", 0, {0}, {0}},
    {4, PF_HALFTIME, {0x0041, 0x0100, 0x5555, 0x0000, 0x0000, 0x0000}, {0x0400, 0x1000, 0xAAAA, 0x0000, 0x0000, 0x0000}, 1, 0, "lofi-midi-arrangements.md 3.4, Online Sequencer half-time snare + recurring kick", 0, {0}, {0}},
    {5, PF_HALFTIME, {0x1011, 0x0100, 0x1111, 0x8000, 0x9249, 0x0000}, {0x0400, 0x0000, 0x4444, 0x0000, 0x0000, 0x0000}, 1, 1, "lofi-midi-arrangements.md 3.4, OS recurring kick; poly3 shaker from lofi-hip-hop.md 3", 0, {0}, {0}},
    {6, PF_SPARSE, {0x0041, 0x0100, 0x1111, 0x0000, 0x0000, 0x1000}, {0x0000, 0x0000, 0x4444, 0x0000, 0x0000, 0x0000}, 1, 0, "lofi-midi-arrangements.md 3.4, quarter hats with half-time snare", 0, {0}, {0}},
    {7, PF_BRUSHES, {0x0101, 0x1010, 0x5555, 0x0000, 0x0000, 0x0000}, {0x0400, 0x0104, 0xAAAA, 0x0000, 0x0000, 0x0000}, 0, 0, "lofi-hip-hop.md 3 swung brush/ride lane; rim on snare lane", 0, {0}, {0}},
    {8, PF_BOSSA, {0x0909, 0x1000, 0x5555, 0x0000, 0x0000, 0x1449}, {0x0000, 0x0010, 0xAAAA, 0x0000, 0x0000, 0x0000}, 0, 0, "lofi-hip-hop.md 3 bossa feel; 3-2 clave on the rim lane", 0, {0}, {0}},
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

static const lib_set_t *wb_set;
static const lib_pattern_t *wb_pattern;
void lib_set_wb(const lib_set_t *s) { wb_set = s; }
void lib_pattern_wb(const lib_pattern_t *p) { wb_pattern = p; }

unsigned lib_lane_hits(const lib_pattern_t *p, int lane, int bar)
{
    return p->two_bar && (bar & 1) ? p->hit_b[lane] : p->hit[lane];
}

unsigned lib_lane_ghosts(const lib_pattern_t *p, int lane, int bar)
{
    return p->two_bar && (bar & 1) ? p->ghost_b[lane] : p->ghost[lane];
}

const lib_set_t *lib_set_by_id(uint8_t id)
{
    int i;
    if (id == LIB_SET_WB) return wb_set;
    for (i = 0; i < LIB_NSETS; i++) if (LIB_SETS[i].id == id) return &LIB_SETS[i];
    return NULL;
}

int lib_kit_sampled(int kit) { return kit >= KIT_S_WARM && kit <= KIT_S_CLEAN; }

const lib_pattern_t *lib_pattern_by_id(uint8_t id)
{
    int i;
    if (id == LIB_PATTERN_WB) return wb_pattern;
    for (i = 0; i < LIB_NPATTERNS; i++) if (LIB_PATTERNS[i].id == id) return &LIB_PATTERNS[i];
    return NULL;
}

const lib_map_t *lib_map_by_id(uint8_t id)
{
    int i;
    for (i = 0; i < LIB_NMAPS; i++) if (LIB_MAPS[i].id == id) return &LIB_MAPS[i];
    return NULL;
}

/* the keys' octave shift for a set: the Chillwave pad an octave lower (Sam, 2026-10-24: "only the chillwave pad needs
   to come down to that lower octave") */
int lib_set_keys_shift(const lib_set_t *s)
{
    return s && s->id == 5 ? -12 : 0;
}

/* the sets whose bass may walk: the four that had the upright (Sam, 2026-10-24: "I don't like the upright's sound";
   their bass is now the electric) */
int lib_set_walks(const lib_set_t *s)
{
    return s && (s->id == 1 || s->id == 4 || s->id == 8 || s->id == 9);
}

int lib_ins_sampled_struck(int ins) { return ins == INS_KNIGHT || ins == INS_NYLON || ins == INS_EMILY; }

int lib_ins_sustained(int ins)
{
    return ins == INS_PAD || ins == INS_FLUTE || ins == INS_SAX || ins == INS_MUTED_TPT || ins == INS_VOX_PAD ||
           ins == INS_ORGAN || ins == INS_SAW_LEAD || ins == INS_STRINGS || ins == INS_WARM_PAD ||
           ins == INS_STR_LEAD || ins == INS_CELLO;
}

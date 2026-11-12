# Changelog

## 0.9.0 (unreleased)

The first public release: a generative lo-fi hip hop firmware for the M-VAVE FM-1.

- **Endless songs:** each from a seed, with its own key, mode, tempo, chords, layout and band; a short silence, then
  the next. 37 instrument sets in five flavors, 44 progressions, five song layouts, four moods.
- **Six parts:** drums (eight kits, two of them sampled), bass (four), comp (eight instruments, eight rhythms, strummed
  guitars), lead (eight instruments, eight styles, looped like a sample), pad, and texture (dust, noise bed, room).
- **Sampled instruments:** piano, nylon, electric and jazz guitar, cello, harp and two room kits, from CC0 libraries, in
  a sample bank written once, by the browser installer or `tools/fm1_bank.py`.
- **A lo-fi mix:** wow and flutter, saturation, grit, chorus, delay, plate reverb, kick ducking and a whole-mix stutter.
- **The comp over a low lead:** with the jazz guitar or the cello leading, the comp keeps its own register with the
  lead (a note fewer and a dip where the lead sounds) in three songs out of four, or lifts an octave over it; the
  Comp screen's page 2 (K3, Over a low lead) forces either.
- **Bass that carries on small speakers:** every bass but the sub adds gentle harmonics (a parallel saturation)
  in the register it plays, so laptop and phone speakers hear its line; the sub stays clean. Settings' BASS
  SATURATION turns it off for good speakers (at the next bass note, kept through power-off).
- **On the device:** a boot splash, a now-playing screen, a main screen, a page per part, Parts, Sections and Mixer
  screens; mute and solo from the keys; every change landing on the next four-bar line; a screen reader; USB audio.
- **The mood on now playing:** the now-playing screen shows the song's mood (SAD, WISTFUL, WARM or SUNNY) on its
  own line under the tempo, and the screen reader says it.
- **Part locks:** hold SEQ on a part's screen, or on the main screen with its tile highlighted, and every new song
  keeps that part (its instrument, rhythm or style, register and variations) while the rest rolls. The song line
  locks too (key and scale, tempo, motion, mood).
- **Saved songs:** SAVE on the Song screen keeps the song in one of 16 slots (its whole roll and every change made
  to its parts), through power-off; REC opens the Saved list, where EDIT loads a song as the next one, SAVE saves into
  a chosen slot and SAVE held deletes. The flash is written only while all is silent (a save while music plays waits
  for the stop). SAVE held on the Song screen shows the measures SAVE shows elsewhere. A USB backup holds the saved
  songs, the home, the theme and the bass saturation; `tools/fm1_songs.py` reads and writes single songs.
- **Themes on the device:** Settings (GLO) has a working THEME: SELECT or ALGO steps through every palette fm1-base
  carries, at once and kept through power-off, so the colors can be judged on the FM-1's own screen. PLUM+ (peach on
  a saturated plum) is the default; PLUM, OXBLOOD and OXBLOOD+ are new beside teal, olive and espresso.
- **A CPU guard** that sheds reverb tails and quiet voices before the audio can glitch.
- **Tangerine on teal:** the screens, the workbench's device mock and the site share one palette (deep teal,
  tangerine, sand white); fm1-base also carries an olive and mustard one.
- **A site with a browser installer** (Chrome or Edge), in the screens' colors: the firmware
  (`vnl-1-0.9.0.fwsc`), then the sample bank; the emulator and the manual beside it.
- **For developers:** the host simulator, the browser emulator, the workbench, and patterns as MIDI and MusicXML.

Fixes:

- **Changes while a section loops:** a change due at the looping section's end used to wait forever (the loop went
  back first); it now lands as the loop starts again. Stay on song alike.
- **Saved songs through `tools/flash.sh`:** its stock V15 step erased them (stock claims that flash as its own); it now
  exports them first and writes them back after. `tools/fm1_songs.py list` tags each song `[flash]` or `[RAM only]`,
  and tells a failed flash write from one waiting for silence.

Known limits: no Bluetooth, so no velocity or pressure from the keys. The Settings screen's only working settings
are THEME and BASS SATURATION. See
`ROADMAP.md` for what's next.

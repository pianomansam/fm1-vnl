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
  in the register it plays, so laptop and phone speakers hear its line; the sub stays clean.
- **On the device:** a boot splash, a now-playing screen, a main screen, a page per part, Parts, Sections and Mixer
  screens; mute and solo from the keys; every change landing on the next four-bar line; a screen reader; USB audio.
- **The mood on now playing:** the now-playing screen shows the song's mood (SAD, WISTFUL, WARM or SUNNY) beside
  the tempo, and the screen reader says it.
- **Part locks:** hold SEQ on a part's screen, or on the main screen with its tile highlighted, and every new song
  keeps that part (its instrument, rhythm or style, register and variations) while the rest rolls. The song line
  locks too (key and scale, tempo, motion, mood).
- **A CPU guard** that sheds reverb tails and quiet voices before the audio can glitch.
- **A site with a browser installer** (Chrome or Edge), in the screens' espresso colors: the firmware
  (`vnl-1-0.9.0.fwsc`), then the sample bank; the emulator and the manual beside it.
- **For developers:** the host simulator, the browser emulator, the workbench, and patterns as MIDI and MusicXML.

Fixes:

- **Changes while a section loops:** a change due at the looping section's end used to wait forever (the loop went
  back first); it now lands as the loop starts again. Stay on song alike.

Known limits: no Bluetooth, so no velocity or pressure from the keys. The Settings screen is a placeholder. See
`ROADMAP.md` for what's next.

## About

ScummVM runs classic point-and-click adventure and role-playing games from their original data
files by reimplementing each game's engine: LucasArts' SCUMM games, Sierra's, Revolution's and
many others. The player supplies the game data. It is an SDL 2 program, drawn here by
[sdl2-compat](#port-sdl2-compat) on SDL 3, with its own launcher and menus and no toolkit.

## Flags

- `--enable-release` — Release mode, which defines `RELEASE_BUILD` and turns on optimisations;
  `--disable-debug` and `--enable-optimizations` say the same thing directly, and the build
  carries no debugging symbols. It also turns the update checker on, so `--disable-updates` has to
  come after it on the command line: configure reads the switches in order.
- `--disable-Werror` — Do not promote some warnings to errors, which configure does by default.
- `--disable-cloud` — This switch, `--disable-libcurl`, `--disable-sdlnet`, `--disable-enet`,
  `--disable-dlc`, `--disable-scummvmdlc`, `--disable-updates`, `--disable-sparkle`,
  `--disable-discord` and `--disable-libunity` leave out everything that reaches the network or
  another desktop's services: cloud saves, the libcurl and SDL_net networking libraries, ENet
  multiplayer, the DLC and update downloaders, Discord presence and the Unity launcher.
- `--disable-gtk` — No GTK file dialogs; the launcher's own file browser is used.
- `--disable-eventrecorder` — No event recorder, a developer tool for replaying input.
- `--enable-tts` — Text to speech through [speech-dispatcher](#port-speech-dispatcher), which reads
  the menus and subtitles aloud.
- `--disable-translation` — No translated interface messages.
- `--enable-alsa` — This switch and the other `--enable-` switches that follow (FluidSynth, the
  MT-32 emulator, Ogg, Vorbis, FLAC, MAD, FAAD, Theora, VPx, JPEG, PNG, GIF, FreeType, FriBidi,
  zlib, MPEG-2, A/52 and OpenMPT) are libraries configure would otherwise probe for. Named on, the
  probe is skipped, and a missing library is a build failure instead of a quietly smaller program.
  MPEG-2 and A/52 play the cutscenes of some games, and OpenMPT the tracker-module music.
- `--disable-fluidlite` — This switch and `--disable-sonivox`, `--disable-mpcdec`,
  `--disable-tremor`, `--disable-retrowave` and `--disable-opl2lpt` are libraries and hardware
  drivers with no port here, turned off rather than detected: FluidLite and Sonivox MIDI,
  Musepack audio, the Tremor integer Vorbis decoder (full Vorbis is used), and the RetroWave and
  OPL2LPT sound-card adapters.
- `--disable-seq-midi` — No OSS MIDI sequencer, and with `--disable-timidity` and
  `--disable-sndio`, no MIDI through TiMidity's network socket or sndio. MIDI is FluidSynth or the
  built-in MT-32 emulator.
- `--disable-mikmod` — No MikMod. configure uses it only when OpenMPT is absent.
- `--disable-readline` — No readline in the text debug console.

## Notes

The SDL 2 interface comes from sdl2-compat, whose `Provides: sdl2` answers pkg-config. Once
pkg-config answers for `sdl2`, configure takes its SDL flags only from an `sdl2-config` found on
`PATH`, and there is none here; without one it reads pkg-config's own version as SDL's and fails
its probe. `build.sh` writes a small `sdl2-config` into `$SRC_ROOT/sdl-shim`, first on `PATH`,
that answers `--cflags`, `--libs` and `--version` from pkg-config.

The panel reads no SVG, so the menu icon is a PNG rasterised from upstream's. Upstream's menu entry
is replaced, and SDL names the Wayland window after the executable, `scummvm`.

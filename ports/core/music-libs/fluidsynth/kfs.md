## About

FluidSynth is a software synthesiser that plays MIDI with SoundFont instruments: the `fluidsynth`
program and `libfluidsynth`, which SDL_mixer, the Doom engines and other programs use for their
MIDI music. The SoundFont comes from [fluidr3-gm-sf3](#port-fluidr3-gm-sf3), which installs
`/usr/share/soundfonts/default.sf2`, a link to its SF3 SoundFont, at the path FluidSynth compiles in
as its default (`DEFAULT_SOUNDFONT` in `cmake_admin/DefaultDirs.cmake`), so
`fluidsynth -a alsa tune.mid` plays with no SoundFont named.

## Flags

- `-Denable-alsa=ON`, `-Denable-pipewire=ON` — ALSA and native PipeWire audio drivers. The desktop
  session runs PipeWire while a text console has only ALSA, so both are built.
- `-Denable-pulseaudio=OFF` — Builds no PulseAudio driver; in the session, the PipeWire driver
  reaches the same graph directly.
- `-Denable-jack=ON` — The JACK audio and MIDI drivers, against PipeWire's `libjack`. There is no
  JACK server, and once `jack.pc` is installed a port that leaves JACK on auto-detection gains it
  or not by build order, so it is named, with [pipewire](#port-pipewire) in `depends` (see
  [JACK programs on the host](../../../../docs/kdos/03-architecture/session.md#jack-programs-on-the-host)).
- `-Denable-sdl3=OFF`, `-Denable-oss=OFF` — Builds neither the SDL 3 audio driver nor the OSS one.
- `-Denable-readline=ON` — Line editing in the `fluidsynth` shell, through readline.
- `-Denable-libsndfile=ON` — Through [libsndfile](#port-libsndfile): rendering to an audio file,
  and decoding the Ogg Vorbis samples of an SF3 SoundFont. Without it FluidSynth refuses a version
  3 SoundFont, which is what fluidr3-gm-sf3 installs.
- `-Denable-dbus=ON` — D-Bus, through which FluidSynth asks rtkit for a realtime audio thread.
- `-Denable-openmp=ON` — OpenMP, from GCC's libgomp: a SoundFont's samples are decoded in parallel
  as it loads, the effects units run in parallel, and the voice-mixing loops are vectorised.
- `-Denable-ladspa=ON` — LADSPA effect units in the audio path, against [ladspa](#port-ladspa).
- `-Denable-signalsmith=OFF` — Builds without the Signalsmith reverb and limiter. Their source is a
  git submodule, `signalsmith-audio-basics/`, which the release archive leaves empty.
- `-Denable-systemd=OFF` — Builds without libsystemd, which the `fluidsynth` program would link
  only to report readiness to a systemd service manager with `sd_notify()`. KDOS has no systemd
  (see [No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)).
- `-Denable-midishare=OFF` — Builds no MidiShare driver; MidiShare is not a port.

## Notes

Upstream's `enable-*` switches mean "if it is available", so each probe is named on, with its port
in `depends`, or off. The two `grep -q` lines after configure check `config.h` for JACK and LADSPA,
so a probe that came back empty stops the build instead of shipping without them.

gcem, the header-only compile-time maths library FluidSynth requires, is a git submodule upstream,
and the release archive leaves its `gcem/` directory empty. `cmake_admin/FindGCEM.cmake` looks for
`gcem/include/gcem.hpp` in the source tree and downloads the archive from GitHub at configure time
only when it is missing, and the build has no network. The pinned revision is therefore the
recipe's second `source =`, unpacked by `build.sh` into `gcem/` where the probe looks. Nothing is
compiled or installed from it.

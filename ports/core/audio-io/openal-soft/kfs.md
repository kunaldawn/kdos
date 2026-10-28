## About

OpenAL Soft is an implementation of OpenAL, the 3D positional audio API games and emulators use.
In KDOS it plays through PipeWire first, with PulseAudio, ALSA and JACK behind it, all four of
which reach the same PipeWire server. It also renders binaural sound for headphones from HRTF data
that is built into the library.

## Flags

- `-DALSOFT_UPDATE_BUILD_VERSION=OFF` — Do not ask git for the commit and branch to compile in; a
  release tarball is not a git checkout.
- `-DALSOFT_ENABLE_MODULES=OFF` — Compile from headers rather than as C++20 modules, which upstream
  uses by default where the compiler supports them.
- `-DALSOFT_UTILS=ON` — Build the utilities: `openal-info`, `makemhr`, `uhjdecoder`, `uhjencoder`
  and `sofa-info`. Upstream's install rule installs only the first two, so `build.sh` copies the
  other three into `/usr/bin` after the install.
- `-DALSOFT_NO_CONFIG_UTIL=ON` — Leave out `alsoft-config`, a Qt program. Its settings are plain
  text, read from `~/.alsoftrc` and `~/.config/alsoft.conf`.
- `-DALSOFT_EXAMPLES=OFF`, `-DALSOFT_TESTS=OFF`, `-DALSOFT_INSTALL_EXAMPLES=OFF` — No example
  players and no tests.
- `-DALSOFT_EMBED_HRTF_DATA=ON` — Compile the default HRTF data sets into the library, so binaural
  output needs no data files. It is upstream's default.
- `-DALSOFT_RTKIT=ON`, `-DALSOFT_REQUIRE_RTKIT=ON` — Build the RTKit fallback, and fail the
  configure if its D-Bus support is missing. The mixer thread first raises itself to real-time
  priority with `pthread_setschedparam` and asks RTKit over D-Bus only when that fails. The image
  ships no RTKit daemon; the direct call succeeds because `kdos-getty` raises `RLIMIT_RTPRIO`
  before the login (see
  [Starting a session](../../../../docs/kdos/03-architecture/session.md#starting-a-session)).
- `-DALSOFT_BACKEND_PIPEWIRE=ON` — This switch and the other three backends turned on with it,
  PulseAudio, ALSA and JACK, each with its `ALSOFT_REQUIRE_` switch also on (`-DALSOFT_REQUIRE_PIPEWIRE=ON`
  and the rest). Without the `REQUIRE_` switch a backend whose library is missing is dropped at
  configure time, and the library is built with no way to open a device. JACK is PipeWire's
  `libjack`.
- `-DALSOFT_BACKEND_OSS=OFF`, `-DALSOFT_BACKEND_SNDIO=OFF`, `-DALSOFT_BACKEND_PORTAUDIO=OFF`,
  `-DALSOFT_BACKEND_SDL2=OFF`, `-DALSOFT_BACKEND_SDL3=OFF` — The other output backends. sndio is not
  a port; PortAudio and SDL would reach the same sound server through another library, and OSS is
  the kernel's older device interface, beside PipeWire rather than through it.
- `-DALSOFT_BACKEND_WAVE=ON` — The backend that writes the mix to a WAV file instead of a device,
  chosen in the configuration. It is upstream's default.
- `-DCMAKE_REQUIRE_FIND_PACKAGE_MySOFA=ON` — Make CMake's `find_package(MySOFA)` a hard
  requirement. `makemhr` and `sofa-info` read SOFA HRTF sets through
  [libmysofa](#port-libmysofa), and upstream otherwise leaves both out when the library is
  missing.

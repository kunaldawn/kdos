## About

Hydrogen is a pattern-based drum machine: drum kits of samples are played from patterns in a
step sequencer, and the patterns are arranged into songs. It is a Qt 6 program, plays through
ALSA, PulseAudio or JACK, takes MIDI and OSC control, and loads LADSPA effects. Its user manual
and tutorial come from a separate documentation repository, the second source of the recipe.

## Flags

- `-D CMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON` — when configure finds Doxygen it adds a `doc` target
  for the API documentation and two directory-making targets to every build; disabled, the build is
  the same whether or not Doxygen is installed.
- `-D WANT_QT6=ON` — build against Qt 6; upstream's default is Qt 5. Configure falls back to Qt 5
  if Qt 6 is not found.
- `-D WANT_SHARED=ON` — build the core library, `hydrogen-core`, as a shared library, as
  upstream does by default on Linux.
- `-D WANT_DEBUG=OFF` — upstream's default is `ON`, which compiles with `-O0`. Off, it compiles
  with `-O3 -ffast-math` (see Notes).
- `-D WANT_LIBARCHIVE=ON` — read and write drum-kit archives with [libarchive](#port-libarchive)
  instead of libtar.
- `-D WANT_ALSA=ON` — and `WANT_PULSEAUDIO`, `WANT_JACK`: the three audio drivers. JACK is the
  JACK API that [pipewire](#port-pipewire) provides.
- `-D WANT_LADSPA=ON` — LADSPA effect plugins ([ladspa](#port-ladspa)), with `-D WANT_LRDF=ON` for
  [lrdf](#port-lrdf), which reads the plugins' RDF descriptions to sort them into categories.
- `-D WANT_OSC=ON` — Open Sound Control remote control through [liblo](#port-liblo).
- `-D WANT_PORTAUDIO=OFF` — with `WANT_PORTMIDI` and `WANT_OSS`: the PortAudio, PortMidi and OSS
  drivers, which upstream's defaults turn on only on Windows (PortAudio, PortMidi) or NetBSD (OSS).
- `-D WANT_LASH=OFF` — no LASH session-handler support; LASH is not a port.
- `-D WANT_RUBBERBAND=OFF` — no linking of the Rubber Band time-stretching library. Upstream marks
  that support experimental, says it produces wrong timing, and asks for the option to stay off;
  Hydrogen uses the `rubberband` command-line program instead when it finds one.
- `-D WANT_APPIMAGE=OFF` — no AppImage-specific behaviour.
- `-D WANT_CPPUNIT=OFF` — no CppUnit test suite, which upstream builds by default;
  `-D WANT_INTEGRATION_TESTS=OFF` and `-D WANT_CLANG_TIDY=OFF` leave out the integration tests and
  the clang-tidy checks.

## Patches

- `nodevel.patch` — In `CMakeLists.txt`, sets `IS_DEVEL_BUILD` to `false` in the branch taken when `git describe --exact-match --tags` finds no tag. A release tarball has no git history, so that branch is always taken, and a development build shows a "You're using a development version of Hydrogen" message box at start-up. With the patch the tarball builds as the release it is.

## Notes

Hydrogen's `CMakeLists.txt` builds `CMAKE_CXX_FLAGS` from the environment variable of that name,
not from `CXXFLAGS`, and empties `CMAKE_CXX_FLAGS_RELEASE`. `build.sh` therefore exports
`CMAKE_CXX_FLAGS="$CXXFLAGS"` before the configure; without it the tree's compiler flags would not
reach the compile.

The manual is a git submodule upstream, so the source tarball carries `data/doc` as an empty
directory. `build.sh` removes it and moves the unpacked documentation tarball into its place, where
`CMakeLists.txt` builds it when `data/doc/CMakeLists.txt` exists. After the install the French and
Italian tutorials and the `i18n` directory of translations are deleted, and the build checks that
`manual_en.html` was installed. Upstream installs its icon as SVG only, which the panel does not draw, so the recipe rasterises it
to PNGs from 32 to 128 pixels. It also writes its own desktop entry: Hydrogen sets no desktop file
name, so its Wayland app id is the executable's name, `hydrogen`, which the entry names in
`StartupWMClass`, and upstream's entry claims the `text/xml` MIME type, every XML file on the
system, which the replacement leaves out.

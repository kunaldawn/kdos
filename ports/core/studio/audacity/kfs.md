## About

Audacity is a multitrack audio editor and recorder. This is Audacity 4, whose interface is
rewritten in Qt 6 and QML on the framework MuseScore Studio also uses ("muse"), over the audio
engine and libraries of Audacity 3 (the tree's `au3/`). It records, edits and mixes tracks and
runs its built-in effects, LV2 plugins and Nyquist effects; the tree carries Audacity 3's LADSPA
host, but no build file compiles it. FFmpeg import and export loads the libraries of the
[ffmpeg](#port-ffmpeg) port at run time.

## Flags

- `-D EXTDEPS_CACHE="$SRC/offline-deps"` — exported as an environment variable as well. Audacity's
  dependency engine, `muse_deps`, resolves each third-party library by downloading a source or a
  prebuilt archive into a cache. The release tarball carries every source the engine can fetch
  under `offline-deps/downloads`, and pointing the cache there means nothing is downloaded. The
  `-D` form is read by the configure, the environment variable by the engine's per-dependency
  builds.
- `-D EXTDEPS_OVERRIDE_ALL=SYSTEM` — take every library dependency the engine would otherwise
  fetch prebuilt or rebuild from its port instead: expat, the Ogg, Vorbis, FLAC, Opus, LAME,
  mpg123, WavPack and libsndfile codecs, PortAudio, libpng, zlib, FreeType, HarfBuzz, pugixml and
  wxWidgets' base library, each in `depends`. The sources the engine has no system path for are
  compiled into the program from the tarball's copies: SQLite, soxr, SoundTouch, SBSMS, twolame,
  the LV2 host stack, Nyquist, pffft, the VST3 SDK the framework always compiles, and a few
  header-only libraries.
- `-D EXTDEPS_OVERRIDE_UTFCPP=REBUILD` — a per-dependency override, which wins over the `_ALL` one:
  utf8cpp is header-only and is rebuilt from the tarball's copy, where `SYSTEM` would swap it for
  the CMake package of the [utfcpp](#port-utfcpp) port.
- `-D CMAKE_SKIP_INSTALL_RPATH=ON` — install every binary with no `RPATH`. The bundled
  KDDockWidgets sets `CMAKE_INSTALL_RPATH_USE_LINK_PATH`, which writes the link directories into
  the installed files; Audacity's libraries are in `/usr/lib`, where the loader looks anyway (see
  [what the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)).
- `-D AU4_BUILD_CONFIGURATION=app` — the desktop application, rather than the portable Windows build
  and the unit-test configuration.
- `-D AU4_BUILD_MODE=release` — a stable release. Upstream's default is `dev`, which marks the
  program unstable and names it `Audacity4Development`.
- `-D MUSE_ENABLE_UNIT_TESTS=OFF` — none of the framework's or the modules' unit tests, which
  default to on.
- `-D MUSE_COMPILE_USE_CCACHE=OFF` — upstream's default puts any `ccache` it finds in front of the
  compiler; off, the tree's compiler is called directly.
- `-D MUSE_MODULE_UPDATE=OFF` — no update checker; upstream's stub module takes its place.
- `-D MUSE_MODULE_DIAGNOSTICS_CRASHPAD_CLIENT=OFF` — no Crashpad crash-reporting client. Off by
  default upstream, which turns it on for its own CI builds.
- `-D AU_BUILD_CLOUD_AUDIOCOM=OFF` — no audio.com cloud module; `-D AU_BUILD_USAGEINFO_MODULE=OFF` leaves out the usage-information module. Both are on
  by default upstream.
- `-D AU_USE_LIBCURL=OFF` — build the network manager without libcurl, which is not in `depends`.
  This is upstream's default.
- `-D AU_MODULE_EFFECTS_VST=OFF` — no VST3 effect hosting; upstream's VST stub module is built in
  its place (see `vst-stub-include.patch`).
- `-D AU_MODULE_EFFECTS_LV2=ON` — and `-D AU_MODULE_EFFECTS_NYQUIST=ON`: LV2 plugin hosting and
  the Nyquist effects, both compiled from the tarball's copies.
- `-D AU_USE_SBSMS=ON` — and `-D AU_USE_SOUNDTOUCH=ON`: the SBSMS time-stretching and SoundTouch
  pitch-and-tempo effects.
- `-D AU_USE_PORTMIXER=ON` — the in-tree PortMixer, which reads and sets the input and output
  volume of the selected audio device.
- `-D AU_RUN_LRELEASE=OFF` — compile no interface translations; the
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)
  keeps the package English only, and `build.sh` also deletes every `.qm` catalogue the install
  puts under a `locale` directory.
- `-D_LARGEFILE64_SOURCE` — added to `CXXFLAGS`. musl declares the 64-bit file-offset names
  (`stat64`, `off64_t`, `lseek64`) only under this macro, which `_GNU_SOURCE` does not imply (see
  [a glibc-only call on musl](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-glibc-only-call-on-musl)).

## Patches

- `sqlite-helpers-cstdint.patch` — Adds `#include <cstdint>` to `au3/libraries/au3-sqlite-helpers/sqlite/Statement.h`, which names `int64_t` without it. The tree's GCC C++ library headers do not bring `<cstdint>` in on the way to their own (see [a standard header the source never includes](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-standard-header-the-source-never-includes)).
- `beats-formatter-pow10.patch` — Renames the file-local `pow10()` in `src/uicomponents/components/internal/numeric/beatsformatter.cpp` to `powerOf10()`. musl's `<math.h>` declares the GNU extension `double pow10(double)` under `_GNU_SOURCE`, which g++ always defines, and every call with a `size_t` argument is then ambiguous between the two (see [a project function named like a C library extension](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-project-function-named-like-a-c-library-extension)).
- `vst-stub-include.patch` — In `src/stubs/vst/vsteffectsstubmodule.cpp`, includes the stub's own header, `vsteffectsstubmodule.h`, instead of the real module's `vsteffectsmodule.h`. With `AU_MODULE_EFFECTS_VST=OFF` the stub is compiled, and the real module's header is not on its include path; the stub's header declares the same class (see [a stub that includes the real module's header](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-stub-that-includes-the-real-modules-header)).
- `wayland-platform.patch` — Removes the block of `src/app/main.cpp` that, on Linux, sets `QT_QPA_PLATFORMTHEME=gtk3` (unless `AU_QT_QPA_PLATFORM` is `offscreen`) and, unless `AU_QT_QPA_PLATFORM` is set, `QT_QPA_PLATFORM=xcb`. Unpatched, Audacity is always an Xwayland window with the GTK 3 platform theme; patched, Qt follows the session's `QT_QPA_PLATFORM` and `QT_QPA_PLATFORMTHEME` (see [a graphical application](../../../../docs/kdos/05-developer/writing-ports.md#a-graphical-application)), so the window is a native Wayland one in the desktop's colours.

## Notes

The engine applies its own patches to the dependency sources with `git apply`, so
[git](#port-git) is a build dependency.

`build.sh` writes its own `org.audacityteam.Audacity.desktop`. Audacity sets its desktop file name
to `org.audacityteam.Audacity.desktop`, so its Wayland app id is `org.audacityteam.Audacity`, which
the entry repeats in `StartupWMClass`. The entry claims only `application/x-audacity`, Audacity's
own project type, and leaves audio files to the music player.

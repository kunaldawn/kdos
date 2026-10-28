## About

MuseScore Studio is a music notation program: scores are written on the staff, played back with
a built-in synthesiser, and printed or exported as PDF, MusicXML, MIDI and audio. The interface is
Qt 6 and QML on upstream's "muse" framework, which Audacity 4 also uses. The package installs the
program as `mscore` with a `musescore` link beside it, and ships the MS Basic soundfont that
playback uses.

## Flags

- `-D CMAKE_SKIP_RPATH=ON` — write no `RPATH` into any binary, at build or at install; the
  libraries are all in `/usr/lib` (see
  [what the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)).
- `-D FETCHCONTENT_FULLY_DISCONNECTED=ON` — FetchContent performs no download or update step at
  all. The MNX importer (`src/importexport/mnx`) declares two git dependencies for FetchContent:
  the mnxdom DOM library and, inside it, pboettch's JSON Schema validator. They are the recipe's second and third
  sources, pinned to the commits upstream names (`_mnxdom` and `_jsv` in
  `kpkgbuild`).
- `-D FETCHCONTENT_SOURCE_DIR_MNXDOM="$SRC_ROOT/mnxdom-$_mnxdom"` — and
  `FETCHCONTENT_SOURCE_DIR_JSON_SCHEMA_VALIDATOR`: point each declaration at its unpacked tarball,
  so FetchContent uses that tree and clones nothing.
- `-D USE_SYSTEM_NLOHMANN_JSON=ON` — mnxdom's switch: take the JSON library from the
  [nlohmann-json](#port-nlohmann-json) port instead of downloading it.
  `-D USE_SYSTEM_JSON_SCHEMA_VALIDATOR=OFF` builds the validator from its own source, which is not
  a port.
- `-D mnxdom_BUILD_TESTING=OFF` — and `JSON_VALIDATOR_BUILD_TESTS`, `JSON_VALIDATOR_BUILD_EXAMPLES`:
  no tests or examples of the two fetched libraries.
- `-D JSON_VALIDATOR_INSTALL=OFF` — the validator installs nothing. It and mnxdom are built as
  static libraries inside the program.
- `-D MUSESCORE_BUILD_CONFIGURATION=app` — the desktop application, upstream's default
  configuration.
- `-D MUSE_APP_BUILD_MODE=release` — a stable release; upstream's default is `dev`, the
  development and nightly mode.
- `-D MUSE_ENABLE_UNIT_TESTS=OFF` — no unit tests.
- `-D MUSE_COMPILE_USE_COMPILER_CACHE=OFF` — upstream's default tries ccache, sccache and
  buildcache in turn and puts the first it finds in front of the compiler; off, the tree's compiler
  is called directly.
- `-D MUSE_COMPILE_USE_UNITY=OFF` — compile every source file as its own translation unit.
  Upstream's default is a unity build, which merges each module's sources, twelve at a time, into
  one.
- `-D MUSE_MODULE_UPDATE=OFF` — and `MUSE_MODULE_CLOUD`, `MUSE_MODULE_LEARN`,
  `MUSE_MODULE_MUSESAMPLER`, `MUE_BUILD_MUSESOUNDS_MODULE`: the update checker, the MuseScore.com
  account and cloud scores, the Learn page, and the Muse Sounds and Muse Sampler integration
  (proprietary instruments delivered online). Upstream's stubs take their place.
- `-D MUE_BUILD_MUSESOUNDS_QML=ON` — the MuseSounds QML target, whose default follows
  `MUE_BUILD_MUSESOUNDS_MODULE`. The app shell's QML module names that target as a dependency, so
  with it off the configure stops with `Argument musesounds_qml is not a target!`; on, the stub
  builds its placeholder page under the same target (see
  [a stub whose QML target is off with its module](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-stub-whose-qml-target-is-off-with-its-module)).
- `-D MUSE_MODULE_DIAGNOSTICS_CRASHPAD_CLIENT=OFF` — no Crashpad crash-reporting client.
- `-D MUSE_MODULE_VST=OFF` — no VST3 hosting, which needs Steinberg's SDK.
- `-D MUSE_MODULE_AUDIO_JACK=OFF` — no JACK driver. `-D MUSE_MODULE_AUDIO_PIPEWIRE=ON` builds the
  native PipeWire driver, off by default upstream; the ALSA driver is built beside it. A
  `libpipewire-0.3` the configure cannot find only prints a warning and drops the PipeWire driver,
  so [pipewire](#port-pipewire) is in `depends`.
- `-D MUE_DOWNLOAD_SOUNDFONT=OFF` — no download of the latest soundfont during the build.
  `-D MUE_INSTALL_SOUNDFONT=ON` installs the `MS Basic.sf3` that the release tree carries under
  `share/sound`.
- `-D MUE_RUN_LRELEASE=OFF` — compile no interface translations; the package is English only
  under the
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy),
  and `build.sh` deletes any `.qm` catalogue the install puts under a `locale` directory.
- `-D MUE_COMPILE_USE_SYSTEM_FLAC=ON` — and the `OPUS`, `OPUSENC`, `LAME`, `HARFBUZZ`, `FREETYPE`,
  `PUGIXML` and `UTF8CPP` switches: link each of these libraries from its port instead of
  compiling upstream's bundled copy. HarfBuzz must be found: when it is not, `SetupHarfBuzz.cmake`
  downloads a build recipe for it from upstream's `muse_deps` repository, which the offline build
  cannot do, so [harfbuzz](#port-harfbuzz) is in `depends`.
- `-D MUE_COMPILE_USE_SYSTEM_MNXDOM=OFF` — build mnxdom through FetchContent from the recipe's
  source; mnxdom is not a port.
- `-D_LARGEFILE64_SOURCE` — added to `CXXFLAGS`: musl declares the 64-bit file-offset names
  (`stat64`, `off64_t`, `lseek64`) only under this macro, which `_GNU_SOURCE` does not imply (see
  [a glibc-only call on musl](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-glibc-only-call-on-musl)).

## Patches

- `fix-destdir-symlinks.patch` — Upstream's pull request 33819. The install rules in `src/app/CMakeLists.txt` (Linux and FreeBSD) and `buildscripts/packaging/Linux+BSD/SetupAppImagePackaging.cmake` create the `musescore` link to `mscore` and the `musescore` manual page's link to `mscore`'s with `cmake -E create_symlink` at a path built from `CMAKE_INSTALL_PREFIX` alone. The patch puts `$ENV{DESTDIR}` in front of the link's own path (the target stays `/usr/...`), so the links land in the package instead of in the build root's `/usr`.
- `wayland-platform.patch` — Removes the block of `src/app/main.cpp` that, on Linux, sets `QT_QPA_PLATFORMTHEME=gtk3` (unless `MU_QT_QPA_PLATFORM` is `offscreen`) and, unless `MU_QT_QPA_PLATFORM` is set, `QT_QPA_PLATFORM=xcb`. Unpatched, MuseScore is always an Xwayland window with the GTK 3 platform theme; patched, Qt follows the session's `QT_QPA_PLATFORM` and `QT_QPA_PLATFORMTHEME` (see [a graphical application](../../../../docs/kdos/05-developer/writing-ports.md#a-graphical-application)).

## Notes

After the install, `build.sh` deletes `/usr/include`, `/usr/lib/pkgconfig` and every static
archive under `/usr/lib`: they come from mnxdom's own install rules, a static library, its headers
and a `mnxdom.pc` that nothing on the system links against.

The recipe writes its own `org.musescore.MuseScore.desktop`. MuseScore sets its desktop file name
to `org.musescore.MuseScore`, which is its Wayland app id and the entry's `StartupWMClass`. The
entry claims MuseScore's score formats and the notation formats it imports, and leaves MIDI files
and soundfonts to the players and synthesisers.

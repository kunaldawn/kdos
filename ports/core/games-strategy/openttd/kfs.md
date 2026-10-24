## About

[OpenTTD]{.idx} is an open-source engine for Transport Tycoon Deluxe: the player builds rail,
road, air and sea networks and runs a transport company over a century of play, alone, against AI
companies or with others over the network. The engine needs base sets of graphics, sounds and
music to start; the free ones are the [openttd-opengfx](#port-openttd-opengfx),
[openttd-opensfx](#port-openttd-opensfx) and [openttd-openmsx](#port-openttd-openmsx) ports in
`depends`, so the game runs without the original game's files.

## Flags

- `-DCMAKE_INSTALL_BINDIR=bin` — install `openttd` into `/usr/bin`. Upstream sets its own default,
  `games`, which would put it in `/usr/games`.
- `-DCMAKE_INSTALL_DATADIR=share/games` — with `-DOPTION_INSTALL_FHS=ON`: the FHS layout, with the
  data in `/usr/share/games/openttd`, where the base-set ports install. `share/games` is upstream's
  own default and FHS is its default on Linux; both are named.
- `-DOPTION_DEDICATED=OFF` — build the game with its interface, not a dedicated server only.
  Upstream's default, named.
- `-DOPTION_USE_ASSERTS=OFF` — no assertions, as upstream recommends for a stable release. Its
  default, named.
- `-DCMAKE_DISABLE_FIND_PACKAGE_Allegro=ON` — with the same switch for `unofficial-breakpad`,
  `Grfcodec` and `Doxygen`: never look for these four. Allegro is an alternative video and sound
  backend; Breakpad is a crash reporter; grfcodec and nforenum would rebuild the engine's own
  `openttd.grf` and `orig_extra.grf` from their sources instead of using the prebuilt ones in the
  release; and Doxygen adds a target for the source documentation. None of the first three is a
  port, and the switches keep the build from changing when one appears in the build root.
- `-DCMAKE_REQUIRE_FIND_PACKAGE_SDL2=ON` — and the same switch for `Freetype`, `Fontconfig`,
  `Harfbuzz`, `ICU`, `Fluidsynth`, `OpusFile`, `PNG`, `ZLIB`, `LibLZMA`, `LZO`, `CURL` and
  `OpenGL`: make each of those lookups required. Upstream looks for every one of them as optional
  and builds without the feature when it is missing: the SDL2 video driver, scalable fonts and
  their lookup, text shaping and line breaking for complex scripts, MIDI music through
  [fluidsynth](#port-fluidsynth), Opus sound files, PNG screenshots and heightmaps, the zlib, LZMA
  and LZO save-game formats, HTTP for downloads from the online content service, and OpenGL
  drawing.
- `-DCURL_NO_CURL_CMAKE=ON` — keep CMake's FindCURL from first looking for a `CURLConfig.cmake`,
  which the tree's curl does not install; with the lookup required, that search would itself fail
  the configure (see
  [A graphical application](../../../../docs/kdos/05-developer/writing-ports.md#a-graphical-application)).
- `-DCMAKE_EXE_LINKER_FLAGS` — the tree's `LDFLAGS` plus `-Wl,-z,stack-size=1048576`. musl gives a
  new thread 128 KiB of stack unless the executable asks for more, and OpenTTD's pathfinder and
  script threads need more than that; the linker flag records a 1 MiB request in the program.

## Patches

- `fix-segfault.patch` — From upstream pull request 15962. In `src/gfx_layout_icu.cpp`, keeps the `icu::UnicodeString` handed to the ICU line-break iterator in a named variable instead of a temporary. The iterator keeps a reference to the text, and a temporary is destroyed at the end of the statement, before the iterator reads it, so laying out a line that needs breaking could crash.
- `musl-getrandom.patch` — In `src/core/random_func.cpp`, takes `getrandom()` from `<sys/random.h>` on any Linux, rather than only when `__GLIBC__` says glibc 2.25 or later. musl defines no `__GLIBC__`, so unpatched the game falls back to its own non-cryptographic random generator and warns that no strong one is available.
- `no-survey.patch` — In `src/network/network_survey.h`, makes `IsSurveyPossible()` return `false`. That removes the opt-in usage survey, its settings button and the question about it at the first network game, so the program never offers to send a survey report.

## Notes

`build.sh` deletes every language file under `/usr/share/games/openttd/lang` except the English
ones. OpenTTD's `.lng` files are its translations, which kpkg's
[language prune](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)
does not recognise. The in-game list then offers English alone, and a configuration that names
another language falls back to English.

Upstream's desktop entry carries no window class, so `build.sh` writes its own over it with
`StartupWMClass=openttd`, the window's Wayland app id, which is the program's name.

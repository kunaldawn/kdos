## About

GoldenDict-ng looks words up in local dictionary files: StarDict, DSL, MDict, Babylon, ZIM and
others, with [hunspell](#port-hunspell) morphology to find a word's base form. It can also
pronounce words through Qt's speech and multimedia modules and pop up a translation for selected
text. The program is called `goldendict`.

## Flags

- `-Wl,-z,stack-size=8388608`, added to `LDFLAGS` — ask for an 8 MiB stack in the program's
  `PT_GNU_STACK` header, the size a thread gets by default on glibc, where upstream develops. musl
  gives each new thread 128 KiB unless that header asks for more, and the pooled thread that loads
  dictionaries overruns it in `Dsl::makeDictionaries`, whose DSL scanner alone keeps a 64 KiB read
  buffer on the stack: the program crashes at start, before its window.
- `-D CMAKE_SKIP_INSTALL_RPATH=ON` — Install the program with no `RPATH`; every library it loads is
  in `/usr/lib`.
- `-D WITH_EPWING_SUPPORT=OFF` — No EPWING dictionaries. They need libeb, which is not a port, and
  upstream turns the option on by default and then requires the library.
- `-D WITH_FFMPEG_PLAYER=OFF` — No FFmpeg audio player. Upstream's own description calls it useless
  on Qt 6.8 and later, where Qt Multimedia does the same job.
- `-D WITH_QT_MULTIMEDIA=ON` — Play pronunciations through [qt6-qtmultimedia](#port-qt6-qtmultimedia).
  It is upstream's default.
- `-D WITH_ZIM=ON` — Read ZIM archives, such as offline Wikipedia, through [libzim](#port-libzim).
  It is upstream's default.
- `-D WITH_TTS=ON` — Speak words with [qt6-qtspeech](#port-qt6-qtspeech)'s text to speech. Off by
  default upstream.
- `-D WITH_X11=ON` — Build the X11 half of the program: global hotkeys and the selection popup
  through libX11 and libXtst, which work for X11 windows under Xwayland. It is upstream's default,
  and upstream marks turning it off, a pure Wayland build, as experimental.
- `-D USE_ALTERNATIVE_NAME=OFF` — Keep the program name `goldendict` and the data directory
  `/usr/share/goldendict`, rather than `goldendict-ng`. It is upstream's default.

## Patches

- `offline-defaults.patch` — Changes three first-run defaults in `src/config.cc` that reach the network: `checkForNewReleases` from true to false, the preset dict.org server from enabled to disabled, and the preset English Wikipedia and English Wiktionary sources (`makeDefaultMediaWikis`) from enabled to disabled. Each stays one checkbox away in Preferences and in the Dictionaries dialog.

## Notes

`build.sh` removes `/usr/share/goldendict/locale`; English is the program's untranslated source
text. Upstream's menu entry is replaced: `setDesktopFileName` makes the Wayland app_id
`io.github.xiaoyifang.goldendict_ng`, which the entry gives as `StartupWMClass`.

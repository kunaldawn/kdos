## About

Kanagram is KDE Education's anagram game: it scrambles a word from a vocabulary list and the
player puts the letters back in order, with a hint and a spoken pronunciation on request. The
lists are KVTML documents read through [libkeduvocdocument](#port-libkeduvocdocument), and the
player can write new ones in the game's own editor.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6TextToSpeech=ON` — Stop the configure if Qt Text to Speech
  from [qt6-qtspeech](#port-qt6-qtspeech) is missing. Upstream looks for it quietly and, without
  it, builds a game that cannot say the word.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The word lists are not in this package. Kanagram reads the shared ones that
[kdeedu-data](#port-kdeedu-data) installs under `/usr/share/apps/kvtml/<language>`, and starts
with no vocabulary without it, so that port is in `depends` for run time. More lists come
through [knewstuff](#port-knewstuff) when the player asks for them.

The game screen is QML that imports `QtMultimedia` for its sounds; CMake does not check for it,
so [qt6-qtmultimedia](#port-qt6-qtmultimedia) is named in `depends`.

Upstream's `CMakeLists.txt` sets `CMAKE_CXX_FLAGS_RELEASE` to `-Wall -Wno-switch -O2`, replacing
ECM's `-O3 -DNDEBUG` for the Release build. The tree's `CXXFLAGS` still come first, so Kanagram
builds at `-O2` with `assert()` compiled in. A plain `set()` in the project overrides a cache
value given with `-D`, so no flag changes this.

`KAboutData` gives the window the app_id `org.kde.kanagram`, the name of upstream's entry.

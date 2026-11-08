## About

KHangMan is KDE Education's hangman game for learning vocabulary: it picks a word from a
vocabulary list, the player guesses its letters, and a hint shows the meaning on request. The
lists are KVTML documents read through [libkeduvocdocument](#port-libkeduvocdocument), and for
28 languages the game shows the special characters of the alphabet as buttons.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — Stop the configure if
  [kdoctools](#port-kdoctools) is missing. Upstream marks it optional and, without it, skips the
  `doc/` directory and the handbook with it.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The word lists are [kdeedu-data](#port-kdeedu-data)'s, read from
`/usr/share/apps/kvtml/<language>`; upstream's `README_packagers` requires shipping at least its
English set, so that port is in `depends`. The per-language files this package installs under
`/usr/share/khangman` are each alphabet's special characters, data the language rule keeps. More
lists come through [knewstuff](#port-knewstuff).

The interface is QML that imports `org.kde.kirigami`, the kirigami-addons components, delegates
and formcard modules, `org.kde.newstuff` and `QtMultimedia`. CMake checks for none of them, so
[kirigami](#port-kirigami), [kirigami-addons](#port-kirigami-addons),
[knewstuff](#port-knewstuff) and [qt6-qtmultimedia](#port-qt6-qtmultimedia) are named in
`depends`. The two fonts go to `/usr/share/khangman/fonts`, where only the game loads them.

`KAboutData` gives the window the app_id `org.kde.khangman`, the name of upstream's entry.

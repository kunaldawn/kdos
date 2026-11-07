## About

libkdegames is the library under the KDE games: the themed card decks and the game theme
selector, high-score tables, the difficulty setting, the standard game actions and menus, the
QGraphicsView and QtQuick rendering helpers, network game discovery over DNS-SD, and the game
sound engine, which plays effects through [openal-soft](#port-openal-soft) and decodes them with
[libsndfile](#port-libsndfile).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

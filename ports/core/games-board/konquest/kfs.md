## About

Konquest is a galactic strategy game for two to ten players, human or computer: each turn a player
sends fleets from the planets they hold to take the others, and whoever holds the galaxy at the end
wins. It is a Qt 6 and KDE Frameworks application built on [libkdegames](#port-libkdegames).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D KONQUEST_EXAMPLE_PLAYER=OFF` — Leave out the example computer player, a template for writing
  new ones that upstream compiles in only behind this switch. Upstream's default, named.

## Notes

The game's turn sequence runs on Qt State Machine, which this tree builds in
[qt6-qtscxml](#port-qt6-qtscxml); there is no separate state-machine port.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes `/usr/share/doc/HTML/<lang>/`
from the package.

The desktop entry is upstream's `org.kde.konquest.desktop`. `KAboutData` derives the window's
Wayland app id, `org.kde.konquest`, from the component name and the `apps.kde.org` home page, so it
is the entry's file name and Plasma pairs the window with the entry without a `StartupWMClass`.

## About

KsirK is a world-domination strategy game in the style of Risk: players, human or computer, on one
machine or over the network, take turns placing armies, invading neighbouring countries and moving
troops, each after their own mission. Maps are skins, and the package carries a second program,
`ksirkskineditor`, for drawing them. It is a Qt 6 and KDE Frameworks application built on
[libkdegames](#port-libkdegames).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The configure requires Qt Test even with `BUILD_TESTING=OFF`, and Qt 5 Compat, whose SAX reader
(`QXmlSimpleReader`) loads saved games; [qt6-qtbase](#port-qt6-qtbase) builds the first and
[qt6-qt5compat](#port-qt6-qt5compat) is the second. The game's sounds play through
[qt6-qtmultimedia](#port-qt6-qtmultimedia). New skins are fetched with
[knewstuff](#port-knewstuff), and only when the player asks for them.

A network game runs over the `KGame` classes of libkdegames' private library and is hosted on TCP
port 20000 by default, which the input chain of `/etc/nftables.conf` drops, so hosting needs a rule
accepting that port in a file of its own under `/etc/nftables.d`; joining someone else's game is an
outgoing connection and needs nothing.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes `/usr/share/doc/HTML/<lang>/`
from the package.

The two desktop entries are upstream's `org.kde.ksirk.desktop` and
`org.kde.ksirkskineditor.desktop`. `KAboutData` derives each window's Wayland app id from its
component name and the `apps.kde.org` home page, `org.kde.ksirk` and `org.kde.ksirkskineditor`, so
each is its entry's file name and Plasma pairs the windows with the entries without a
`StartupWMClass`.

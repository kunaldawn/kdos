## About

KNetWalk is a puzzle for system administrators: every cable piece on the board is turned until each
terminal is connected to the server, in as few turns as possible. The board is drawn in Qt Quick
inside a Qt Widgets window, from the QML types [libkdegames](#port-libkdegames) installs as
`org.kde.games.core`.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D CMAKE_DISABLE_FIND_PACKAGE_7Zip=ON` — Compress the themes' SVGs to SVGZ with gzip, never with
  7-Zip. Upstream takes 7-Zip whenever it finds one, and [7zip](#port-7zip) is in the build root
  from `41_system` on, so the tool would follow whatever happens to be installed. Refused, the build
  runs `gzip -9 -n`, which upstream then requires and which stores no file name or time stamp in the
  output; [gzip](#port-gzip) is in `depends`.

## Notes

The configure requires Qt Test even with `BUILD_TESTING=OFF`; [qt6-qtbase](#port-qt6-qtbase)
builds it.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes `/usr/share/doc/HTML/<lang>/`
from the package.

The desktop entry is upstream's `org.kde.knetwalk.desktop`. `KAboutData` derives the window's
Wayland app id, `org.kde.knetwalk`, from the component name and the `apps.kde.org` home page, so it
is the entry's file name and Plasma pairs the window with the entry without a `StartupWMClass`.

## About

Picmi is a nonogram game: the numbers beside each row and column give the runs of filled cells in
it, and the player works out from them which cells hold the hidden picture. It has random boards of
several sizes and a set of preset levels with pictures. It is a Qt 6 and KDE Frameworks application
built on [libkdegames](#port-libkdegames).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D CMAKE_DISABLE_FIND_PACKAGE_7Zip=ON` — Compress the theme's SVG to SVGZ with gzip, never with
  7-Zip. Upstream takes 7-Zip whenever it finds one, and [7zip](#port-7zip) is in the build root
  from `41_system` on, so the tool would follow whatever happens to be installed. Refused, the build
  runs `gzip -9 -n`, which upstream then requires and which stores no file name or time stamp in the
  output; [gzip](#port-gzip) is in `depends`.

## Notes

The configure requires Qt Test even with `BUILD_TESTING=OFF`, and `test/` builds its one unit
test, `streaks_test`, whatever the switch says; the test is compiled and never installed.
[qt6-qtbase](#port-qt6-qtbase) builds Qt Test.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes `/usr/share/doc/HTML/<lang>/`
from the package.

The desktop entry is upstream's `org.kde.picmi.desktop`. `KAboutData` derives the window's Wayland
app id, `org.kde.picmi`, from the component name and the `apps.kde.org` home page, so it is the
entry's file name and Plasma pairs the window with the entry without a `StartupWMClass`.

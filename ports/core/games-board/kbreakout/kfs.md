## About

KBreakOut is a Breakout game: keep the ball in play with the paddle, break every brick, and catch
the gifts that fall out of some of them. The playing field is a Qt Quick scene inside a Widgets
window, and the levels are one level set, `/usr/share/kbreakout/levelsets/default.levelset`.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
  `kdoctools_install()` still builds the handbook in every translated language, and kpkg's
  language rule removes `/usr/share/doc/HTML/<lang>/` from the package.
- `-D CMAKE_DISABLE_FIND_PACKAGE_7Zip=ON` — Compresses the theme SVGs to SVGZ with gzip.
  Upstream's `cmake/InternalMacros.cmake` uses 7-Zip's `7zz` whenever it finds one and otherwise
  requires gzip; [7zip](#port-7zip) is built in an earlier phase, so without the switch the package
  would follow whatever the build root holds. gzip runs with `-n`, which writes no file name or time
  stamp into the SVGZ files.

## Notes

The scene imports `org.kde.games.core`, the QML module [libkdegames](#port-libkdegames) installs,
and `QtQuick` from [qt6-qtdeclarative](#port-qt6-qtdeclarative). Neither import is checked at build
time; a missing one leaves the window empty at run time.

The window's Wayland app_id is `org.kde.kbreakout`: `KAboutData` builds that desktop file name from
the component name and its organisation domain, `kde.org`. It is the desktop entry's file name too,
and upstream's entry sets no `StartupWMClass`, so the task manager pairs the window with its entry
without a patch.

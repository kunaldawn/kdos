## About

KSquares is Dots and Boxes: players take turns drawing a line between two neighbouring dots, and
whoever closes a square scores it and draws again. Up to four players share the board, people or
computer players of three strengths. It is a Qt 6 and KDE Frameworks application built on
[libkdegames](#port-libkdegames).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as an optional package: without it the
  `doc` directory is skipped with only a line in the feature summary, and Help opens nothing.
  Required, a missing kdoctools stops the configure.

## Notes

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes `/usr/share/doc/HTML/<lang>/`
from the package.

The desktop entry is upstream's `org.kde.ksquares.desktop`. `KAboutData` derives the window's
Wayland app id, `org.kde.ksquares`, from the component name and the `apps.kde.org` home page, so it
is the entry's file name and Plasma pairs the window with the entry without a `StartupWMClass`.

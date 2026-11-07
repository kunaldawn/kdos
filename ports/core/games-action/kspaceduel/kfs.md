## About

KSpaceDuel is a two-player space arcade game: two satellites circle a sun whose gravity pulls on
them and on every bullet, and each tries to destroy the other before its energy runs out. The second
player is a person at the same keyboard or the computer. It is a Qt 6 and KDE Frameworks
application built on [libkdegames](#port-libkdegames).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes `/usr/share/doc/HTML/<lang>/`
from the package.

The desktop entry is upstream's `org.kde.kspaceduel.desktop`. `KAboutData` derives the window's
Wayland app id, `org.kde.kspaceduel`, from the component name and the `apps.kde.org` home page, so
it is the entry's file name and Plasma pairs the window with the entry without a `StartupWMClass`.

## About

KSnakeDuel is a light-cycle race: two snakes race across the field leaving a wall behind them, and
the first to crash into a wall loses. A second player shares the keyboard, or the computer takes
the other snake. It is a Qt 6 and KDE Frameworks application built on
[libkdegames](#port-libkdegames).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The themes' SVGs are installed as SVGZ, which upstream always compresses with `gzip` on Linux and
requires at configure time; [gzip](#port-gzip) is in `depends`.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes `/usr/share/doc/HTML/<lang>/`
from the package.

The desktop entry is upstream's `org.kde.ksnakeduel.desktop`. `KAboutData` derives the window's
Wayland app id, `org.kde.ksnakeduel`, from the component name and the `apps.kde.org` home page, so
it is the entry's file name and Plasma pairs the window with the entry without a `StartupWMClass`.

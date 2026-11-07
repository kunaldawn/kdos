## About

Shisen-Sho (`kshisen`) is a solitaire tile game in the Mahjongg family: two matching tiles are
removed when a path of at most three straight lines, around the other tiles, joins them, and the
board is cleared pair by pair. It draws its tiles and backgrounds from
[libkmahjongg](#port-libkmahjongg) and is built on [libkdegames](#port-libkdegames).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes `/usr/share/doc/HTML/<lang>/`
from the package.

The desktop entry is upstream's `org.kde.kshisen.desktop`. `KAboutData` derives the window's
Wayland app id, `org.kde.kshisen`, from the component name and the `apps.kde.org` home page, so it
is the entry's file name and Plasma pairs the window with the entry without a `StartupWMClass`.

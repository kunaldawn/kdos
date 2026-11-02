## About

Kollision is a casual arcade game: the player's blue ball follows the pointer and has to stay clear
of the red balls bouncing around the field, whose number grows the longer the round lasts. It is a
Qt 6 and KDE Frameworks application built on [libkdegames](#port-libkdegames).

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

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes `/usr/share/doc/HTML/<lang>/`
from the package.

The desktop entry is upstream's `org.kde.kollision.desktop`. `KAboutData` derives the window's
Wayland app id, `org.kde.kollision`, from the component name and the `apps.kde.org` home page, so it
is the entry's file name and Plasma pairs the window with the entry without a `StartupWMClass`.

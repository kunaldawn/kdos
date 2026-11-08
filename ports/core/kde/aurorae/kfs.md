## About

Aurorae is the window decoration engine for themes that are files rather than code. Its two
[kdecoration](#port-kdecoration) plugins are loaded by [kwin](#port-kwin): `org.kde.kwin.aurorae.v2`
draws a title bar and its buttons from a theme's SVG files, and `org.kde.kwin.aurorae` runs a
decoration written in QML, of which the port ships one, Plastik. When the configured decoration
does not load, KWin tries Breeze and then `org.kde.kwin.aurorae`, so a window keeps a frame and
buttons even with no other decoration installed. The port also installs the theme's settings page,
which the Window Decorations page of System Settings opens.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Compiles none of the translation catalogues. kpkg removes
  every non-English catalogue from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

SVG themes are read from `aurorae/themes/` under the data directories, so a theme another port
installs to `/usr/share/aurorae/themes/<name>` appears in the list with nothing to register.

The port installs `aurorae.knsrc`, the KNewStuff source for downloading themes, and
`/usr/lib/libexec/plasma-apply-aurorae`, the script that source runs to apply a downloaded theme
through KWin's `kwin-applywindowdecoration` helper. Neither is reached on KDOS, where the `ghns`
Kiosk restriction in the session's `kdeglobals` disables every "Get New…" button, so nothing is
downloaded at run time. The script is a `#!/usr/bin/bash` script, so [bash](#port-bash) is in
`depends`. [knewstuff](#port-knewstuff) itself is a build requirement of the settings page and
stays.

The decoration QML imports `org.kde.kirigami` from [kirigami](#port-kirigami) and `org.kde.ksvg`
from [ksvg](#port-ksvg) at run time; the build requires KSvg and does not look for Kirigami.

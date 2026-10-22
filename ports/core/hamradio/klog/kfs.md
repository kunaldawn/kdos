## About

KLog is an amateur radio logbook: it logs contacts, tracks DXCC and other awards, imports and
exports ADIF, exchanges confirmations with LoTW and eQSL, and shows a DX cluster. It is built
against Qt 6 and reads the radio's frequency and mode through [hamlib](#port-hamlib).

## Flags

- `-DKLOG_COVERAGE=OFF` — Build without test coverage instrumentation, which upstream turns on
  by default.

## Patches

- `no-update-check.patch` — Make `checkNewVersions` default to false in `src/mainwindow.cpp`, both at start-up and when the settings are read, and leave its check box in `src/setuppages/setuppagemisc.cpp` unticked by default. KLog otherwise asks its server for a newer release at every start; the box on the settings page still turns the check on.

## Notes

Upstream installs its icon as an SVG under hicolor's `scalable` directory and as a single
256-pixel PNG under `/usr/share/pixmaps`. `build.sh` adds hicolor PNGs at the sizes in its loop,
rasterised from `src/klog.svg`, and writes the desktop entry over upstream's, which names no
window class. Qt builds the Wayland
app id from the reversed organisation domain, `klog.xyz`, and the program name, so the entry's
`StartupWMClass` is `xyz.klog.klog`.

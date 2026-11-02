## About

KDebugSettings turns the Qt logging categories of KDE programs on and off. It lists every
category the installed packages declare in `/usr/share/qlogging-categories6`, groups them by
program, and writes the user's choices to `~/.config/QtProject/qtlogging.ini`, which every Qt
program reads at start.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

`USE_DBUS` stays at its Linux default, on: the single-instance behaviour goes through
[kdbusaddons](#port-kdbusaddons) and [kwindowsystem](#port-kwindowsystem), and
KDSingleApplication, which the switch's other side requires, is left out. `BUILD_QUICK_VERSION`,
an unfinished Qt Quick front end, stays off by default.

A changed category takes effect when a program starts; one already running keeps its old rules.

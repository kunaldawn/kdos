## About

Lokalize is KDE's computer-aided translation tool: it edits Gettext PO and XLIFF files one entry
at a time, suggests translations from a translation memory kept in SQLite and from a glossary,
checks spelling through [sonnet](#port-sonnet) and runs quality checks, and manages a whole
project's files at once. Word stems for the memory's fuzzy matching come from
[hunspell](#port-hunspell), which upstream requires.

## Flags

- `-D BUILD_DOC=ON` — Build the handbook through [kdoctools](#port-kdoctools), which is then a
  required package. On by default upstream; named so the handbook cannot drop out.
- `-D USE_DBUS=ON` — The D-Bus interface that lets scripts and other programs open files and query
  the translation memory, with [kdbusaddons](#port-kdbusaddons). On by default on Linux; named so
  it stays on.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Translating an OpenDocument file runs `odf2xliff` and `xliff2odf` from translate-toolkit, looked
for on `PATH` when the action is used; translate-toolkit is not a port, so that action reports the
missing package and everything else works. The unit tests in `autotests` are compiled whatever
`BUILD_TESTING` says and are not installed.

Lokalize's `main()` sets `QT_QPA_PLATFORM=xcb` on Linux, over the session's `wayland;xcb`,
because its several `QMainWindow`s misbehave under Wayland (KDE bugs 424024 and 477704). It
always runs as an X11 client on Xwayland.

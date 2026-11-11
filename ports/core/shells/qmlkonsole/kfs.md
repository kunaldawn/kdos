## About

QMLKonsole is the Plasma Mobile terminal: a Kirigami window around a terminal widget, with a
toolbar of the keys a phone keyboard lacks (Tab, Ctrl, Esc, the arrows) and saved commands. The
widget is a copy of qmltermwidget in `lib/`, which carries its own copy of Konsole's emulation
code and runs the shell through [kpty](#port-kpty). On KDOS the terminal is
[konsole](#port-konsole); this port is the touch-screen alternative.

## Flags

- `--set-name` — Rename the entry `Terminal (QMLKonsole)` with `desktop-file-edit` from
  [desktop-file-utils](#port-desktop-file-utils), on the installed copy. Upstream calls it plain
  `Terminal`, and the plain name of a role belongs to the program that fills it; an alternative
  is qualified (see
  [Desktop entries belong to the port](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port)).
  `GenericName=Terminal` stays, so a search for "terminal" still lists it.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

`desktop-file-edit` rewrites only `Name=`; the translated `Name[…]=` lines keep upstream's
wording, which an English-only system never shows. The entry carries
`X-KDE-AuthorizeAction=shell_access`, so a kiosk profile that denies shell access hides it.

`build.sh` writes `/usr/share/kdos/xdg/qmlkonsolerc`, a KDOS default that sets `colorScheme` to
`KDOS-Bone`. The widget looks a scheme up by file name in its own resources, then
`~/.local/share/konsole` and `/usr/share/konsole`; upstream's default, `Breeze`, is built into
Konsole and is in none of them, so without the file the widget falls back to its black-on-white
scheme, on which the shell prompt `kdos theme` colours for a dark accent is unreadable.
`kdos theme` writes the accent's scheme into the person's `qmlkonsolerc` while the value in force
names a `KDOS-` scheme, so the terminal follows the accent from its next start.

[qt6-qt5compat](#port-qt6-qt5compat) is a build requirement (`Core5Compat`): the bundled widget
still uses Qt 5 compatibility classes. `KAboutData` gives the window the app_id
`org.kde.qmlkonsole`, the name of upstream's entry.

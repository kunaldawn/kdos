## About

KCharSelect is KDE's character table: it browses every Unicode block in any installed font,
searches characters by name or code point, shows each one's Unicode properties, and copies a
selection to the clipboard. The table itself is the `KCharSelect` widget of
[kwidgetsaddons](#port-kwidgetsaddons); this port is the window around it, with bookmarks for
favourite characters.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional; required, a missing one
  stops the configure instead of leaving the handbook out.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The desktop entry's icon is the theme name `accessories-character-map`, which
[breeze-icons](#port-breeze-icons) draws; the PNGs in the source become only the Windows and macOS
application icons.

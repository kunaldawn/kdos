## About

KEditBookmarks edits a KBookmarks file: folders, separators, sorting, import from Mozilla,
Netscape, Opera, Internet Explorer and Galeon files and export to HTML. With no argument it opens
the shared `~/.local/share/konqueror/bookmarks.xml`; Konsole, Kate and the others start it on
their own file from their "Edit Bookmarks…" menu entry, which does nothing without this port. The
port also installs `kbookmarkmerger`, which merges a directory of bookmark fragments into a file.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional; required, a missing one
  stops the configure instead of leaving the handbook out.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_X11=ON` — Make [libX11](#port-libX11) a requirement. Upstream
  uses X11 only to raise an already running copy with `KX11Extras::activateWindow`, which does
  nothing under Wayland, but [kwindowsystem](#port-kwindowsystem) is built with its X11 half and
  its CMake package looks X11 up itself, so the search cannot be turned off: refused, it takes
  KWindowSystem with it and the configure stops. Required, the X11 code is always compiled in.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The desktop entry is `NoDisplay=true` upstream, since the editor is opened from an application's
Bookmarks menu.

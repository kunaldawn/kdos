## About

flamp sends files to many stations at once over the Amateur Multicast Protocol, cutting each file
into numbered blocks that a receiving station can fill in from later repeats. It transmits and
receives through [fldigi](#port-fldigi), over XML-RPC. It is an FLTK program.

## Flags

- `--with-flxmlrpc` — Link the [flxmlrpc](#port-flxmlrpc) port instead of the XML-RPC copy in
  `src/xmlrpcpp`, and fail the configure if it is missing.

## Patches

- `x11-hints-on-x11-only.patch` — In `src/flamp.cxx`, make the Xlib calls that set the icon's transparency mask after the main window opens only when `fl_x11_display()` reports an open X11 display. FLTK chooses Wayland or X11 when the program starts, and under Wayland there is no X11 display: `fl_display` is NULL.

## Notes

Upstream's icon is an XPM and its desktop entry carries no window class, so `build.sh` converts
the icon to a hicolor PNG and replaces the entry. FLTK takes the Wayland app id from the main
window's X class, `flamp`.

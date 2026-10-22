## About

flmsg composes and reads amateur radio message forms, among them ICS 213 and the other incident
command forms, the hospital ICS forms, the ARRL Radiogram, IARU and MARS messages and plain text, and hands them to
[fldigi](#port-fldigi) over XML-RPC for transmission. It is an FLTK program.

## Flags

- `--with-flxmlrpc` — Link the [flxmlrpc](#port-flxmlrpc) port instead of the XML-RPC copy in
  `src/xmlrpcpp`, and fail the configure if it is missing.

## Patches

- `x11-hints-on-x11-only.patch` — In `src/flmsg.cxx`, make the Xlib calls that set the icon's transparency mask after the main window opens only when `fl_x11_display()` reports an open X11 display. FLTK chooses Wayland or X11 when the program starts, and under Wayland there is no X11 display: `fl_display` is NULL.
- `font-browser-pthread.patch` — Add `#include <pthread.h>` to `src/widgets/font_browser.cxx`. The font browser starts a thread, and none of the headers it includes reaches `<pthread.h>` (see [A standard header the source never includes](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-standard-header-the-source-never-includes)).

## Notes

Upstream's icon is an XPM and its desktop entry carries no window class, so `build.sh` converts
the icon to a hicolor PNG and replaces the entry. FLTK takes the Wayland app id from the main
window's X class, `flmsg`.

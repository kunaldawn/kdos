## About

libX11 (Xlib) is the core X Window System client library: it opens a connection to an X server,
here over [libxcb](#port-libxcb), and provides the requests, events, keyboard mapping, input
methods and compose tables an X client uses. The one X server on KDOS is Xwayland, which serves
X clients beside the Wayland desktop, and libX11 is what those programs, and the X11 backends of Mesa, GTK, Qt and
mpv beside their Wayland ones, link. See [No Xorg server, and one carve-out](../../../../docs/kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out).

## Flags

- `--disable-specs` — Skip the specifications under `specs/` (the Xlib manual, the XIM and
  locale documents, the compose-sequence charts), which upstream builds from DocBook by default.
- `--without-xmlto`, `--without-fop`, `--without-xsltproc` — Do not look for the DocBook
  tools those documents are built with, so the configure result does not depend on whether they
  are in the build root.
- `--without-perl` — Do not use Perl. libX11 runs it only for `nls/compose-check.pl`, a check of
  the compose tables under `make check`, and for the compose-chart generator of the specs; the
  library and its manual pages are built without it.

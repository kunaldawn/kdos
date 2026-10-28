## About

libXi is the client library for the X Input extension, versions 1 and 2 (XInput2): per-device
events, raw motion, smooth scrolling, multi-touch and pointer barriers. GTK, Qt and SDL use XInput2 for touch and high-resolution scrolling when they run as X clients on Xwayland. See
[No Xorg server, and one carve-out](../../../../docs/kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out).

## Flags

- `--disable-docs`, `--disable-specs` — Skip the XInput library specifications under `specs/`,
  which upstream builds from DocBook by default. `--disable-docs` switches nothing else off in
  this release: no Makefile reads it.
- `--without-xmlto`, `--without-fop`, `--without-xsltproc`, `--without-asciidoc` — Do not look
  for the document tools, so the configure result does not depend on whether they are in the
  build root. The manual pages ship in the tarball as `.man` files and install either way; only
  with both xmlto and asciidoc found does the build gain the rules that remake them from their
  AsciiDoc `.txt` sources.

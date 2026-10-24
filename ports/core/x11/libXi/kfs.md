## About

libXi is the client library for the X Input extension, versions 1 and 2 (XInput2): per-device
events, raw motion, smooth scrolling, multi-touch and pointer barriers. GTK, Qt and SDL use XInput2 for touch and high-resolution scrolling when they run as X clients on Xwayland. See
[No Xorg server, and one carve-out](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out).

## Flags

- `--disable-docs`, `--disable-specs` — Skip the documentation and the XInput specifications
  that upstream builds from AsciiDoc and DocBook by default. The manual pages ship prebuilt in
  the tarball as `.man` files and install either way.
- `--without-xmlto`, `--without-fop`, `--without-xsltproc`, `--without-asciidoc` — Do not look
  for the document tools, so the configure result does not depend on whether they are in the
  build root.

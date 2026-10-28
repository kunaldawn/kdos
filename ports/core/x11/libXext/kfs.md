## About

libXext is the client library for a group of small X extensions: SHAPE (non-rectangular
windows), MIT-SHM (images passed through shared memory), XSync, DPMS and a few others. Almost every X client links it, which on KDOS means the programs that run on Xwayland and
the X11 backends of the toolkits and Mesa. See [No Xorg server, and one carve-out](../../../../docs/kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out).

## Flags

- `--disable-specs` — Skip the extension specifications under `specs/`, which upstream builds
  from DocBook by default.
- `--without-xmlto`, `--without-fop`, `--without-xsltproc` — Do not look for the DocBook tools,
  so the configure result does not depend on whether they are in the build root.

## About

cairo is the 2D vector graphics library: paths, fills, strokes, gradients, text and images,
rendered through [pixman](#port-pixman) to memory or written out as PNG, PDF, PostScript and SVG.
GTK, pango, librsvg, poppler, WebKitGTK, Inkscape and GStreamer's overlays draw with it. This build
carries its image, PDF, PS, SVG and script surfaces, the FreeType and fontconfig font backend,
`cairo-gobject`, and the Xlib and XCB surfaces that the X11 backends of GTK 3 and 4 need.

## Flags

- `-frandom-seed=cairo` — With LTO, fixes the name GCC gives each object's `.gnu.lto_*` sections,
  which is otherwise random on every build. cairo is one of the hot libraries the tree builds with
  link-time optimisation, and every one of them carries the seed (see [Every
  port](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#every-port)).
- `-D b_lto=true` — Link-time optimisation, through meson's own switch.
- `-D fontconfig=enabled`, `-D freetype=enabled` — The FreeType font backend (`cairo-ft`), with
  fontconfig to find fonts; pango and every GTK program render their text through it.
- `-D png=enabled`, `-D zlib=enabled` — The PNG and SVG surfaces, and the script, PostScript, PDF
  and XML surfaces, which compress with zlib.
- `-D lzo=enabled` — Lets the cairo-script interpreter compress and expand the string data of a
  script trace with LZO, through [lzo](#port-lzo).
- `-D glib=enabled` — Builds `cairo-gobject`, the GObject types for cairo's structures that pango,
  GTK and the introspection data are built on.
- `-D tee=enabled` — Builds the tee surface, which sends every drawing operation to several
  surfaces at once, and installs `cairo-tee.pc`.
- `-D xlib=enabled`, `-D xcb=enabled`, `-D xlib-xcb=enabled` — The Xlib and XCB surfaces. They
  install `cairo-xlib.pc` and `cairo-xcb.pc`, and GTK 3 and 4 stop at setup without them when their
  X11 backend is built; FLTK, cairomm and Ardour draw on Xlib surfaces under Xwayland. `xlib-xcb`,
  which upstream leaves disabled, adds the Xlib/XCB functions and `cairo-xlib-xcb.pc`.
- `-D dwrite=disabled`, `-D quartz=disabled` — The Windows DirectWrite and macOS Quartz backends;
  neither can build on Linux.
- `-D spectre=disabled` — Only cairo's test suite uses [libspectre](#port-libspectre), to rasterise
  PostScript output for comparison; with the tests off, the switch spares the probe and nothing
  else.
- `-D symbol-lookup=disabled` — On `auto`, configure finds binutils' `libbfd` in the root and links
  it into `libcairo` itself, for symbol names in the debugging utilities' backtraces.
- `-D gtk2-utils=disabled` — Builds no GTK 2 debugging utilities; GTK 2 is not a port.
- `-D gtk_doc=false`, `-D tests=disabled` — No API reference and no test suite.
- `XML_CATALOG_FILES` — Points libxml2 at `/etc/xml/catalog`. With `gtk_doc=false` no step of this
  build runs an XML tool, so it has no effect here.

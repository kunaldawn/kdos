## About

Graphviz lays out and draws graphs described in the DOT language: `dot` for hierarchies, `neato`,
`fdp` and `sfdp` for force-directed layouts, `circo` and `twopi`, and the `cgraph` and `gvc`
libraries that programs link to do the same. Output formats are plugins under
`/usr/lib/graphviz`. In the tree, [gramps](#port-gramps) draws family trees with it and
[yosys](#port-yosys) draws circuits.

## Flags

- `--disable-man-pdfs` — Make no PDF copies of the manual pages. Left at its default, `auto`,
  configure makes them whenever `groff` and `ps2pdf` happen to be installed.
- `--enable-ltdl`, `--disable-ltdl-install`, `--without-included-ltdl` — Load the plugins on
  demand through libltdl, and use the libltdl of [libtool](#port-libtool) rather than building or
  installing the copy bundled in the source.
- `--enable-swig=no` and `--enable-sharp=no` to `--enable-tcl=no` — Build no language bindings:
  no SWIG wrappers for C#, D, Go, Guile, Java, JavaScript, Lua, Perl, PHP, Python, R, Ruby or Tcl.
- `--without-x` — No X11 support, which only adds the `-Txlib` preview window.
- `--with-ghostscript=no` — No Ghostscript plugin. configure builds it only together with Xrender,
  which `--without-x` leaves out.
- `--with-expat=yes`, `--with-pangocairo=yes`, `--with-freetype2=yes`, `--with-libgd=yes`,
  `--with-webp=yes`, `--with-rsvg=yes`, `--with-poppler=yes` — The XML parser for HTML-like labels
  and the renderers: Cairo with Pango for PNG, PDF, SVG and PostScript output,
  [libgd](#port-libgd) for its bitmap formats, WebP, SVG images in nodes through
  [librsvg](#port-librsvg), and PDF images in nodes through [poppler](#port-poppler).
- `--with-gts=yes` — Link the GNU Triangulated Surface library, [gts](#port-gts), into the neato
  layout plugin. It is what gives `sfdp` and `neato` the `overlap=prism` overlap removal.
- `--with-sfdp=yes`, `--with-ortho=yes`, `--with-digcola=yes`, `--with-ipsepcola=yes` — The
  `sfdp` layout, orthogonal edge routing, and neato's hierarchical (`mode=hier`) and constrained
  (`mode=ipsep`) layouts.
- `--with-gdk-pixbuf=no`, `--with-gdk=no` — No GdkPixbuf or GDK plugins; the gd, rsvg and
  poppler plugins already load the image formats they would.
- `--with-gtk=no`, `--with-gtkgl=no`, `--with-gtkglext=no`, `--with-glade=no`, `--with-qt=no`,
  `--with-glut=no`, `--with-smyrna=no` — No graphical programs: `gvedit` is a Qt program and
  `smyrna` an OpenGL viewer built on GTK and GLUT.
- `--with-lasi=no`, `--with-devil=no`, `--with-ann=no`, `--with-aalib=no` — No LASi PostScript
  text plugin, no DevIL image plugin, no ANN library (used by the `mingle` edge-bundling tool) and
  no AAlib ASCII-art output. LASi, DevIL and ANN are not ports.

## Notes

Every renderer is a pkg-config probe in configure that drops its plugin without a word when the
library is missing, even when asked for with `=yes`. `build.sh` therefore checks after the install
that the core, dot, neato, pango, gd, rsvg, poppler and webp plugins exist, and that the neato
plugin links libgts; a missing one fails the build.

The plugin index, `/usr/lib/graphviz/config8`, is written in `build.sh` with `dot -c`, run against
the staged plugins through `GVBINDIR`. Upstream's install hook runs `dot -c` only when `DESTDIR` is
empty, and without the index `dot` loads no plugin and knows no output format. Every plugin is in
this one package, so the index is complete when the package is built.

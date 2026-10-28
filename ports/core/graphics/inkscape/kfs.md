## About

Inkscape is a vector graphics editor whose native format is SVG. It draws and edits paths,
shapes, text and gradients, traces bitmaps, imports PDF, EPS, CorelDRAW, Visio and WordPerfect
graphics, and exports PNG, PDF, PostScript and more, from its window or from `inkscape` on the
command line. The port builds it on GTK 3 through gtkmm, with the Python extensions that ship with
it.

## Flags

- `-D BUILD_SHARED_LIBS=ON` — Builds Inkscape's internal library, `inkscape_base`, as a shared
  library that the program links; upstream's default, named.
- `-D WITH_INTERNAL_2GEOM=OFF` — Uses the tree's [lib2geom](#port-lib2geom) instead of the copy
  in the tarball; upstream's default, named.
- `-D ENABLE_POPPLER=ON`, `-D ENABLE_POPPLER_CAIRO=ON` — PDF import through
  [poppler](#port-poppler), and its Cairo renderer for the import dialog's preview. The
  `poppler-*` patches are what keep this importer compiling against the tree's poppler.
- `-D WITH_IMAGE_MAGICK=OFF`, `-D WITH_GRAPHICS_MAGICK=ON` — The raster effects under
  Extensions > Raster, and reading an embedded image's resolution, link Magick++ from
  [graphicsmagick](#port-graphicsmagick). Upstream's ImageMagick probe asks for ImageMagick 6;
  its option is off so the probe is not made, and without either library the raster effects are
  absent.
- `-D ENABLE_LCMS=ON`, `-D WITH_LIBCDR=ON`, `-D WITH_LIBVISIO=ON`, `-D WITH_LIBWPG=ON`,
  `-D WITH_GSPELL=ON`, `-D WITH_GSOURCEVIEW=ON`, `-D WITH_GNU_READLINE=ON`, `-D WITH_OPENMP=ON`,
  `-D WITH_SVG2=ON`, `-D WITH_X11=ON` — Upstream's defaults, named: colour management through
  lcms2, the CorelDRAW, Visio and WordPerfect importers, spell checking, syntax colouring in the
  XML and CSS editors, readline in `inkscape --shell`, OpenMP threads for filters, the SVG 2
  features, and GTK 3's X11 half for running under Xwayland.
- `-D WITH_NLS=OFF` — Builds no translations; the image is English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D WITH_JEMALLOC=OFF` — The C library's allocator rather than jemalloc; upstream's default,
  named.
- `-D WITH_MANPAGE_COMPRESSION=OFF` — Installs the manual pages uncompressed, like every other
  page on the image. Upstream gzips them whenever `gzip` is found.
- `-D ENABLE_BINRELOC=OFF` — Fixed install paths rather than ones found relative to the program
  at run time; upstream's default, named.

## Notes

Upstream looks for gspell, gtksourceview, the CorelDRAW, Visio and WordPerfect libraries,
readline, OpenMP and the Magick++ library and, when one is not found, turns its option off with
no error. Naming the option `ON` does not make the library required;
it is the library's entry in `depends` that keeps the feature in the package.

The patches are upstream's own commits for poppler releases newer than this Inkscape release,
and the loop in `build.sh` applies them in the order of their commit dates, which puts the
version-check correction before the last of the release fixes. The PDF importer uses poppler's
private headers, whose interface changes with every poppler release, so a poppler update can
need another such patch.

After the install, `build.sh` removes the extension manager, `extensions/inkman`, which installs
extensions from the network into a private Python environment and fails on a machine with no
network; the translated manual-page directories; and every translated tutorial file, any name
with a language before its extension (`tutorial-<name>.<language>.svg` and translated screenshots
such as `potrace.<language>.png`), keeping the English `tutorial-<name>.svg` and its images.

Upstream's desktop entry is replaced by one with `StartupWMClass=org.inkscape.Inkscape`: GTK 3
takes the Wayland app_id from the program name, which Inkscape sets to its application id. The
hicolor PNG icons it names are upstream's.

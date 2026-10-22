## About

GD is a C library for drawing images in code: lines, shapes, fills, text through FreeType, scaling
and filters, read from and written to PNG, JPEG, GIF, WebP, TIFF, HEIF, AVIF and BMP. In KDOS,
gnuplot's PNG, GIF and JPEG terminals, graphviz's GD renderer, libsixel, libgphoto2, ptouch-print
and TeX Live build on it.

## Flags

- `--disable-rpath` — Hardcodes no run-time library path into the library and its programs. With
  every library in `/usr/lib`, the default search path, such an entry adds nothing, and the package
  lint warns on it (see [What the package lint
  refuses](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)).
- `--disable-werror` — Keeps `-Werror` out of the compile. configure adds it for a GCC build of a
  git checkout; the release tarball has no `.git`, and the switch keeps it off whatever the tree.
- `--enable-gd-formats` — Builds the GD and GD2 file formats, GD's own uncompressed and chunked
  image formats. Upstream's default is off.
- `--without-x` — Skips configure's search for X11 include and library paths, which it runs for
  FreeType and XPM.
- `--with-zlib`, `--with-png`, `--with-jpeg`, `--with-webp`, `--with-tiff`, `--with-heif`,
  `--with-avif`, `--with-freetype`, `--with-fontconfig` — Each format and the text path through its
  port. With `--with-<name>` given, configure stops with "requested but not found" when the library
  is missing; on its default, `auto`, it would build without that format.
- `--with-raqm` — Lays out text drawn with `gdImageStringFT` through [libraqm](#port-libraqm),
  which adds bidirectional text and complex-script shaping with HarfBuzz.
- `--with-liq` — Converts a true-colour image to a palette (`gdImageTrueColorToPalette`) with
  [libimagequant](#port-libimagequant), instead of GD's own quantiser.
- `--without-xpm` — Builds no XPM reader. [libXpm](#port-libXpm) is a port but not in this port's
  `depends`, so the switch keeps the result independent of build order.
- `ax_cv_c_openmp=unknown` — Answers configure's OpenMP check in advance with "no flag found", so
  `OPENMP_CFLAGS` is empty and the libimagequant link carries no `-fopenmp`.

## Patches

- `heif-stride.patch` — Reads and writes the interleaved RGBA plane of a HEIF image row by row at the stride libheif reports, in both `_gdImageCreateFromHeifCtx` and `_gdImageHeifCtx` in `src/gd_heif.c`, where upstream passes `NULL` for the stride and walks the plane as one packed run of `width * 4` bytes. libheif answers `heif_image_get_plane_readonly()` with `NULL` when the stride pointer is `NULL`, so without the patch every HEIF read and every HEIF write fails with "gd-heif cannot get image plane"; and libheif may pad each row, so a packed walk would shear the picture.

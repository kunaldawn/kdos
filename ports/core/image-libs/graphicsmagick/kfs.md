## About

[GraphicsMagick]{.idx} is the image-processing library and `gm` command that forked from
ImageMagick, kept in KDOS for Magick++, its C++ API. Octave's `imread`, `imwrite` and `imfinfo` are
built on it, because Octave needs the ImageMagick 6 API (`Magick::PixelPacket`), which
GraphicsMagick keeps and ImageMagick 7 dropped; inkscape, timg and xastir link it too. The
[imagemagick](#port-imagemagick) port is a separate program and installs its own commands.

## Flags

- `LDFLAGS=` — Run configure with an empty `LDFLAGS`, and pass the tree's flags to `make` instead.
  The configure script writes its `LDFLAGS` into the `Libs.private` line of `GraphicsMagick.pc`, so
  a consumer that links statically would inherit the tree's linker flags; given to `make`, they
  apply to this build alone.
- `--enable-openmp` — Run the resize, convolution and colour-conversion loops in parallel with
  OpenMP, over gcc's `libgomp`.
- `--without-modules` — Link every coder into the library rather than build each as a loadable
  module.
- `--with-quantum-depth=16` — Store 16 bits per sample instead of the default 8, so 16-bit images,
  Octave's `uint16` among them, keep their full range.
- `--with-magick-plus-plus` — Build Magick++, the C++ library Octave links.
- `--without-perl` — Build no PerlMagick binding.
- `--disable-magick-compat` — Install no ImageMagick-named commands (`convert`, `identify` and the
  rest), which upstream offers as shortcuts to `gm`, so nothing collides with the imagemagick port.
  It is upstream's default, named.
- `--with-threads` — With `--with-bzlib`, `--with-zlib`, `--with-zstd`, `--with-lzma`, `--with-png`,
  `--with-jpeg`, `--with-jp2`, `--with-jxl`, `--with-jbig`, `--with-tiff`, `--with-webp`,
  `--with-heif`, `--with-lcms2`, `--with-ttf`, `--with-xml`, `--with-libzip` and `--with-gs`, build
  every coder and helper whose library is a port: JPEG 2000 here is JasPer's (`--with-jp2`),
  TrueType is FreeType's, and `--with-gs` lets it render PostScript and PDF through ghostscript.
  Each is a probe that disables its coder when the library is missing, so each library is in
  `depends`.
- `--without-fpx` — With `--without-wmf` and `--without-trio`, build no FlashPIX or WMF coder and no
  TRIO printf replacement; libfpx, libwmf and trio are not ports.

## Notes

The configure script drops OpenMP without an error when the compiler refuses `-fopenmp`, so
`build.sh` checks the installed `libGraphicsMagick.so` with `readelf` for a `NEEDED` entry on
`libgomp`, and likewise on `libjasper`, the one coder library it checks by name. It also checks that
`libGraphicsMagick++.so` and `GraphicsMagick++.pc` were installed, since Octave builds against them.

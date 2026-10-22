## About

libtiff reads and writes [TIFF]{.idx}, the format scanners, fax software and document feeders write,
and installs the classic tools: `tiffinfo`, `tiffcp`, `tiffsplit`, `tiff2pdf`, `tiff2ps`, `fax2tiff`
and the rest. leptonica, and through it tesseract, reads multi-page TIFF through it; ghostscript,
poppler, imagemagick, gimp, gdk-pixbuf and Qt's image-format plugins link it.

## Flags

- `-DCMAKE_REQUIRE_FIND_PACKAGE_ZLIB=ON` — With `CMAKE_REQUIRE_FIND_PACKAGE_Deflate`,
  `_libjpeg-turbo`, `_liblzma`, `_ZSTD` and `_JBIG`, stop the configure when a codec library is
  missing. Every codec option defaults to whether its library was found, and even set to `ON` it
  takes effect only when the search succeeded. The requirements turn a missing library into an
  error instead of a libtiff that quietly reads fewer kinds of file.
- `-Dzlib=ON` — Deflate (ZIP) compression, through zlib.
- `-Dpixarlog=ON` — Pixar's log-encoded compression, which also needs zlib.
- `-Dlibdeflate=ON` — Use [libdeflate](#port-libdeflate) for faster Deflate coding; zlib is still
  required beside it.
- `-Djpeg=ON` — JPEG compression inside TIFF, through libjpeg-turbo.
- `-Dold-jpeg=ON` — Read the obsolete "old-style" JPEG compression some old files carry; it is
  read-only.
- `-Dlzma=ON` — LZMA compression, through xz's liblzma.
- `-Dzstd=ON` — Zstandard compression.
- `-Djbig=ON` — JBIG compression, through [jbigkit](#port-jbigkit).
- `-Dwebp=OFF` — Build no WebP codec. [libwebp](#port-libwebp) depends on libtiff for its TIFF
  input, so a WebP codec here would close the two into a dependency cycle.
- `-Dlerc=OFF` — Build no LERC codec; LibLerc is not a port.
- `-Dtiff-opengl=OFF` — Build no `tiffgt`, an OpenGL and GLUT viewer.
- `-Dsphinx=OFF` — Do not build the manual with Sphinx. The release carries the same pages already
  built, in `doc/man-prebuilt`, and `build.sh` installs those.
- `-Dtiff-tests=OFF` — Build no test programs.

## Notes

The prebuilt section-1 pages cover every tool in the source tree, including `tiffgt` (not built
here) and `rgb2ycbcr` and `thumbnail`, which upstream builds but never installs. `build.sh` therefore
copies a page only when `/usr/bin` in the package has the program it describes. All the section-3 pages (`*.3tiff`) are installed.

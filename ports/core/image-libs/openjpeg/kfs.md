## About

OpenJPEG is the [JPEG 2000]{.idx} codec, `libopenjp2`, with the `opj_compress`, `opj_decompress` and
`opj_dump` tools. poppler and mupdf decode the JPEG 2000 images inside PDFs through it, and
imagemagick, gimp, ghostscript, leptonica, libheif and the web engines link it.

## Flags

- `-DBUILD_CODEC=ON` — Build the `opj_*` command-line tools. This is upstream's default, named in
  the recipe.
- `-DBUILD_THIRDPARTY=OFF` — Do not build the copies of zlib, libpng, libtiff and lcms2 that the
  source tree carries under `thirdparty/`; the tools link the system libraries instead.
- `-DCMAKE_REQUIRE_FIND_PACKAGE_ZLIB=ON` — With `CMAKE_REQUIRE_FIND_PACKAGE_PNG`, `_TIFF` and
  `_LCMS2`, stop the configure when one of the tools' libraries is missing. `opj_compress` and
  `opj_decompress` read and write PNG and TIFF and apply ICC profiles through lcms2, and each
  library is looked up with a `find_package` that drops the format without an error when it is not
  found.

## Notes

CMake installs no manual pages for the project, so `build.sh` copies the three tool pages and
`libopenjp2(3)` from `doc/man` itself.

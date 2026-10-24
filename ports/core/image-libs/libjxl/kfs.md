## About

libjxl is the reference implementation of [JPEG XL]{.idx}: `libjxl` and `libjxl_threads`,
`libjxl_cms`, the `cjxl` and `djxl` converters, and a gdk-pixbuf loader, so GTK programs open `.jxl`
files. ffmpeg, imagemagick, gimp, krita, the web engines and KDE's kimageformats link it.

## Flags

- `-DJPEGXL_FORCE_SYSTEM_HWY=ON` — With `JPEGXL_FORCE_SYSTEM_BROTLI` and
  `JPEGXL_FORCE_SYSTEM_LCMS2`, take highway, brotli and lcms2 from the system. The release tarball
  carries `third_party/` with every submodule empty, so there is no bundled copy to build; the
  switches make the configure look only for the ports.
- `-DJPEGXL_BUNDLE_LIBPNG=OFF` — Link the system libpng; upstream bundles it only for sanitizer and
  Emscripten builds.
- `-DJPEGXL_ENABLE_SKCMS=OFF` — Manage colour with lcms2 instead of skcms. Upstream defaults to
  skcms on a little-endian machine and to lcms2 only on a big-endian one; skcms is one of the empty
  submodules, so the switch is needed here.
- `-DJPEGXL_ENABLE_SJPEG=OFF` — Build without sjpeg, an alternative JPEG encoder for `cjxl`, also an
  empty submodule.
- `-DJPEGXL_ENABLE_OPENEXR=ON` — Let the tools read and write OpenEXR.
- `-DJPEGXL_ENABLE_TRANSCODE_JPEG=ON` — With `JPEGXL_ENABLE_BOXES`, keep two decoder features that
  upstream builds by default: reconstructing the original JPEG from a losslessly recompressed file,
  and reading the metadata boxes in a file.
- `-DJPEGXL_ENABLE_TOOLS=ON` — Build `cjxl` and `djxl`.
- `-DJPEGXL_ENABLE_MANPAGES=ON` — Build their manual pages, with asciidoc's `a2x`; `asciidoc` is a
  dependency for this.
- `-DJPEGXL_ENABLE_DOXYGEN=OFF` — With `_BENCHMARK`, `_EXAMPLES`, `_DEVTOOLS`, `_VIEWERS`,
  `_FUZZERS`, `_JNI`, `_TCMALLOC` and `_COVERAGE` all `OFF`, build no API reference, benchmark,
  examples, developer tools, viewers, fuzzers, Java binding, tcmalloc allocator or coverage
  instrumentation. Several are on by default, and the Java binding and tcmalloc are built whenever
  their dependencies happen to be installed.
- `-DJPEGXL_ENABLE_PLUGINS=ON` — With `-DJPEGXL_ENABLE_PLUGIN_GDKPIXBUF=ON`, build the gdk-pixbuf
  loader; upstream builds no plugins by default.
- `-DJPEGXL_ENABLE_PLUGIN_MIME=OFF` — Install no `image-jxl.xml`. shared-mime-info's own database
  already defines `image/jxl`.
- `-DJPEGXL_WARNINGS_AS_ERRORS=OFF` — Upstream's default, named.
- `-DCMAKE_REQUIRE_FIND_PACKAGE_GIF=ON` — With `_JPEG` and `_PNG`, stop the configure if giflib,
  libjpeg-turbo or libpng is missing. `cjxl` and `djxl` would otherwise build without that input or
  output format and say nothing.

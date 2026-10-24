## About
[MuPDF]{.idx} is a library and set of programs for PDF, XPS, EPUB, FictionBook, comic book
archives and images. The port installs `libmupdf`, `mutool` (split, merge, draw, extract, clean,
sign, run scripts, and dump text a `grep` can reach), and two viewers: `mupdf-gl`, the full one
with search, annotations, forms and a file dialog, and `mupdf-x11`, a plain Xlib viewer. Both
viewers are X11 programs and run under Xwayland. [aerc](#port-aerc) names the port in
`depends`.

## Flags
- `USE_SYSTEM_LIBS=yes` — Turn on the `USE_SYSTEM_*` default of most libraries MuPDF keeps a
  copy of in `thirdparty/`, so they are linked from the system: freetype, harfbuzz, jbig2dec,
  libjpeg, openjpeg, zlib, curl, brotli, leptonica, tesseract, zxing-cpp (with the `zint` it
  links), freeglut, all ports, and gumbo-parser, which
  the next line turns back off. A library that is not installed is a build that
  stops on a missing header a few files in. The defaults it leaves off are lcms2, MuJS, JPEG XR
  and cmark-gfm; Artifex's own `extract` library has no system switch. libarchive has no bundled
  copy and is always the system's.
- `USE_SYSTEM_GUMBO=no`, `USE_SYSTEM_MUJS=no`, `USE_SYSTEM_LCMS2=no` — Keep three bundled
  copies: gumbo-parser (the HTML5 parser of the EPUB and HTML reader), MuJS (the JavaScript
  interpreter of PDF forms and `mutool run`) and lcms2mt (MuPDF's thread-safe variant of Little
  CMS). Only the first changes the build: `Makethird` already leaves MuJS off ("not available")
  and lcms2 off ("lcms2mt is strongly preferred"), and the other two lines state it. MuJS is not
  a port; [gumbo-parser](#port-gumbo-parser) and [lcms2](#port-lcms2) are, for other programs.
- `USE_SYSTEM_ZXINGCPP=yes`, `USE_SYSTEM_GLUT=yes` — Two defaults of `USE_SYSTEM_LIBS`, stated.
  The system GLUT is [freeglut](#port-freeglut), which declares `GLUT_API_VERSION` 4; MuPDF's
  own freeglut fork declares 6. `mupdf-gl`'s clipboard and its Unicode keyboard input are
  compiled only for version 6, so this build has no clipboard and reads keys as Latin-1.
- `barcode=yes` — Build `mutool barcode`, which decodes QR codes and barcodes from a page or an
  image and draws new ones, through [zxing-cpp](#port-zxing-cpp).
- `tesseract=yes` — Build OCR output, `mutool draw` with an `.ocr.pdf`, `.ocr.txt` or other
  `.ocr.*` output file, on [tesseract](#port-tesseract) and [leptonica](#port-leptonica).
- `archive=yes` — Open CBR, CB7 and tar comic archives through [libarchive](#port-libarchive).
  Like `barcode` and `tesseract`, it stops the build when its library is missing.
- `HAVE_LIBCRYPTO=yes` — Build PDF signing and signature checking on
  [OpenSSL](#port-openssl)'s libcrypto. Left unset, it is a pkg-config probe that drops both
  without a word when libcrypto is not found.
- `HAVE_X11=yes`, `HAVE_GLUT=yes` — Build both viewers. Left unset, each is a pkg-config probe
  (`x11 xext` for `mupdf-x11`; `gl x11 xrandr` for `mupdf-gl`) that skips its viewer when a
  library is missing; set, a missing one is a link error.
- `HAVE_OBJCOPY=yes` — Embed the fonts and other resources into `libmupdf` by turning each file
  into an object with `ld -r -b binary`, instead of compiling the hex dumps under `generated/`.
  It is the default on Linux, stated; the hyphenation step below depends on it.
- `build=release`, `prefix=/usr`, `shared=yes` — MuPDF has no configure; its makefile takes the
  build type, the prefix and a shared `libmupdf.so` (static by default) on the command line, on
  both the build and the install `make`.
- `export CC="gcc -std=gnu11" CFLAGS="${CFLAGS/-std=gnu11/}"` — Move the tree's `-std=gnu11` out
  of `CFLAGS` and into the C compiler's name. MuPDF's makefile compiles its C++ sources (the
  tesseract and zxing-cpp glue) with `$(CXX) $(CFLAGS)`, where a C standard flag does not belong;
  see [C flags handed to the C++ compiler](../../../../docs/kdos/05-developer/build-troubleshooting.md#c-flags-handed-to-the-c-compiler).

## Notes
- **The hyphenation archives are rebuilt from source.** The tarball carries
  `resources/hyphen/hyph-std.zip` and `hyph-all.zip`, prebuilt, and a hex dump of one under
  `generated/`. `build.sh` deletes all three and writes the two archives again from the text
  patterns beside them: `hyph-std.zip` from `core/`, `hyph-all.zip` from `core/` and `extra/`,
  entries in path order under the fixed timestamp of upstream's `scripts/zip.py`, deflated, so
  the result is the same from build to build. With `HAVE_OBJCOPY` the build links the archives
  themselves, and nothing prebuilt is left to be picked up instead.
- **The fonts are upstream's.** The faces MuPDF compiles into the library are font data, one of
  the payloads [What is built from source](../../../../docs/kdos/05-developer/writing-ports.md#what-is-built-from-source)
  exempts.
- **The menu entry.** `build.sh` writes `mupdf-gl.desktop` and installs upstream's PNG icons into
  `hicolor`. The entry names no `MimeType`, so it is never offered as the opener of a file type
  and `kdos-mimeapps.list` never has to rank it. freeglut sets no `WM_CLASS` on its window, so the
  entry has no `StartupWMClass` either.

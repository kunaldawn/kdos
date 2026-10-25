## About

[gImageReader]{.idx} is a graphical front end for the [tesseract](#port-tesseract) OCR engine: it
scans a page through SANE or opens an image, a PDF or a DjVu file, recognises the text, and lets
it be corrected and spell-checked as plain text or as an hOCR document, from which it can
generate a PDF. This build is its Qt 6 interface, `gimagereader-qt6`. PDF input comes through [poppler](#port-poppler), DjVu
input through the `ddjvuapi` of [djvulibre](#port-djvulibre), and PDF output through
[podofo](#port-podofo).

## Flags

- `-D INTERFACE_TYPE=qt6` — Build the Qt 6 interface rather than the Qt 5 or GTK one. It is
  upstream's default, written out.
- `-D ENABLE_VERSIONCHECK=0` — No check at startup that asks the project's server for a newer
  release.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_QuaZip-Qt6=ON` — Require [quazip](#port-quazip)'s Qt 6 CMake
  package. Upstream looks it up quietly and falls back to a QuaZip 5 package or to a bare header
  search.

## Notes

Tesseract, PoDoFo and SANE are found by `pkg_check_modules` without `REQUIRED`, so a module
pkg-config cannot find does not stop the configuration; for Tesseract and PoDoFo upstream then
guesses an include directory and a `-l` flag. `build.sh` reads `CMakeCache.txt` after the
configuration and stops the build unless pkg-config found all three.

`build.sh` removes the translated manuals, `manual-*.html`, from the package; the English manual
stays.

`build.sh` replaces upstream's desktop entry to drop its `MimeType` line, which claims images, PDF
and HTML that the image viewers and the browser open. The Wayland app id is the executable's name,
`gimagereader-qt6`, which the entry names in `StartupWMClass`.

## About

poxml translates DocBook documentation through gettext. `xml2pot` extracts a DocBook file's
messages into a PO template, `po2xml` writes a translated DocBook file back from a PO file,
`split2po` builds a PO file from an original and a translated DocBook file, and `swappo` swaps the
message and translation of every entry in a PO file.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Passed as every KDE recipe passes it. The module calls no
  `ki18n_install()`; its `po/` holds translated manual pages, which `kdoctools_install(po)` builds
  and kpkg's language rule removes (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

The four tools link `libgettextpo` from [gettext](#port-gettext), found by the bundled
`FindGettextPO.cmake` as a requirement, and Qt's XML module. Their manual pages are written in
DocBook and built by [kdoctools](#port-kdoctools). The `find_package(Gettext)` beside the
requirement only gates the tests, which `BUILD_TESTING=OFF` leaves out.

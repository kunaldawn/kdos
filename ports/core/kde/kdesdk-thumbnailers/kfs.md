## About

kdesdk-thumbnailers holds one KIO thumbnail plugin, `pothumbnail`, which draws a gettext `.po`
catalogue's preview as a chart of its translated, fuzzy and untranslated messages. Dolphin and
every other KIO file view show it in place of a generic text icon.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_GettextPO=ON` — Make `libgettextpo` a requirement. Upstream looks
  for it quietly and leaves out `po_thumbnailer/`, the module's only plugin, when it is missing, so
  the package would install nothing. The library and `gettext-po.h` come from
  [gettext](#port-gettext).
- `-D KF_SKIP_PO_PROCESSING=ON` — Passed as every KDE recipe passes it. This module has no `po/`
  directory and calls no `ki18n_install()`, so CMake reports the variable as unused.
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

`BUILD_FUZZERS` stays at its default, off: the fuzzers need static libraries and stop the
configure in a shared build.

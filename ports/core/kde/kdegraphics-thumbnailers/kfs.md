## About

kdegraphics-thumbnailers is a set of KIO thumbnailers: PostScript, PDF and DVI pages through
[ghostscript](#port-ghostscript), Blender files from the preview they carry, camera raw files
through [libkdcraw](#port-libkdcraw) and [libkexiv2](#port-libkexiv2), and Mobipocket e-book
covers through [kdegraphics-mobipocket](#port-kdegraphics-mobipocket).

## Flags

- `-D DISABLE_BLENDER=OFF` — Builds the Blender thumbnailer, which reads `.blend` files through
  [karchive](#port-karchive). Upstream's default, named.
- `-D BUILD_FUZZERS=OFF` — No fuzzing targets. Upstream's default, named.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KExiv2Qt6=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_KDcrawQt6=ON` —
  Require both libraries of the raw thumbnailer, which upstream builds only when it finds both.

## Notes

`DISABLE_MOBIPOCKET` is left at its default, off, under which upstream requires QMobipocket6, so
a missing [kdegraphics-mobipocket](#port-kdegraphics-mobipocket) stops the configure. The
PostScript thumbnailer runs `gs` at run time, and for a DVI file first converts it with
`dvips`. The tarball has no translations, so `build.sh`
passes no translation switch.

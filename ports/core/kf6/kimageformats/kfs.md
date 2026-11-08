## About

KImageFormats is a set of Qt image format plugins, so that every Qt program that reads an image
through `QImageReader` also reads AVIF, HEIF, JPEG XL, JPEG 2000, OpenEXR, camera raw files,
DirectDraw surfaces, Krita and OpenRaster documents, Photoshop, GIMP's XCF, Radiance HDR, QOI and
a dozen older formats. [gwenview](#port-gwenview) and [kolourpaint](#port-kolourpaint) name it in
`depends`; thumbnails in Dolphin and the file dialogs use the same plugins.

## Flags

- `-D KIMAGEFORMATS_DDS=ON` — Build the DirectDraw Surface plugin. It is upstream's default,
  written out with the others so the recipe states every format it builds.
- `-D KIMAGEFORMATS_HEIF=ON` — Build the HEIF and HEIC plugin, through [libheif](#port-libheif).
  Upstream's default is off. The configure looks for libheif through pkg-config with no way to make
  it a requirement, so a missing libheif would leave the plugin out with only a line in the
  summary; it is in `depends`.
- `-D KIMAGEFORMATS_JXL=ON` — Build the JPEG XL plugin, through [libjxl](#port-libjxl), looked for
  through pkg-config in the same way. It is upstream's default.
- `-D KIMAGEFORMATS_JP2=ON` — Build the JPEG 2000 plugin, through [openjpeg](#port-openjpeg). It is
  upstream's default.
- `-D KIMAGEFORMATS_WITH_KNOWN_CRASHES_JXR=OFF` — Build no JPEG XR plugin. Upstream keeps it off
  by default and names the switch for its known crashes, and the jxrlib it needs is not a port.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Archive=ON` — Make [karchive](#port-karchive) a requirement,
  for the Krita (`.kra`) and OpenRaster (`.ora`) plugins, which read the image out of the zip
  container. Upstream looks for it as optional and leaves both formats out without it.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_libavif=ON` — Make [libavif](#port-libavif) a requirement, for
  the AVIF plugin.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_OpenEXR=ON` — Make [openexr](#port-openexr) a requirement, for
  the OpenEXR plugin.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_OpenJPEG=ON` — Make [openjpeg](#port-openjpeg) a requirement, for
  the JPEG 2000 plugin.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_LibRaw=ON` — Make [libraw](#port-libraw) a requirement, for the
  camera raw plugin.

## Notes

Each of the five libraries the `CMAKE_REQUIRE_FIND_PACKAGE_` switches name is optional upstream,
and a missing one would leave its formats out with only a line in the configure summary.

The EPS plugin is built whenever Qt Print Support is found, which it always is in
[qt6-qtbase](#port-qt6-qtbase). It renders through Ghostscript's `gs`, which it runs at read time
and without which it reads nothing, so [ghostscript](#port-ghostscript) is a run-time dependency
in `depends`.

The tarball is about 34 MB: most of it is the test images under `autotests/`, which
`BUILD_TESTING=OFF` leaves unread.

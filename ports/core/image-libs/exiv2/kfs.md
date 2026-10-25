## About

Exiv2 reads and writes image [metadata]{.idx}: EXIF, IPTC and XMP, the date, camera, lens and
position a camera writes into every photo. It is a C++ library and the `exiv2` command. gimp,
darktable, digikam, krita, gwenview, rawtherapee, qgis and gexiv2 link it.

## Flags

- `-DEXIV2_ENABLE_XMP=ON` — Build XMP support from the XMP toolkit copy in the source tree's
  `xmpsdk/`, which parses through expat; `-DEXIV2_ENABLE_EXTERNAL_XMP=OFF` is the other half, and
  the two are mutually exclusive.
- `-DEXIV2_ENABLE_PNG=ON` — With `_BMFF`, `_BROTLI`, `_VIDEO` and `_LENSDATA`, read metadata in PNG
  (through zlib), in ISO base media files such as HEIF, AVIF, CR3 and JPEG XL (whose compressed
  boxes need brotli), and in video containers, and include the Nikon lens table. All are upstream's
  defaults and are named.
- `-DEXIV2_ENABLE_INIH=ON` — Read a `.exiv2` configuration file, from the working directory or else
  the home directory, which can name lenses the built-in tables do not know, through the
  [inih](#port-inih) port. inih is looked up without
  being required, so it is in `depends`.
- `-DEXIV2_ENABLE_NLS=OFF` — Build no message catalogues.
- `-DEXIV2_ENABLE_WEBREADY=OFF` — With `-DEXIV2_ENABLE_CURL=OFF`, build no HTTP access: the library
  opens only local files, never a URL handed to it as a path.
- `-DEXIV2_BUILD_DOC=OFF` — With `_SAMPLES` and `_UNIT_TESTS` off, build no API documentation,
  sample programs or tests.
- `-DEXIV2_BUILD_EXIV2_COMMAND=ON` — Build the `exiv2` program; upstream's default, named.

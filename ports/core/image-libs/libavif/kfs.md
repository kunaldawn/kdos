## About

libavif reads and writes [AVIF]{.idx}, the still-image format built on the AV1 video codec, with the
`avifenc` and `avifdec` converters and a gdk-pixbuf loader. Like libheif it codes nothing itself:
[dav1d](#port-dav1d) decodes and [svt-av1](#port-svt-av1) encodes. The web engines, SDL's image
libraries, opencv, gdal, chafa and darktable open AVIF files through it.

## Flags

- `-DFETCHCONTENT_FULLY_DISCONNECTED=ON` — Never let CMake's FetchContent download anything.
- `-DFETCHCONTENT_SOURCE_DIR_LIBARGPARSE="$SRC_ROOT/libargparse-$_argparse"` — Build
  `avifgainmaputil`'s argument parser, libargparse, from the recipe's second source, unpacked beside
  the tree. Without it the configure clones libargparse from GitHub, which the offline build cannot
  do.
- `-DAVIF_CODEC_DAV1D=SYSTEM` — With `-DAVIF_CODEC_SVT=SYSTEM`, decode through the dav1d port and
  encode through the svt-av1 port. Each codec option takes `OFF`, `LOCAL` (a copy built in `ext/`)
  or `SYSTEM`, and every codec defaults to `OFF`.
- `-DAVIF_CODEC_AOM=OFF` — With `_LIBGAV1`, `_RAV1E` and `_AVM`, build no codec on libaom, libgav1,
  rav1e or AVM; none of them is a port.
- `-DAVIF_LIBYUV=SYSTEM` — With `AVIF_LIBSHARPYUV`, `AVIF_LIBXML2`, `AVIF_JPEG` and `AVIF_ZLIBPNG`,
  use the system's libyuv for fast colour conversion, libwebp's `libsharpyuv` for higher-quality
  conversion, libxml2 so `avifenc` can convert a JPEG with a gain map, and libjpeg-turbo, zlib and
  libpng for the tools' JPEG and PNG input and output. `AVIF_LIBSHARPYUV` and `AVIF_LIBXML2` default
  to `OFF`; the others default to `SYSTEM` and are named.
- `-DAVIF_GTEST=OFF` — With `-DAVIF_FUZZTEST=OFF`, build neither test framework.
- `-DAVIF_BUILD_APPS=ON` — Build `avifenc`, `avifdec` and `avifgainmaputil`, which upstream builds
  only when asked.
- `-DAVIF_BUILD_GDK_PIXBUF=ON` — Build the gdk-pixbuf loader.
- `-DAVIF_BUILD_MAN_PAGES=OFF` — Do not let the build render the manual pages; it would run pandoc.
  `build.sh` renders the same Markdown sources, `doc/avifenc.1.md` and `doc/avifdec.1.md`, with
  `lowdown` instead.

## Notes

The pages' pandoc title block reads `AVIFENC(1) | General Commands Manual`, which lowdown does not
take apart, so `build.sh` passes the title, the section and the source line as `-M` metadata.

The helper `_argparse` names the libargparse commit, and it must be the one libavif pins:
`AVIF_LIBARGPARSE_GIT_TAG` in `cmake/Modules/LocalLibargparse.cmake`. A new libavif release that
moves that tag needs the helper and the second source's `sha256 =` line moved with it.

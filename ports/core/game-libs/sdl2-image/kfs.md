## About

SDL2_image loads image files into SDL 2 surfaces and textures: PNG, JPEG, WebP, AVIF, JPEG XL, TIFF
and SVG, plus the older formats it decodes itself (BMP, GIF, LBM, PCX, PNM, QOI, TGA, XCF, XPM and
XV). It is built against [sdl2-compat](#port-sdl2-compat), so the SDL 2 programs that use it run on
[sdl3](#port-sdl3). Every format that takes an outside library is named on, and each of those
libraries is linked rather than loaded at run time.

## Flags

- `-DSDL2IMAGE_VENDORED=OFF` — Builds against the libraries in the root rather than upstream's own
  copies of them, which are git submodules that `external/download.sh` fetches and the release
  tarball does not contain. This is upstream's default everywhere but MSVC, named here.
- `-DSDL2IMAGE_STRICT=ON` — Stops configure when a library a named format needs is missing.
  Upstream's default is off, and a missing library then builds the library without that format.
- `-DSDL2IMAGE_DEPS_SHARED=OFF` — Links each codec library. Upstream's default loads each one with
  `dlopen` at run time instead; a dlopened library is a dependency the package solver cannot see,
  and a missing one is a format that silently stops loading; a linked one is a `NEEDED` entry.
- `-DSDL2IMAGE_BACKEND_STB=OFF` — Decodes PNG and JPEG with [libpng](#port-libpng) and
  [libjpeg-turbo](#port-libjpeg-turbo) instead of the `stb_image` code compiled into the library,
  which upstream uses by default: a private decoder that no libjpeg-turbo or libpng update reaches.
- `-DSDL2IMAGE_AVIF=ON`, `-DSDL2IMAGE_JPG=ON`, `-DSDL2IMAGE_PNG=ON`, `-DSDL2IMAGE_TIF=ON`,
  `-DSDL2IMAGE_WEBP=ON` — Formats upstream builds by default, named so that `STRICT` holds them to
  [libavif](#port-libavif), [libjpeg-turbo](#port-libjpeg-turbo), [libpng](#port-libpng),
  [libtiff](#port-libtiff) and [libwebp](#port-libwebp).
- `-DSDL2IMAGE_JXL=ON` — JPEG XL through [libjxl](#port-libjxl). Upstream's default is off.
- `-DSDL2IMAGE_SVG=ON` — SVG, rasterised by the nanosvg code compiled into the library; it takes no
  outside library.
- `-DSDL2IMAGE_SAMPLES=OFF`, `-DSDL2IMAGE_TESTS=OFF` — Builds neither the sample programs, which
  upstream builds by default when SDL2_image is the top-level project, nor the unit tests, which are
  off by default.

## Notes

The formats left at their defaults (BMP, GIF, LBM, PCX, PNM, QOI, TGA, XCF, XPM, XV) are decoded by
SDL2_image's own code and need no library. [sdl3-image](#port-sdl3-image) is the SDL 3 counterpart,
built the same way.

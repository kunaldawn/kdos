## About

libwebp is Google's [WebP]{.idx} codec: `libwebp`, the decoder-only `libwebpdecoder`, `libwebpmux`
and `libwebpdemux` for animated and annotated files, and `libsharpyuv`, its RGB to YUV converter,
which libavif and libheif also use. The tools are `cwebp`, `dwebp`, `gif2webp`, `img2webp`,
`webpinfo` and `webpmux`. The browsers, the toolkits' image loaders, imagemagick, ffmpeg and the
desktop's own `kdos-shell` and `kdos-term` link it.

## Flags

- `-DWEBP_ENABLE_SIMD=ON` — Build the SIMD code paths; upstream's default for every target but
  Emscripten, named in the recipe.
- `-DWEBP_BUILD_LIBWEBPMUX=ON` — Build `libwebpmux`, which reads and writes the chunks of an
  extended or animated WebP file. Upstream's default, named.
- `-DWEBP_BUILD_EXTRAS=ON` — Build the extras, `get_disto` and `webp_quality`; upstream installs
  neither, so the package is the same without them.
- `-DWEBP_BUILD_VWEBP=OFF` — Build neither viewer: `vwebp`, which needs OpenGL and GLUT, nor the SDL
  `vwebp_sdl` among the extras, which is built under the same switch whenever SDL2 is found.
- `-DWEBP_ENABLE_SWAP_16BIT_CSP=ON` — Write the two bytes of each pixel in the 16-bit output modes,
  RGB565 and RGBA4444, swapped: low byte first, which is the pixel's in-memory order as a
  little-endian 16-bit value. A program that asks the decoder for one of those modes sees the
  difference; the 24- and 32-bit modes are unaffected.

## Notes

`build.sh` checks the install before packaging: the CMake package file
`/usr/share/WebP/cmake/WebPConfig.cmake` must exist, and so must each of the five libraries under
the soname the loop names. A release that changes one of those sonames stops the build, instead of
leaving every program linked against the old name without its library.

`cwebp` and `img2webp` read PNG, JPEG and TIFF, `dwebp` writes PNG, and `gif2webp` reads GIF.
Each of those four libraries is found with a plain `find_package`, which leaves its format out
without an error when the library is missing; all four are in `depends` so that each is present
when the configure runs. Because libwebp depends on libtiff, libtiff is built without WebP support;
see [libtiff](#port-libtiff).

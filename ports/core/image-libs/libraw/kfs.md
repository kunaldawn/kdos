## About

LibRaw decodes [camera raw files]{.idx}: CR3, NEF, ARW, DNG and the other formats a camera writes
before any processing. The JPEG a camera writes beside a raw file is a lossy preview it made itself;
LibRaw is what gets the picture out of the raw one. darktable, rawtherapee, gthumb, imagemagick,
libvips, openimageio, gegl and KDE's libkdcraw link it.

## Flags

- `${CFLAGS/-O2/-O3}` — Compile at `-O3` instead of the tree's `-O2`. [Every
  port](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#every-port)
  says when a recipe raises the level.
- `--disable-examples` — Build none of the sample programs (`dcraw_emu`, `raw-identify` and the
  others); consumers link the library.
- `--enable-openmp` — Parallelise the demosaic algorithms and several decoders (Canon CR3,
  Fujifilm compressed, Panasonic) with OpenMP, over gcc's `libgomp`; `gcc` is in `depends` for that
  library.
- `--enable-jpeg` — With `--enable-lcms` and `--enable-zlib`, decode lossy-JPEG raws through
  libjpeg-turbo, apply colour profiles through lcms2, and read deflate-compressed DNG through zlib.

## Notes

The configure probe for OpenMP drops it without an error when the compiler refuses `-fopenmp`, so
`build.sh` checks after the install that `-fopenmp` is in `libraw.pc`, the flag the probe adds there
when it succeeds.

The source is the GitHub tag archive, which carries no generated configure script, so `build.sh`
runs `autoreconf -f -i` first; `autoconf`, `automake` and `libtool` are dependencies for that.

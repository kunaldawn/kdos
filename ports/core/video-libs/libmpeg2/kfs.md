## About

libmpeg2 decodes MPEG-1 and MPEG-2 video streams, the video of DVDs and of DVB and ATSC
broadcasts. The phase list names it for GStreamer: the `mpeg2dec` element of
[gst-plugins-ugly](#port-gst-plugins-ugly) wraps it. VLC, ScummVM and Amiberry link it as well. The package also installs `mpeg2dec`, the
demonstration decoder, which here can only write its output to files.

## Flags

- `--enable-shared` — Build `libmpeg2.so` and `libmpeg2convert.so`.
- `--with-pic` — Compile the libraries' objects position-independent.
- `--without-x` — Build `mpeg2dec` without its X11 video output. Its display outputs are X11 and
  SDL; with both off it decodes to files only.
- `--disable-sdl` — Build `mpeg2dec` without its SDL output. configure otherwise turns it on
  whenever `sdl-config` is on the path, which [sdl12-compat](#port-sdl12-compat) installs.
- `--enable-accel-detect` — Choose the processor's SIMD routines at run time rather than at build
  time. It is upstream's default.

## Patches

- `gcc-15.patch` — Gives the `getenv` declaration in the bundled `src/getopt.c` its argument list, `char *getenv (const char *)`: GCC 15 compiles C23 by default, where `getenv ()` declares a function with no arguments and conflicts with the C library's prototype. It also widens the guard in `src/getopt.h` that prototypes `getopt`, from `__GNU_LIBRARY__` alone to any C library that is not glibc, so the bundled header gives `getopt` its full prototype instead of the empty `getopt ()`, which conflicts the same way.

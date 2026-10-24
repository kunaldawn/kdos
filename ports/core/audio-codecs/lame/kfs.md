## About
[LAME]{.idx} is the MP3 encoder: the library `libmp3lame` and the `lame` command.
[ffmpeg](#port-ffmpeg), [libsndfile](#port-libsndfile), [gst-plugins-good](#port-gst-plugins-good),
[sox](#port-sox), [audacity](#port-audacity), [k3b](#port-k3b) and [handbrake](#port-handbrake)
write MP3 through the library. It decodes MP3 too, through [mpg123](#port-mpg123), for `lame
--decode`, MP3 input and its `hip_*` decoding API.

## Flags
- `-Wno-implicit-function-declaration` — with `-Wno-implicit-int`, `-Wno-int-conversion` and
  `-Wno-incompatible-pointer-types`: turn off the four diagnostics that GCC 14 and later report as
  errors by default (see
  [Newer-compiler diagnostics as errors](../../../../docs/kdos/05-developer/build-troubleshooting.md#newer-compiler-diagnostics-as-errors)).
- `--enable-static` — build `libmp3lame.a` as well as the shared library, for the `lame` program
  alone; `build.sh` deletes the archive after the install. With the decoder on, `lame` calls
  `hip_finish_pinfo`, which `libmp3lame.sym` leaves out of the shared library's exports, so `lame`
  cannot link against `libmp3lame.so`. configure links the programs
  with libtool's `-static` unless `--enable-dynamic-frontends` is given, so with the archive
  present `lame` takes the library from it.
- `--enable-nasm=no` — changes nothing in this build. Upstream's `AC_ARG_ENABLE(nasm, …)` sets
  `ASM_FOR_ARCH=i386` whenever the switch is given, `no` included. The assembly routines stay out
  because they are written for i386 alone, and the build log says
  `checking for assembler routines for this processor type... no`.
- `--enable-frontend` — build the `lame` command, configure's default, named.
- `--enable-decoder` — decode through [mpg123](#port-mpg123), configure's default, named. With it
  on, configure stops when `libmpg123` is missing instead of building without the decoder.
- `--disable-gtktest` — skip the test program configure compiles and runs against GTK 1.2, which
  only the `mp3x` frame analyser uses and which is not a port.

## Notes
The `lame` command reads the terminal's capabilities through the termcap functions of ncurses
(`tgetent`, `tgetstr`) for its console output, and configure links ncurses whenever it finds
it, so [ncurses](#port-ncurses) is in `depends`; the installed `lame` needs `libncursesw.so.6`.

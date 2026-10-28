## About

DOSBox-X is an x86 PC emulator for DOS software, for Windows 3.x and 9x, and for the NEC PC-98 and
the PCjr and Tandy machines. It aims to emulate the hardware of the period accurately rather than
only what games need. The port builds its SDL 2 front end on [sdl2-compat](#port-sdl2-compat).

## Flags

- `--enable-sdl2` — Builds against SDL 2, here [sdl2-compat](#port-sdl2-compat). Without it
  configure looks for SDL 1.x first and takes SDL 2 only when that is missing. The SDL 3 build,
  `--enable-sdl3`, asks for a newer SDL 3 than the [sdl3](#port-sdl3) port.
- `--disable-x11` — No X11 integration: the XKB keyboard-layout lookup and the XRandR queries
  through Xlib. Configure also leaves it out by itself when SDL's `SDL_config.h` defines no X11
  video driver.
- `--enable-opengl` — With `--enable-freetype`, `--enable-printer`, `--enable-screenshots` and
  `--enable-xbrz`: OpenGL output through [libglvnd](#port-libglvnd), the TrueType console through
  [freetype2](#port-freetype2), printer emulation, PNG screenshots and the xBRZ scaler. All are
  upstream's defaults, named.
- `--enable-avcodec` — Video capture through [ffmpeg](#port-ffmpeg)'s libavcodec. Upstream's
  default, named.
- `--enable-libfluidsynth` — With `--enable-alsa-midi` and `--enable-mt32`: General MIDI through
  [fluidsynth](#port-fluidsynth), MIDI out through [alsa-lib](#port-alsa-lib), and the MUNT
  MT-32 emulation compiled from its copy in `src/libs/mt32`. Upstream's defaults, named.
- `--enable-libslirp` — User-mode NE2000 networking through [libslirp](#port-libslirp), which
  needs no privileges. Upstream's default, named.
- `--enable-dynamic-core` — With `--enable-dynrec`: the dynamic CPU cores. The first allows all of
  them, the x86-64 one among them; the second is the portable recompiling core. Upstream's
  defaults, named.

## Notes

The tag archive carries no `configure` script, so `build.sh` runs upstream's `./autogen.sh` first,
which is why [autoconf](#port-autoconf) and [automake](#port-automake) are in `depends`.

Every optional library is found by a probe that drops the feature, at most with a warning, when
the library is missing: the ones above, and also [sdl2-net](#port-sdl2-net) for the modem and IPX
emulation, [libpcap](#port-libpcap) for Ethernet pass-through to the host, and
[duktape](#port-duktape), the ECMAScript engine `sdlmain.cpp` starts a heap of. Each is in
`depends`, which is all that keeps the feature in the build.

`build.sh` deletes every interface translation in `/usr/share/dosbox-x/languages` but
`en_US.lng`; DOSBox-X falls back to its built-in English strings. These `.lng` files are not
message catalogs, so the package-wide English-only rule (see
[Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
does not reach them.

Upstream installs its icon as SVG only, in `hicolor/scalable`. `build.sh` renders
`contrib/icons/dosbox-x.svg` with `rsvg-convert` from [librsvg](#port-librsvg) into five PNG sizes
in the hicolor theme.

Upstream's desktop entry is replaced: its `StartupWMClass` ends in a space, which no app_id
matches. SDL's Wayland app_id is the executable's name, `dosbox-x`.

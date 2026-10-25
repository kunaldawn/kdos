## About

Mupen64Plus is a Nintendo 64 emulator made of a core library and plugins that the core loads at run
time. The port builds upstream's source bundle, one `make` per component: the core, the
`mupen64plus` console front end, the SDL audio and SDL input plugins, the high-level-emulation RSP
plugin, and two video plugins, Rice and Glide64mk2. The front end has no ROM browser; it is started
with a ROM file, so its desktop entry is hidden from the menu and serves only to open N64 ROM images.

## Flags

- `CC="gcc -std=gnu11"` — With `export CFLAGS="${CFLAGS/-std=gnu11/}"`: the tree's C standard
  moves from `CFLAGS` into the C compiler's command. The Rice and Glide64mk2 makefiles append
  `CFLAGS` to `CXXFLAGS`, so a `-std=gnu11` left in `CFLAGS` reaches every C++ compile, where
  `cc1plus` warns that the option is not valid for C++. The makefiles assign `CC` themselves, so it
  is given on the `make` line, and `CFLAGS` is exported rather than given there, because a
  `CFLAGS` on the `make` line would replace every `CFLAGS +=` in the makefile (see
  [C flags handed to the C++ compiler](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/build-troubleshooting.md#c-flags-handed-to-the-c-compiler)).
- `PLUGINDIR=/usr/lib/mupen64plus` — Where the plugins are installed and, in the console front
  end, the directory it searches for them by default; the same value is given to every component
  so the two agree.
- `SHAREDIR=/usr/share/mupen64plus` — Where the shared data files (the core's ROM database
  `mupen64plus.ini`, its OSD font and cheat list, the plugins' `.ini` files) are installed, and
  where the core and front end look for them.
- `SDL_CFLAGS` — With `SDL_LDLIBS`: every makefile asks `sdl2-config` for SDL unless both are
  given, and the tree has no `sdl2-config` ([sdl2-compat](#port-sdl2-compat) removes it), so
  `pkg-config` answers them.
- `GL_CFLAGS=` — With `GL_LDLIBS`, set per component by `_gl`: OpenGL is linked as `-lOpenGL`,
  libglvnd's vendor-neutral GL, which needs no GLX; the GL context is SDL's, made through EGL on
  Wayland. Glide64mk2 alone links `-lGL`: it calls ARB and EXT extension entry points by name,
  which libglvnd's `libGL` exports and `libOpenGL` does not, and musl binds every symbol when the
  plugin is loaded, so against `libOpenGL` the plugin does not load at all.
- `MINIZIP_CFLAGS` — With `MINIZIP_LDLIBS`: the core includes `<unzip.h>`, while minizip's
  pkg-config file names the include directory above `minizip/`, which its other consumers need for
  `<minizip/unzip.h>`. `MINIZIP_CFLAGS` names the `minizip/` directory itself. Given either
  variable, the makefile asks pkg-config for neither, so `MINIZIP_LDLIBS` is given too.
- `OSD=1` — The core's OpenGL on-screen display, drawn with FreeType and GLU. Upstream's default,
  named.
- `VULKAN=1` — The Vulkan support of the core's default video extension, through
  [vulkan-loader](#port-vulkan-loader). Upstream's default, named.
- `NETPLAY=0` — No netplay. Upstream's default, named; netplay links SDL2_net, which is not in
  `depends`.
- `HIRES=1` — Glide64mk2's high-resolution texture packs and texture filters (GlideHQ). Upstream's
  default, named.
- `PIE=1` — Compiles the console front end with `-fPIE` outright. Without it the makefile decides
  by searching `$(CC) -v` for `enable-default-pie`.
- `LDCONFIG=true` — The core's install target runs `ldconfig` on `$(DESTDIR)$(LIBDIR)`; `true`
  takes its place, since the package is staged in `$PKG` and the library cache is the installer's
  business.

## Patches

- `fix-null-usage.patch` — Passes `(uintptr_t)0` instead of `(uintptr_t)NULL` as the window handle of `grSstWinOpenExt` and `grSstWinOpen` in Glide64mk2's `Main.cpp`. musl defines `NULL` in C++ as a pointer constant, and C++ will not cast it to the integer handle.

## Notes

The audio plugin finds its two resamplers, libsamplerate and speexdsp, by a pkg-config probe that
drops the resampler with only a warning when the library is missing. Both libraries are in
`depends`, and that is all that keeps the `src-*` and `speex-*` resamplers in the plugin.

Upstream's desktop entry is replaced by one written in `build.sh` that carries
`StartupWMClass=mupen64plus`: the core's SDL window takes the executable's name as its Wayland
app_id.

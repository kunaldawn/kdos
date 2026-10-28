## About

fastfetch prints a summary of the system beside a logo: the distribution, kernel, uptime, shell,
display, desktop, CPU, GPU, memory, disks and more. On KDOS it also supplies the information
column of the login banner, which [`kdos-banner`](../../../../docs/kdos/04-programs/kdos-command.md#kdos-banner-and-kdos-getty)
draws from `fastfetch --logo none` beside its own logo. The recipe's second source,
`config.jsonc`, is the KDOS layout of that column, installed as `/etc/xdg/fastfetch/config.jsonc`.

## Flags

- `-DENABLE_XCB_RANDR=OFF` — and `-DENABLE_XRANDR=OFF`: fastfetch's X display-server probes,
  `src/detection/displayserver/linux/xcb.c` and `xlib.c`, are compiled always, but their code sits
  inside `#ifdef FF_HAVE_XCB_RANDR` and `#ifdef FF_HAVE_XRANDR`, which only these switches define.
  Off, the display is reported through the Wayland probe alone and no X library is needed.
- `-DENABLE_VAX11=OFF` — and `-DENABLE_GLX=OFF`: the same for libva's X11 backend and GLX, the X
  paths of the video-codec and OpenGL detection; VA-API is still queried over DRM and OpenGL
  through EGL.
- `-DENABLE_WAYLAND=ON` — and every other `ENABLE_*=ON`: Vulkan, DRM, VA-API over DRM, GIO,
  D-Bus, SQLite, EGL, ImageMagick 7, chafa, zlib, PulseAudio, ddcutil, libelf and Lua. These are
  upstream's defaults on Linux, named so the recipe states which libraries the binary is built
  for. A switch whose library is missing only prints `Library: missing: <NAME>` and builds without
  it, so the switch does not make the library present; each one turned on has its port in
  `depends`. fastfetch opens almost all of these libraries with `dlopen()` at run time, so the
  build needs only their headers and pkg-config files.
- `-DLUA_INCLUDE_DIR=/usr/include/lua5.4` — and `-DLUA_LIBRARY=/usr/lib/liblua5.4.so`: build the
  Lua scripting against [lua54](#port-lua54). fastfetch opens Lua at run time by the name
  `liblua5.<minor>.so`, which only lua54 installs; the [lua](#port-lua) port names its library
  `liblua.so`, so scripting built against its headers could never load.
- `-DENABLE_VDPAU=OFF` — and `ENABLE_DCONF`, `ENABLE_EET`, `ENABLE_RPM`, `ENABLE_IMAGEMAGICK6`,
  `ENABLE_QUICKJS`, `ENABLE_OPENCL`: off. VDPAU, EET, RPM, ImageMagick 6 and QuickJS are not ports.
  [dconf](#port-dconf) and the OpenCL loader [ocl-icd](#port-ocl-icd) are ports, and their
  switches are off all the same, so fastfetch reports no dconf settings and no OpenCL devices.
- `-DENABLE_SYSTEM_YYJSON=OFF` — use the copy of the yyjson JSON library embedded in fastfetch's
  tree; yyjson is not a port. This is upstream's default.
- `-DBUILD_TESTS=OFF` — no test programs, upstream's default.

## Notes

`config.jsonc` is a source of the recipe with its own `sha256 =` line, so editing it changes the
recipe and rebuilds the port. It lays out the information column for the login banner and for a
bare `fastfetch`; the banner itself runs fastfetch with `--logo none` and places the logo beside
the output. fastfetch reads `/etc/xdg/fastfetch/config.jsonc` only when the user has no
`~/.config/fastfetch/config.jsonc`, and `fs/etc/skel` gives every new account its own copy of the
layout, so a change to the banner's layout is made in both files or new accounts keep the old
one.

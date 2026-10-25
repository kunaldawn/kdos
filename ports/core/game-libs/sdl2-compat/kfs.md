## About

sdl2-compat is the SDL 2 library, `libSDL2-2.0.so.0`, with SDL 2's headers, `SDL2Config.cmake` for
`find_package(SDL2)` and `sdl2-compat.pc`, whose `Provides: sdl2` is how pkgconf answers a lookup
for `sdl2`. It is built as a thin layer that answers every SDL 2 call through [sdl3](#port-sdl3).
It loads `libSDL3.so.0` at run time, so an SDL 2 program gets SDL 3's drivers: the Wayland video
driver, PipeWire audio and the controller support. It is the SDL 2 every SDL 2 port in KDOS links,
and the base of the SDL 2 satellites ([sdl2-image](#port-sdl2-image), [sdl2-ttf](#port-sdl2-ttf),
[sdl2-mixer](#port-sdl2-mixer)) and of `sdl12-compat`, which serves SDL 1.2 programs through it.

## Flags

- `-DSDL2COMPAT_TESTS=OFF` — Builds none of the SDL 2 test programs, which upstream builds by
  default when sdl2-compat is the top-level project.
- `-DSDL2COMPAT_STATIC=OFF` — Builds no static `libSDL2.a` link library. This is upstream's
  default, named here.
- `-DSDL2COMPAT_X11=OFF` — Builds without libX11: no `SDL_VIDEO_DRIVER_X11` in the installed
  configuration header, no X11 event hook behind SDL 2's `SDL_SYSWMEVENT`, and none of the
  per-program workarounds that send named programs to SDL's X11 driver. Upstream's default is on,
  with `find_package(X11 REQUIRED)`. It goes with [sdl3](#port-sdl3)'s `-DSDL_X11=OFF`: the X11
  hooks could never be filled, and a program that tests the define would take an X11 path that
  cannot run, so the two go on together or not at all.

## Notes

`build.sh` deletes `/usr/bin/sdl2-config`. With no `sdl2-config`, a consumer that pkg-config cannot
answer fails at configure instead of taking the prefix the script hardcodes. A port whose build
runs `sdl2-config` itself gives it a shim script that runs `pkg-config sdl2` (`grafx2` and `stella`
do), or passes the flags from pkgconf as make variables (`neverball`).

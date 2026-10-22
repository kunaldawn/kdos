## About

Hatari emulates the Atari ST, STE, TT and Falcon computers, with a built-in debugger and
disassembler. It is an SDL 2 program, drawn here by [sdl2-compat](#port-sdl2-compat) on SDL 3. The
port also installs the Hatari UI, `hatariui`, a Python GTK 3 front end for configuring and
controlling the emulator. Hatari carries no TOS: the player supplies a TOS or EmuTOS image, which
the first-start dialog asks for.

## Flags

- `-DCMAKE_DISABLE_FIND_PACKAGE_X11=ON` — Do not look for X11. Hatari uses the X11 window of its
  SDL surface for embedding, and sdl2-compat is built without X11 hooks, so there is none to read.
- `-DCMAKE_DISABLE_FIND_PACKAGE_CapsImage=ON` — Do not look for the closed CAPS library, which
  reads IPF, RAW and CTR disk images. It is not a port.
- `-DENABLE_MAN_PAGES=ON` — Build and install the manual pages. Upstream turns this on only when it
  finds `gzip`, which compresses each page; naming it makes a missing `gzip` a build failure rather
  than a package without manual pages.
- `-DENABLE_WERROR=OFF` — Do not compile with `-Werror`. Unset, the option is already off.

## Notes

Every optional library is in `depends`, because each is found or quietly left out:
[zlib](#port-zlib) for zipped disk images, [libpng](#port-libpng) for screenshots and AVI frames,
[readline](#port-readline) for the debugger's history, [portmidi](#port-portmidi) for MIDI out,
[capstone](#port-capstone) for the disassembler, and [eudev](#port-eudev) for joystick
hot-plugging.

The Hatari UI is built only when configure's probe imports GTK 3 from Python, which
[python3-gobject](#port-python3-gobject) and [gtk3](#port-gtk3) in `depends` provide. The probe
fails silently, so `build.sh` checks that `hatariui` was installed: without it the menu entry
`build.sh` writes for it would name a program that is not there.

Both menu entries are replaced for `StartupWMClass`. Hatari sets SDL 2's
`SDL_VIDEO_X11_WMCLASS` to `hatari` with `setenv()` in `main()`, after sdl2-compat has carried the
environment's SDL 2 variables over to SDL 3, so SDL 3 takes the executable's name, also `hatari`,
as the Wayland app_id. The Hatari UI is PyGObject with no application id, so GTK
takes the program name, the script's base name, `hatariui.py`.

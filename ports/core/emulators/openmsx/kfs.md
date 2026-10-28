## About

openMSX is an emulator of the MSX family of home computers, MSX1 to the turbo R, with a built-in
debugger and an ImGui interface, scripted in Tcl. The package installs C-BIOS, the free MSX system
ROMs from upstream's `Contrib/`, so the emulator starts with no copyrighted ROM.

## Flags

- `INSTALL_BINARY_DIR=/usr/bin` — With `INSTALL_SHARE_DIR=/usr/share/openmsx` and
  `INSTALL_DOC_DIR=/usr/share/doc/openmsx`: the install directories. Upstream installs everything
  into one self-contained directory, `/opt/openMSX`. The share directory is compiled into the
  binary, which reads its machines, extensions and scripts from there, so the same values go to
  `make` and to `make install`.
- `INSTALL_CONTRIB=true` — Installs `Contrib/`, which is C-BIOS. Upstream's default, named.
- `SYMLINK_FOR_BINARY=false` — Changes nothing in this build. `build/install.py` reads this
  setting from `build/custom.mk`, never from the `make` line, and it makes its link to the binary
  (in `/usr/local/bin` or `~/bin`) only for an install with no `DESTDIR`.
- `OPENMSX_FLAVOUR=opt` — The generic optimised build, which targets no particular CPU, and
  whose link line strips the binary with `-s`. Upstream's default on x86-64, named.
- `3RDPARTY_FLAG=false` — Links the system libraries dynamically. `true` links the static
  copies that upstream's `make 3rdparty` downloads and compiles, its route for binary packages.
- `CXXFLAGS="${CXXFLAGS/-O2/-O3} -DNDEBUG"` — The makefile starts `CXXFLAGS` empty and never
  reads the environment, so the tree's flags go on the `make` line. There they replace every
  `CXXFLAGS +=` in the makefile, the opt flavour's `-O3 -DNDEBUG -fomit-frame-pointer` among them,
  so this value carries the first two back. GCC omits the frame pointer at `-O3` on x86-64
  without the third.
- `LDFLAGS="${LDFLAGS//-Wl,/}"` — The makefile also starts `LDFLAGS` empty, and treats it as a
  list of linker arguments, putting `-Wl,` before each word. The tree's flags are passed without
  their own `-Wl,` prefix, so each comes out as one `-Wl,` argument.
- `INSTALL_VERBOSE=false` — The install prints neither its opening line nor the closing notice
  about where to put MSX system ROMs.

## Notes

openMSX's probe asks `sdl2-config` for SDL's flags and has no other route, and the tree has no
`sdl2-config` ([sdl2-compat](#port-sdl2-compat) removes it). `build.sh` writes a shim of that
name into `$SRC_ROOT/sdl-shim`, first on `PATH`, that answers `--cflags`, `--libs` and
`--version` from `pkg-config`, which reads sdl2-compat's `Provides: sdl2`. The shim is never
installed.

Beyond the emulation core, the probe builds every component whose libraries it finds: the OpenGL
renderer through [glew](#port-glew), LaserDisc video through [libogg](#port-libogg),
[libvorbis](#port-libvorbis) and [libtheora](#port-libtheora), and ALSA MIDI through
[alsa-lib](#port-alsa-lib). Each of those libraries is in `depends`, which is all
that keeps a component from being left out with no error.

Upstream ships no desktop entry and installs its icons only under its share directory; `build.sh`
writes the entry, with `StartupWMClass=openmsx` because SDL's Wayland app_id is the executable's
name, and installs the logo in six sizes into the hicolor theme.

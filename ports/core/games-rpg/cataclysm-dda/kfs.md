## About

[Cataclysm: Dark Days Ahead]{.idx} is a turn-based survival roguelike set in a procedurally
generated world after a catastrophe: the player scavenges, crafts, builds and fights through
an open map that is simulated as they move. This port is its tiles build, `cataclysm-tiles`,
which draws through SDL2 with graphical tilesets and plays sound, rather than the curses build
for a terminal. It also installs `zzip`, upstream's tool for compressing and expanding save
files.

## Flags

- `WARNINGS` — set to `-Wall -Wextra`, replacing upstream's warning list, which starts with
  `-Werror`. With upstream's list, any warning a newer compiler adds would stop the build.
- `RELEASE=1` — an optimised build. The Makefile's default is a debug build.
- `TILES=1` — with `SOUND=1`: the SDL2 tiles build, with music and sound effects through
  [sdl2-mixer](#port-sdl2-mixer). Sound needs the tiles build.
- `LOCALIZE=0` — with `LANGUAGES=`: build without gettext and compile no translations, so the game
  is English only, like the rest of the system.
- `USE_XDG_DIR=1` — keep saves and settings in the XDG base directories under the user's home
  (`~/.local/share/cataclysm-dda`, `~/.config/cataclysm-dda`) rather than beside the program.
- `DYNAMIC_LINKING=1` — link the system libraries dynamically. On Linux the Makefile links them
  dynamically whatever this says; the variable changes the link only in a Windows build.
- `BACKTRACE=0` — no crash backtrace. Upstream's default is on, and its Linux backtrace code uses
  `execinfo.h`, which musl does not have.
- `PCH=0` — compile without precompiled headers, which upstream's Makefile turns on by default.
- `TESTS=0` — with `RUNTESTS=0`: do not build the test program, and do not run it.
- `ASTYLE=0` — with `LINTJSON=0`: skip the source-style check and the JSON format check that a
  `RELEASE=1` build runs first. Neither changes what is built: the style check runs `astyle`, which
  is not a port, and the JSON check compiles upstream's JSON formatter and runs it over every data
  file.

## Patches

- `flatbuffers.patch` — In the copy of FlatBuffers in `src/third-party`, defines `FLATBUFFERS_LOCALE_INDEPENDENT` to `0` in `util.h`, so number parsing uses plain `strtod`, `strtoll` and `strtoull` instead of the `_l` variants with an explicit C locale. musl declares `strtod_l` and `strtof_l` but not `strtoll_l` or `strtoull_l`. It also comments out the include of `cata_error_locale_dependent.h`, a header the Cataclysm project adds to its copy, which stops the build with `#error` on any platform but Cygwin when the locale-independent functions are off, and comments out the `count_` assignment in the copy-assignment operator of FlatBuffers' `span`. With the patch, that parsing follows the C library's current locale.
- `pathmax.patch` — Defines `PATH_MAX` as `4096` in the bundled zstd's `common/zstd_internal.h`. No file in this release's `src` or `tools` names `PATH_MAX`, so the definition has no effect on this version's build.
- `make.patch` — In the `install` rules of `Makefile` and `lang/Makefile`, replaces `install --mode=` with `install -m` and `cp -R --no-preserve=ownership` with `cp -R`. The tree's `cp` and `install` are GNU coreutils', which accept both forms, and `cp -R` without `-p` keeps no ownership in the first place, so the patched rules install the same files with the same modes.
- `zzip-ldflags.patch` — Adds `$(LDFLAGS)` to the link line of `zzip`, which otherwise takes only the compile flags. Without it `zzip` links without the tree's `-z,now`, and the package lint warns that it lacks `BIND_NOW` (see [What the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)).

## Notes

The `mk` function in `build.sh` passes the same variables to both `make` runs, the build and
`make install`: the Makefile decides what to install from them, and an install run without
`TILES=1` and `SOUND=1` would leave out the tilesets and the sound data.

Upstream installs a scalable SVG icon and a desktop entry. The desktop's icon lookup reads PNGs
only (see
[Desktop entries belong to the port](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port)),
so `build.sh` removes the SVG, renders a 256-pixel PNG from it with `rsvg-convert` from
[librsvg](#port-librsvg), and replaces the entry with one that adds
`StartupWMClass=cataclysm-tiles`, the window's Wayland app id, which is SDL's default, the
executable's name.

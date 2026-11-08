## About
ncdu shows which directories fill a disk, in a browsable ncurses view. It is written in Zig, and
the port builds it with [zig](#port-zig), linking [ncurses](#port-ncurses) and [zstd](#port-zstd)
from the system. `ncdudep-translate_c` and `ncdudep-aro` are the two Zig packages its `build.zig`
fetches; the recipe unpacks them where `zig build --system` looks, so the build uses no network.

## Flags
- `unset PKG_CONFIG_PATH` — Leaves pkgconf with its built-in search path, where it finds
  `ncursesw` and `libzstd` all the same. Zig reads the whole of `pkg-config --list-all` to look up
  a system library and stops with `error: ReadFailure` past 400 KiB. The tree's `PKG_CONFIG_PATH`
  repeats directories pkgconf already searches, so each package is listed three times and a tree
  of about 1,800 `.pc` files passes the limit. With its own path pkgconf lists each one once.
- `-Dcpu=baseline` — Compiles for the architecture's base instruction set and not the build
  machine's CPU.
- `-Dpie=true` — Links a position-independent executable, as every program in the tree is.
- `-Doptimize=ReleaseFast` — The optimised build; without a mode, Zig builds for debugging.

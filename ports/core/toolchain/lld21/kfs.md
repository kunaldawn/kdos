## About
LLD of the 21 series, installed under `/usr/lib/llvm21` beside [llvm21](#port-llvm21) for
[zig](#port-zig), which embeds the linker: lld's libraries are static archives (`liblldELF.a`,
`liblldCOFF.a`, `liblldMachO.a`, `liblldWasm.a` and the rest), and zig links them into its own
binary. The `ld.lld`, `lld-link`, `ld64.lld` and `wasm-ld` programs under the prefix's `bin` are on
no `PATH`; the system's linkers are [lld](#port-lld)'s. How the slot is kept apart from the system
copy is in
[A second version beside the first](../../../../docs/kdos/05-developer/writing-ports.md#a-second-version-beside-the-first).

The port carries three of the series' per-component tarballs: lld's, the shared `cmake` one, and
libunwind's, for one header. lld's CMake files read the shared modules from `../cmake`, so the
`mv` line in `build.sh` gives the unpacked `cmake-<version>.src` that name.

## Flags
- `CMAKE_INSTALL_PREFIX=$_prefix` — Installs everything under `/usr/lib/llvm21`.
- `CMAKE_C_FLAGS_RELEASE` — Replaces CMake's Release flags, `-O3 -DNDEBUG`, with the tree's
  `CFLAGS` and `-DNDEBUG`.
- `CMAKE_CXX_FLAGS_RELEASE` — The same for C++, with `-I` naming the `include` directory of the
  unpacked libunwind tarball. The Mach-O linker includes `mach-o/compact_unwind_encoding.h`, and
  its own include path, `$LLVM_MAIN_SRC_DIR/../libunwind/include`, does not resolve in a
  standalone build: there is no LLVM source tree beside it, and a path through a missing directory
  fails even when its target exists. The header comes from the same release as the linker, so it
  is the encoding the linker was written against; the [libunwind](#port-libunwind) port's copy is
  of the current series and sits under a private prefix.
- `LLVM_DIR=$_prefix/lib/cmake/llvm` — The LLVM CMake package to build against: llvm21's, not the
  system LLVM's.

## Notes
`zig` holds no `NEEDED` entry for any `liblld` library, because the archives are linked into it;
a change to this port reaches `zig` only when zig is rebuilt.

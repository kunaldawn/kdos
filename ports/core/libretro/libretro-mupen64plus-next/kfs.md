## About
Mupen64Plus-Next is a Nintendo 64 emulator packaged as a [libretro]{.idx} core: a shared object,
`mupen64plus_next_libretro.so`, that [retroarch](#port-retroarch) loads from `/usr/lib/libretro`.
It joins the Mupen64Plus core to the GLideN64 renderer, which draws through desktop OpenGL from
[mesa](#port-mesa) by way of [libglvnd](#port-libglvnd).

## Flags
- `platform=unix` — the Linux branch of upstream's `Makefile`: it names the target
  `mupen64plus_next_libretro.so`, links it with `-shared` and the core's version script, links
  desktop GL (`-lGL`), and assembles with `nasm -f elf64` on x86-64.
- `WITH_DYNAREC=x86_64` — compile the dynamic recompiler for x86-64 (`NEW_DYNAREC=2`), which
  translates the console's MIPS code to host code, with its assembly linkage file built by
  [nasm](#port-nasm). Upstream derives the value from `uname -m`; naming it keeps the recompiler
  in the build whatever the build host reports.
- `SYSTEM_ZLIB=1` — with `SYSTEM_LIBPNG=1`, `SYSTEM_MINIZIP=1` and `SYSTEM_XXHASH=1`, link the
  tree's [zlib](#port-zlib), [libpng](#port-libpng), [minizip](#port-minizip) and
  [xxhash](#port-xxhash) through `pkg-config` instead of the copies the source carries: libpng and
  zlib under `custom/dependencies/`, minizip under `mupen64plus-core/subprojects/minizip/`, and the
  xxHash headers under `xxHash/`. Each upstream default is `0`.

## Patches
- `gcc15.patch` — adds `#include <cstdint>` at the top of `GLideN64/src/GLideNHQ/TxHiResLoader.h`, ahead of its include guard, for the `uint32_t` and `uint8_t` in its declarations. The header in this snapshot already includes `<cstdint>` after the guard, so the added line repeats an include the compiler has seen.

## Notes
The `CFLAGS` line adds the `minizip/` include directory by hand. The save-state code
(`mupen64plus-core/src/main/savestates.c`) includes `<unzip.h>` and `<zip.h>` by bare name, while
the minizip port's `minizip.pc` gives only `-I/usr/include`, the directory above `minizip/`. The
makefile appends its own flags to the exported `CFLAGS`, so the directory reaches every compile.

`gcc14.patch` carries three upstream commits, each with its own description. Of them, only the
`EmuThread` change touches this build: the other two add defines to the branches of
`Makefile.common` that compile the bundled minizip and zlib, which `SYSTEM_MINIZIP=1` and
`SYSTEM_ZLIB=1` leave unused.

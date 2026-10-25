## About

pixman is the low-level pixel library under [cairo](#port-cairo) and much of the graphics stack:
it composites images, fills rectangles and rasterises trapezoids, with hand-written SIMD code
paths chosen at run time for the processor it finds. Besides cairo, the ports that link it include
[wlroots](#port-wlroots), the terminal [foot](#port-foot) and its font library
[fcft](#port-fcft).

It is one of the hot libraries the tree builds with link-time optimisation (see
[Every port](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#every-port), "LTO only for the
hot libraries").

## Flags

- `-frandom-seed=pixman` — Fix the seed GCC uses to name each object's `.gnu.lto_*` sections.
  Every library the tree builds with LTO sets it, because a static archive keeps those sections
  and would otherwise differ on every build. pixman installs only the shared library, which
  carries no `.gnu.lto_*` section, so here the seed changes no installed byte.
- `-D b_lto=true` — Compile and link with link-time optimisation, meson's own switch for it.
- `-D mmx=enabled`, `-D sse2=enabled`, `-D ssse3=enabled` — Build the x86 MMX, SSE2 and SSSE3
  code paths. Each defaults to `auto`, which drops the path when the compiler cannot build it; named
  `enabled`, such a compiler stops the build instead of producing a slower library. pixman picks the
  best compiled path at run time, so the library still runs on a processor that lacks one.
- `-D loongson-mmi=disabled`, `-D vmx=disabled`, `-D arm-simd=disabled`, `-D neon=disabled`,
  `-D a64-neon=disabled`, `-D mips-dspr2=disabled`, `-D rvv=disabled` — Build none of the code
  paths for other processors (Loongson, PowerPC, ARM, MIPS and RISC-V), which an x86-64 build
  cannot use.
- `-D openmp=disabled`, `-D libpng=disabled`, `-D tests=disabled`, `-D demos=disabled`,
  `-D gtk=disabled` — Build no tests and no demos. OpenMP and libpng are used only by the tests,
  and GTK only by the demos.

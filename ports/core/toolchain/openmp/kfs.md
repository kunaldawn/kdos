## About
`libomp`, LLVM's [OpenMP]{.idx} runtime: the library `clang -fopenmp` links, with `omp.h` and the
OMPT tool interface headers, and `libompd` with its gdb plug-in for debugging OpenMP programs.
[clang](#port-clang) depends on it, because clang's driver uses `libomp` for `-fopenmp` by default.

GCC keeps its own OpenMP runtime, `libgomp`, which clang's driver cannot use in its place, and
its own `omp.h` in GCC's private include directory, which GCC searches before `/usr/include`. The
two runtimes therefore sit side by side: this port's `omp.h` in `/usr/include` is the one clang
finds, and GCC never sees it.

The port configures LLVM's `runtimes` directory and names itself in `LLVM_ENABLE_RUNTIMES`, the
build upstream supports for its runtimes; `openmp`'s own directory refuses to configure on its
own (see
[A second version beside the first](../../../../docs/kdos/05-developer/writing-ports.md#a-second-version-beside-the-first)).
It is compiled by GCC, the system compiler: this port builds before clang, which depends on it.

## Flags
- `CMAKE_C_FLAGS_RELEASE` — Replaces CMake's Release flags, `-O3 -DNDEBUG`, with the tree's
  `CFLAGS` and `-DNDEBUG`.
- `CMAKE_CXX_FLAGS_RELEASE` — The same for C++.
- `LLVM_ENABLE_RUNTIMES="openmp"` — The runtime the `runtimes` directory builds: this one.
- `LLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF` — Installs into `/usr/lib` rather than a directory per
  target triple, so `libomp.so` is on the default library path.
- `LIBOMP_ENABLE_SHARED=ON` — Builds `libomp` as a shared library, which is upstream's default,
  named.
- `LIBOMP_INSTALL_ALIASES=OFF` — Installs none of the alias links upstream adds by default,
  `libgomp.so`, `libgomp.so.1` and `libiomp5.so`, each pointing at `libomp`. The two `libgomp`
  names are GCC's library, and with the aliases a program linked with `-lgomp`, or one already
  linked against GCC's `libgomp.so.1`, would get LLVM's runtime instead.
- `LIBOMP_ARCHER_SUPPORT=OFF` — Builds no Archer, the OMPT tool that reports data races in
  OpenMP programs through ThreadSanitizer. [compiler-rt](#port-compiler-rt) builds no
  ThreadSanitizer, which does not support musl, so Archer would have nothing to report through.
- `OPENMP_ENABLE_LIBOMPTARGET=OFF` — Builds no `libomptarget`, the runtime that offloads OpenMP
  regions to a GPU.

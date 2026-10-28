## About

LLVM's libunwind is a small C and C++ library that walks the call stack of the running process:
the unwinder that C++ exceptions and `_Unwind_Backtrace` use in a program built against LLVM's
runtimes rather than gcc's. It is built from the same `llvm-project` tarball as
[llvm](#port-llvm), [clang](#port-clang) and the other runtimes of the current series.

KDOS installs it under a private prefix, `/usr/lib/llvm-libunwind`, with its headers in
`/usr/lib/llvm-libunwind/include` and `libunwind.so` and `libunwind.a` in
`/usr/lib/llvm-libunwind/lib`. The names `/usr/include/libunwind.h`, `/usr/lib/libunwind.so` and
the `libunwind*.pc` files belong to [libunwind-nongnu](#port-libunwind-nongnu), the other library
called libunwind, which unwinds another process through `libunwind-ptrace` and which programs such
as `htop`, GStreamer, libcamera and samba link. Both projects use the same file names, so this one
stays off every default search path, and no port names it in `depends`. Why the prefix, and how
the LLVM ports share one source, is in
[A second version beside the first](../../../../docs/kdos/05-developer/writing-ports.md#a-second-version-beside-the-first).

## Flags

- `-S runtimes` — Configures LLVM's `runtimes/` directory, not `libunwind/`. The standalone
  `libunwind/` directory has no `project()` of its own, so CMake gives it an implicit one for C and
  C++ only and silently drops the assembly sources; the library then lacks the routines written in
  assembly, and every static link against it fails on unwinder symbols. `runtimes/` declares
  `project(Runtimes C CXX ASM)`. See
  [A CMake file with no project declaration](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-cmake-file-with-no-project-declaration).
- `CMAKE_INSTALL_PREFIX=$_prefix` — Installs into `/usr/lib/llvm-libunwind`, the `_prefix` helper
  the `kpkgbuild` sets, instead of `/usr`, for the file-name clash described above.
- `CMAKE_C_FLAGS_RELEASE`, `CMAKE_CXX_FLAGS_RELEASE` — Replace CMake's Release flags, `-O3
  -DNDEBUG`, with the tree's `$CFLAGS` and `$CXXFLAGS` followed by `-DNDEBUG`. The optimisation
  level is then the tree's `-O2`, and `-DNDEBUG` stays, as the CMake conventions ask of a recipe
  that sets these variables itself (see
  [cmake](../../../../docs/kdos/05-developer/writing-ports.md#cmake)).
- `LLVM_ENABLE_RUNTIMES="libunwind"` — Selects which runtimes the `runtimes/` build configures;
  naming only libunwind builds nothing else from the tarball.
- `LLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF` — With it on, the library would go into a
  subdirectory named after the target triple, `lib/x86_64-…/`; off, it goes straight into
  `/usr/lib/llvm-libunwind/lib`. The on-by-default for Linux is set in `llvm/CMakeLists.txt`,
  which a configure of `runtimes/` alone never reads, so here the switch names the value the build
  would take anyway and keeps it from depending on that. The [compiler-rt](#port-compiler-rt) and
  [openmp](#port-openmp) ports set the same switch.
- `LLVM_INCLUDE_TESTS=OFF`, `LIBUNWIND_INCLUDE_TESTS=OFF` — Generate no test targets for the
  runtimes build or for libunwind. `LIBUNWIND_INCLUDE_TESTS` defaults to the value of
  `LLVM_INCLUDE_TESTS`, which defaults to on in `runtimes/`.
- `LIBUNWIND_INSTALL_HEADERS=ON` — Installs the library's headers, `libunwind.h`, `unwind.h` and
  `mach-o/compact_unwind_encoding.h` among them, into the prefix's `include/`. It is upstream's
  default, named here.
- `LIBUNWIND_ENABLE_ASSERTIONS=OFF` — libunwind's own switch for `assert()`, independent of the
  build type, and on by default; Release alone would leave the assertions compiled in.

## Notes

The port is in `31_compilers`, before `40_lang`, where [libunwind-nongnu](#port-libunwind-nongnu)
installs. kpkgadd refuses a file another package owns, so a `libunwind` package that held
`/usr/include/libunwind.h` or `/usr/lib/libunwind.so` would stop libunwind-nongnu from
installing; building this port first puts its files in their prefix before that install.

`lld21`, the linker of the LLVM 21 slot, needs `mach-o/compact_unwind_encoding.h` from its own
release. It carries LLVM 21's libunwind source for that header rather than reading this port's
copy, which is the current series and is not on a default include path.

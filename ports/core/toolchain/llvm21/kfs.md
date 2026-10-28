## About
LLVM of the 21 series, installed beside the system [llvm](#port-llvm) under the private prefix
`/usr/lib/llvm21` for [zig](#port-zig), which is built against exactly one LLVM series and
refuses any other. It is the first of the three slot ports, with [clang21](#port-clang21) and
[lld21](#port-lld21) built on it. Everything it installs, the tools, the libraries, the headers
and the CMake package, is under the prefix, which is on no default search path, so every other
port still finds the system LLVM. How the slot is kept apart from the system copy is in
[A second version beside the first](../../../../docs/kdos/05-developer/writing-ports.md#a-second-version-beside-the-first).

The configuration follows the system llvm port's, less the manual pages, the gold plugin
(`LLVM_BINUTILS_INCDIR`), the perf JIT listener (`LLVM_USE_PERF`) and the separate directory for
the test utilities. The prefix is the recipe helper `_prefix`, `/usr/lib/llvm${version%%.*}`, and `series = 21` keeps the update
checker on the 21 line.

## Flags
- `CMAKE_INSTALL_PREFIX=$_prefix` — Installs everything under `/usr/lib/llvm21` instead of `/usr`.
- `LLVM_HOST_TRIPLE` — gcc's triple, `$(cc -dumpmachine)`, as in the llvm port. Left to itself,
  LLVM guesses the glibc triple `x86_64-unknown-linux-gnu`, under which this series' clang would
  find no gcc installation and no C++ headers; see
  [An LLVM that guessed its triple](../../../../docs/kdos/05-developer/build-troubleshooting.md#an-llvm-that-guessed-its-triple).
- `LLVM_DEFAULT_TARGET_TRIPLE` — The same triple, as the default target of the slot's clang.
- `CMAKE_C_FLAGS_RELEASE` — Replaces CMake's Release flags, `-O3 -DNDEBUG`, with the tree's
  `CFLAGS` and `-DNDEBUG`.
- `CMAKE_CXX_FLAGS_RELEASE` — The same for C++, with `-include cstdint` added, as in the llvm port:
  GCC's C++ library headers do not bring in `<cstdint>`, and a file that uses the fixed-width types
  without including it does not compile.
- `LLVM_BUILD_LLVM_DYLIB=OFF` — No `libLLVM.so`.
- `LLVM_LINK_LLVM_DYLIB=OFF` — The tools link the component libraries.
- `BUILD_SHARED_LIBS=ON` — One shared library per component, as the system LLVM is built. The
  programs and libraries find each other through their `$ORIGIN/../lib` run path.
- `LLVM_PARALLEL_COMPILE_JOBS` — The compile-job limit for Ninja, taken from the `-jN` in
  `MAKEFLAGS`, which `script/env/common.env` keeps in exactly that form for this recipe and the
  llvm one.
- `LLVM_INCLUDE_DOCS=OFF` — No documentation targets. The slot installs no manual pages, and
  `man llc` shows the system LLVM's.
- `LLVM_INCLUDE_EXAMPLES=OFF` — No examples; with `LLVM_INCLUDE_TESTS=OFF` and
  `LLVM_INCLUDE_BENCHMARKS=OFF`, no tests and no benchmarks.
- `LLVM_ENABLE_FFI=ON` — `lli` calls native functions through [libffi](#port-libffi).
- `LLVM_ENABLE_RTTI=ON` — Built with C++ run-time type information, as the system LLVM is.
- `LLVM_ENABLE_BINDINGS=OFF` — No OCaml bindings, which would otherwise follow whether an OCaml is
  found.
- `LLVM_ENABLE_ZLIB=FORCE_ON` — zlib support, with a missing zlib a configure error.
- `LLVM_ENABLE_ZSTD=FORCE_ON` — The same for zstd.
- `LLVM_ENABLE_LIBXML2=FORCE_ON` — The same for libxml2, which the Windows manifest library uses.
- `LLVM_ENABLE_LIBEDIT=OFF` — No line editing: [libedit](#port-libedit) is a `41_system` port,
  and the automatic probe would make the result depend on build order.
- `LLVM_ENABLE_LIBPFM=OFF` — No libpfm, which the tree does not carry.
- `LLVM_ENABLE_Z3_SOLVER=OFF` — No Z3 solver for the static analyzer; [z3](#port-z3) is a later
  phase's port.
- `LLVM_INSTALL_UTILS=ON` — Installs `FileCheck`, `not` and the other utilities that LLVM's CMake
  package exports, under the prefix's `bin`, where they collide with nothing.
- `LLVM_ENABLE_LIBCXX=OFF` — Builds against libstdc++, the tree's C++ library.
- `LLVM_ENABLE_LLD=OFF` — Links with binutils' `ld`.
- `LLVM_OPTIMIZED_TABLEGEN=ON` — Matters only in a debug or assertion build; in this Release build
  it changes nothing.
- `LLVM_TARGETS_TO_BUILD=all` — Every code generator. Zig's CMake refuses an LLVM that lacks any
  of the targets it lists, and `zig` links the libraries of every one of them, as its `NEEDED`
  entries show.

## Notes
The 21 series is published as one tarball per component, and the LLVM tarball expects two of its
siblings beside it under set names: its CMake files read `../cmake` for the shared modules, and
LLVM's support library includes the SipHash header from `../third-party/siphash`. The two `mv`
lines give the unpacked `cmake-<version>.src` and `third-party-<version>.src` those names.

Zig finds this LLVM by `CMAKE_PREFIX_PATH=/usr/lib/llvm21` and records `/usr/lib/llvm21/lib` as
its run path, so `/usr/bin/zig` loads the slot's libraries directly. Removing the slot breaks
`zig`, whatever the system LLVM's version.

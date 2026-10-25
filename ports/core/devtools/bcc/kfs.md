## About
The [BPF Compiler Collection]{.idx}: `libbcc`, a library that compiles a BPF program from C with
clang and loads it into the kernel, with USDT (user-level statically defined tracing) probe
support; its Python module and the Python tools that use it, under `/usr/share/bcc/tools`; and
the libbpf-tools, compiled CO-RE tracers such as `execsnoop`, `opensnoop` and `biolatency`,
installed as `bpf-<tool>` in `/usr/bin`. [bpftrace](#port-bpftrace) is the reason for the port:
its `find_package(LibBcc REQUIRED)` is unconditional.

The source is upstream's `bcc-src-with-submodule` release asset, not the tag archive. bcc
carries libbpf, bpftool and blazesym as git submodules, which a tag archive leaves as empty
directories.

## Flags
- `-DREVISION=$version` — The version bcc gives `libbcc.so` and `libbcc_bpf.so` (the
  `VERSION` property, so the file names end in the release) and its Python package. Unset,
  `cmake/version.cmake` asks git for it, and a release tarball has no `.git`.
- `-DCMAKE_USE_LIBBPF_PACKAGE=ON` — Find and link the [libbpf](#port-libbpf) port. Without it
  bcc compiles the libbpf sources of its submodule into `libbcc_bpf`, and bpftrace, which links
  both, would carry two copies.
- `-DENABLE_LLVM_SHARED=OFF` — Link LLVM's component libraries by name. With it on,
  `cmake/clang_libs.cmake` links the single aggregate `libLLVM.so`, which the [llvm](#port-llvm)
  port does not build: it installs one shared library per component
  (`LLVM_BUILD_LLVM_DYLIB=OFF`, `BUILD_SHARED_LIBS=ON`).
- `-Dlibclang$_c=$_clang` — One cache entry per clang library the top-level `CMakeLists.txt`
  looks up, `libclangAnalysis` through `libclangAPINotes`, each set to [clang](#port-clang)'s
  aggregate `libclang-cpp.so`. `find_library` keeps a set entry, so `libbcc.so` links that one
  shared library, as bpftrace does. Left to the search, each entry finds the static
  `libclang<Name>.a`, and `cmake/clang_libs.cmake` links only those fifteen archives; clang 23's
  code in them calls into archives the list does not name (`clangAnalysisLifetimeSafety`,
  `clangDependencyScanning`, `clangOptions`, the `clangScalableStaticAnalysis*` set) and into
  `libLLVMABI`, so 45 symbols would stay unresolved in `libbcc.so.0` and `import bcc` would stop
  at load. The `ldd` test after the install fails the build when any symbol is unresolved.
- `-DENABLE_CLANG_JIT=ON` — Build the clang front end that compiles BPF C at run time. It is
  upstream's default, and it is what bpftrace links; without it neither the tools nor the manual
  pages are built either.
- `-DENABLE_LIBDEBUGINFOD=OFF`, `-DCMAKE_DISABLE_FIND_PACKAGE_LibDebuginfod=TRUE` — No symbol
  lookup through debuginfod, which fetches debug information over the network. The option alone
  only guards the include directory; `src/cc/CMakeLists.txt` links libdebuginfod whenever
  `find_package` finds it, so the search is disabled too. [elfutils](#port-elfutils) is built with
  `--disable-libdebuginfod` in any case.
- `-DCMAKE_DISABLE_FIND_PACKAGE_LuaJIT=TRUE` — Build no Lua front end. `src/lua` builds one
  whenever it finds LuaJIT, and [luajit](#port-luajit) is a port, so without this the result
  would depend on whether it happened to be installed.
- `-DENABLE_MAN=ON` — Install the tools' manual pages, under `/usr/share/bcc/man`. It is
  upstream's default.
- `-DENABLE_EXAMPLES=OFF`, `-DENABLE_TESTS=OFF` — Build neither the example programs nor the
  test suite; both are on by default.
- `-DENABLE_USDT=ON` — Build USDT probe support, upstream's default and the part bpftrace uses;
  the C++ API depends on it.
- `-DPYTHON_CMD=python3` — The interpreter the Python module is installed for.
- `EXTRA_LDFLAGS=-largp` — Appended to the libbpf-tools Makefile's `LDFLAGS`. The tools parse
  their arguments with `argp`, which is part of glibc and a separate library,
  [argp-standalone](#port-argp-standalone), on musl.
- `USE_BLAZESYM=0` — Build the tools without blazesym symbolisation. On x86 the Makefile
  turns it on whenever `cargo` is on the `PATH`, and then builds the blazesym crate with cargo,
  which needs the network.
- `BPFTOOL=/usr/sbin/bpftool` — Generate the BPF skeletons with the [bpftool](#port-bpftool)
  port. Unset, the Makefile first builds a bootstrap bpftool from its `bpftool` submodule, whose
  own `libbpf` submodule is empty even in the release asset.
- `APP_PREFIX=bpf-` — Install each tool as `bpf-<name>`, so that generic names such as
  `profile`, `capable` and `offcputime` stay free in `/usr/bin`.
- `-Wno-error=missing-field-initializers` — Part of the `CFLAGS` given on the make command
  line. A command-line `CFLAGS` replaces the Makefile's own line (`-g -O2 -Wall
  -Wmissing-field-initializers -Werror -Werror=undef`), so the tools compile with the tree's
  flags and no `-Werror`; the appended switch keeps that one warning from ever being an error.

## Patches
- `llvm23-mccontext.patch` — In `src/cc/bcc_debug.cc`, constructs `MCContext` with its `MCAsmInfo`, `MCRegisterInfo` and `MCSubtargetInfo` passed by reference when `LLVM_VERSION_MAJOR` is 23 or later, and by pointer otherwise. The source does not compile against the [llvm](#port-llvm) port without it.

## Notes
The libbpf-tools are a separate `Makefile` outside the CMake build, run with `make -C
../libbpf-tools` from the CMake build directory. Each tool links a static libbpf the Makefile
builds from the submodule's sources, which is a copy inside that executable and not a second
libbpf in any process that loads the shared one. The tools carry `vmlinux.h` per architecture
in the source, so nothing is read from the build machine's kernel.

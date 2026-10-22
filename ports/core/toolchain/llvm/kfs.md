## About
[LLVM]{.idx}: the code generator libraries and their tools (`llc`, `opt`, `lli`, `llvm-ar`,
`llvm-objdump`, `llvm-config` and the rest) of the tree's current LLVM series. It is the first
port `31_compilers` builds, and much of the rest of the phase is built on it:
[clang](#port-clang), [lld](#port-lld), [compiler-rt](#port-compiler-rt), [openmp](#port-openmp)
and [libunwind](#port-libunwind) configure against its installed CMake package, and
[rust](#port-rust) links it instead of the LLVM in its own source. Later phases use it for
[mesa](#port-mesa)'s shader compilers, the BPF tools ([bpftrace](#port-bpftrace),
[bcc](#port-bcc), [bpftool](#port-bpftool)), [qemu](#port-qemu), [wine](#port-wine),
[spirv-llvm-translator](#port-spirv-llvm-translator) and [libclc](#port-libclc).

The port carries upstream's whole `llvm-project` source archive and configures its `llvm`
directory alone; each other LLVM port carries the same archive and configures its own directory
(see
[A second version beside the first](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#a-second-version-beside-the-first)).
It is built as one shared library per component, `libLLVMCore.so`, `libLLVMX86CodeGen.so` and so
on, and there is no `libLLVM.so`.

## Flags
- `LLVM_HOST_TRIPLE` — The triple LLVM is built for, set to gcc's own, `$(cc -dumpmachine)`.
  Left to itself, LLVM guesses `x86_64-unknown-linux-gnu` on this system; see the next switch.
- `LLVM_DEFAULT_TARGET_TRIPLE` — The target clang and libclang compile for when none is given, also
  gcc's triple. Under LLVM's guess, clang looks for a gcc installation under the glibc triple,
  finds none beside gcc's `x86_64-pc-linux-musl`, searches no C++ header directory and would link
  against glibc's loader; every C++ compile through clang or bindgen then fails on
  `'cstddef' file not found`. clang and libclang compile the default in from LLVM's
  `llvm-config.h`, so a change here needs [clang](#port-clang) rebuilt too; see
  [An LLVM that guessed its triple](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/build-troubleshooting.md#an-llvm-that-guessed-its-triple).
- `CMAKE_C_FLAGS_RELEASE` — Replaces CMake's Release flags, `-O3 -DNDEBUG`, with the tree's
  `CFLAGS` and `-DNDEBUG`: LLVM compiles at the tree's level with its assertions off.
- `CMAKE_CXX_FLAGS_RELEASE` — The same for C++, with `-include cstdint` added, which includes
  `<cstdint>` at the top of every C++ file. GCC's C++ library headers do not bring it in on the way
  to their own, so a file that uses `uint64_t` and the other fixed-width types without including
  the header fails to compile; see
  [A standard header the source never includes](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/build-troubleshooting.md#a-standard-header-the-source-never-includes).
- `LLVM_BINUTILS_INCDIR=/usr/include` — Where binutils' `plugin-api.h` is. With it LLVM builds
  `/usr/lib/LLVMgold.so`, LLVM's link-time optimisation plugin for the plugin interface of
  binutils' linkers, which `ar` and `nm` can also load with `--plugin` to read LLVM bitcode
  objects. It is not in `/usr/lib/bfd-plugins`, so none of them loads it unasked. Without the header
  the plugin is skipped without a message, which is why [binutils](#port-binutils) is in
  `depends`.
- `LLVM_BUILD_LLVM_DYLIB=OFF` — Builds no `libLLVM.so`, the single library holding every
  component.
- `LLVM_LINK_LLVM_DYLIB=OFF` — Links the tools against the component libraries, since there is no
  `libLLVM.so` to link.
- `BUILD_SHARED_LIBS=ON` — Builds each component as its own shared library. A consumer links the
  components it names; `bcc` and `bpftrace` carry switches for this layout, described in the
  [ports catalogue](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/06-reference/ports-catalogue.md#devtools).
- `LLVM_PARALLEL_COMPILE_JOBS` — The number of compile jobs Ninja runs at once, taken from the
  `-jN` in `MAKEFLAGS`, which is the build's job count, `KDOS_JOBS`. `script/env/common.env` keeps
  `MAKEFLAGS` exactly `-jN` because this recipe reads the number out of it.
- `LLVM_INCLUDE_EXAMPLES=OFF` — Generates no targets for the examples; with
  `LLVM_INCLUDE_TESTS=OFF` and `LLVM_INCLUDE_BENCHMARKS=OFF`, the build has no test suite and no
  benchmarks.
- `LLVM_ENABLE_FFI=ON` — Links the interpreter, `lli`, with [libffi](#port-libffi), so interpreted
  code can call functions in native libraries.
- `LLVM_ENABLE_RTTI=ON` — Builds LLVM with C++ run-time type information. Upstream's default is
  off; a program compiled with RTTI that derives from an LLVM class needs LLVM's type information
  to link.
- `LLVM_ENABLE_ZLIB=FORCE_ON` — zlib compression in the tools (compressed debug sections, among
  others). `FORCE_ON` makes a missing zlib a configure error instead of a build without it.
- `LLVM_ENABLE_ZSTD=FORCE_ON` — The same for zstd.
- `LLVM_ENABLE_LIBXML2=FORCE_ON` — The same for libxml2, which LLVM's Windows manifest library uses
  (`llvm-mt`, and the COFF linker of lld through it).
- `LLVM_ENABLE_LIBEDIT=OFF` — No line editing in LLVM's interactive tools. [libedit](#port-libedit)
  is built in `41_system`, a later phase, so the automatic probe would make `clang-repl`'s line
  editing, and the `HAVE_LIBEDIT` that LLVM's CMake package exports, depend on build order.
- `LLVM_ENABLE_LIBPFM=OFF` — No hardware performance counters through libpfm, which the tree does
  not carry.
- `LLVM_ENABLE_Z3_SOLVER=OFF` — No Z3 constraint solver for clang's static analyzer.
  [z3](#port-z3) is a `41_system` port, so a tree rebuilt after that phase has it; naming the
  switch keeps LLVM's configuration the same either way.
- `LLVM_ENABLE_BINDINGS=OFF` — Builds no OCaml bindings. They are built when an OCaml with its
  `ctypes` package is found, and [ocaml](#port-ocaml) is a `40_lang` port.
- `LLVM_ENABLE_OCAMLDOC=OFF` — No documentation for those bindings.
- `LLVM_USE_PERF=ON` — Builds LLVM's perf JIT event listener, which writes the `jit-<pid>.dump`
  files [perf](#port-perf) reads to name JIT-compiled code in a profile.
- `LLVM_INSTALL_UTILS=ON` — Installs LLVM's test utilities, `FileCheck`, `not`, `count`,
  `split-file`, `yaml2obj` and the rest. LLVM's CMake package exports them, and
  `find_package(LLVM)` fails when an exported file is missing. [rust](#port-rust)'s bootstrap
  refuses an external LLVM that has no `FileCheck`.
- `LLVM_UTILS_INSTALL_DIR=lib/llvm/utils` — Installs those utilities in `/usr/lib/llvm/utils`
  instead of `/usr/bin`, where generic names such as `not` and `count` could collide with another
  port's. A consumer that runs one names its path.
- `LLVM_TOOL_LLVM_IR2VEC_BUILD=OFF` — Leaves out `llvm-ir2vec`. It links two helper libraries
  declared `BUILDTREE_ONLY`, which `BUILD_SHARED_LIBS` turns into shared objects that are never
  installed, so the installed binary could not start.
- `LLVM_ENABLE_LIBCXX=OFF` — Builds against GCC's libstdc++, the C++ library of the whole tree, not
  LLVM's libc++, which the tree does not carry.
- `LLVM_ENABLE_LLD=OFF` — Links with the default linker, binutils' `ld`. [lld](#port-lld) is built
  from this port's libraries, later in the phase.
- `LLVM_OPTIMIZED_TABLEGEN=ON` — In a debug or assertion build, builds an optimised copy of
  TableGen to run during the build. In this Release build without assertions TableGen is already
  optimised, so the switch changes nothing.
- `LLVM_TARGETS_TO_BUILD=all` — Builds every code generator LLVM has, not only x86: clang, Rust and
  the other consumers can compile for any of them, and Mesa needs the AMDGPU one.
- `LLVM_ENABLE_SPHINX=ON` — Makes the Sphinx documentation targets available, which
  [python3-sphinx](#port-python3-sphinx) in `depends` builds.
- `SPHINX_OUTPUT_HTML=OFF` — No HTML documentation targets.
- `SPHINX_OUTPUT_MAN=ON` — The manual-page target, `docs-llvm-man`, built by name after the
  install.
- `SPHINX_WARNINGS_AS_ERRORS=OFF` — Upstream's default turns every Sphinx warning into an error,
  and the release's own documents draw warnings, so the manual pages would not build.

## Notes
The manual pages are one target, built and installed by name after the main install.
`LLVM_BUILD_DOCS` would put `docs-llvm-man` in the default build beside `docs-dsymutil-man` and
`docs-llvm-dwarfdump-man`, which rebuild the same tree into the same output directory at the same
time. The set is built from every page of the command guide, including the one for `llvm-ir2vec`,
which this build leaves out, so that page is deleted after the copy.

Every port that builds on LLVM reads its installed CMake package, `/usr/lib/cmake/llvm`, and
takes from it the choices made here: the target list, RTTI, exception handling and the library
layout. A consumer that names one of those on its own command line does not override what
`LLVMConfig.cmake` sets; [clang](#port-clang) and [lld](#port-lld) show the effect.

## About
[Zig]{.idx} is a systems programming language whose compiler is also a C and C++ compiler and a
linker. In KDOS it builds [ncdu](#port-ncdu) and the Zig language server [zls](#port-zls), and
[python3-maturin](#port-python3-maturin) is built with its `zig` feature, the
`maturin build --zig` that links through Zig. How a Zig port is written is
in [zig](../../../../docs/kdos/05-developer/writing-ports.md#zig).

Zig is built against exactly one major series of LLVM, and its CMake search refuses an
`llvm-config` from any other. It therefore depends on the slot ports [llvm21](#port-llvm21),
[clang21](#port-clang21) and [lld21](#port-lld21), which install LLVM, clang and lld of that series
under `/usr/lib/llvm21`, and not on the system [llvm](#port-llvm); how the slot is kept apart from
the system LLVM is in
[A second version beside the first](../../../../docs/kdos/05-developer/writing-ports.md#a-second-version-beside-the-first).

The CMake build is upstream's bootstrap. It translates `stage1/zig1.wasm`, a WebAssembly build of
the compiler carried in the source archive, to C, compiles it, uses the result to build the
compiler from its Zig source, and installs that compiler and the standard library under
`/usr/lib/zig`. `zig1.wasm` is a [bootstrap seed]{.idx} and is not installed.

## Flags
- `CMAKE_PREFIX_PATH=/usr/lib/llvm21` — Makes Zig's `llvm-config` search look in the slot's
  prefix before `PATH`, so it finds `/usr/lib/llvm21/bin/llvm-config` and not the system LLVM's.
- `CMAKE_C_FLAGS_RELEASE` — Replaces CMake's Release flags, `-O3 -DNDEBUG`, with the tree's
  `CFLAGS` and `-DNDEBUG`, so the C the build compiles, the bootstrap compiler translated from
  `zig1.wasm` among it, is built at the tree's level with assertions off.
- `CMAKE_CXX_FLAGS_RELEASE` — The same for C++: Zig's glue to LLVM's C++ interfaces.
- `ZIG_TARGET_TRIPLE=native` — The target the installed `zig` binary is built for: the machine it
  is built on. This is upstream's default, named.
- `ZIG_TARGET_MCPU=baseline` — The CPU the `zig` binary itself is compiled for. Upstream's default
  is `native`, the build machine's own CPU, which would make the package stop with an illegal
  instruction on an older processor; `baseline` is the architecture's base instruction set.
- `ZIG_USE_LLVM_CONFIG=ON` — Finds LLVM, clang and lld through `llvm-config`, which is upstream's
  default for a native build, named.
- `ZIG_STATIC_LLVM=OFF` — Links the slot's shared LLVM libraries instead of static archives, which
  the slot does not install for LLVM and clang; this is upstream's default, named. The `zig`
  binary records `/usr/lib/llvm21/lib` as its run path, so it finds them without any search path
  being changed.
- `ZIG_STATIC_ZLIB=ON` — Links zlib's static archive, `libz.a`, into `zig` in place of the
  `-lz` that `llvm-config` reports, so the installed binary needs no `libz.so`.
- `ZIG_STATIC_ZSTD=ON` — The same for zstd, with `libzstd.a`.
- `ZIG_PIE=ON` — Builds `zig` as a position-independent executable, as every program in the tree
  is (see
  [The release flags](../../../../docs/kdos/05-developer/writing-ports.md#the-release-flags)).
  Upstream's default follows `CMAKE_POSITION_INDEPENDENT_CODE`, which this build does not set.
- `CCACHE_DISABLE=1` — Keeps the compiler cache out of this one build. ccache holds a file's
  preprocessed form in memory to hash it, and the preprocessed form of the generated `zig2.c` is
  far larger than the memory the build has, so ccache fails where gcc alone compiles the file; see
  [The compiler cache](../../../../docs/kdos/05-developer/how-kdos-is-built.md#the-compiler-cache).

## Notes
`ZIG_STATIC_ZLIB` and `ZIG_STATIC_ZSTD` take whatever `find_library` finds first among the static
and shared names. They give a static link only while the [zlib](#port-zlib) and [zstd](#port-zstd)
ports install `libz.a` and `libzstd.a`; without them the same build links the shared libraries
and succeeds. `readelf -d /usr/bin/zig` shows which: its `NEEDED` entries list the slot's LLVM
and clang libraries, `libstdc++` and `libc`, and no `libz` or `libzstd`.

A bump of this port to a release built against another LLVM series needs a slot of that series
first: `llvm21`, `clang21` and `lld21` stay on their series (`series = 21`), and Zig's CMake
refuses any other.

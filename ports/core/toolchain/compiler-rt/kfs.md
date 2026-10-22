## About
[compiler-rt]{.idx} is clang's runtime library set. This port builds three parts of it: the
builtins with `crtbegin` and `crtend`, which `--rtlib=compiler-rt` links in place of GCC's
`libgcc`; the profile runtime, which `-fprofile-instr-generate` links for source-based code
coverage; and UndefinedBehaviorSanitizer, which `-fsanitize=undefined` links. Every file goes into
clang's resource directory, `/usr/lib/clang/<major>`, where the driver looks for
`libclang_rt.*`. [chromium](#port-chromium) depends on it.

Of the sanitizers only UBSan is built. AddressSanitizer, ThreadSanitizer, MemorySanitizer and the
others intercept glibc internals and do not support musl.

## Flags
- `CMAKE_C_COMPILER=clang` — Compiles the runtimes with clang, the compiler that links them,
  rather than the system's GCC. The recipe depends on [clang](#port-clang) for this.
- `CMAKE_CXX_COMPILER=clang++` — The same for C++.
- `CMAKE_C_COMPILER_TARGET="$_triple"` — The target the runtimes are compiled for, gcc's own
  triple from `cc -dumpmachine`, which is also the default target of the llvm port. The C++ and
  assembler forms (`CMAKE_CXX_COMPILER_TARGET`, `CMAKE_ASM_COMPILER_TARGET`) are set to the same.
- `LLVM_DEFAULT_TARGET_TRIPLE="$_triple"` — The triple the runtimes build is configured for, the
  same one.
- `CMAKE_C_FLAGS_RELEASE` — Replaces CMake's Release flags, `-O3 -DNDEBUG`, with the tree's
  `CFLAGS` and `-DNDEBUG`.
- `CMAKE_CXX_FLAGS_RELEASE` — The same for C++.
- `LLVM_ENABLE_RUNTIMES="compiler-rt"` — Configures LLVM's `runtimes` directory for this one
  runtime, the build upstream supports for it.
- `LLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF` — Installs the libraries as
  `lib/linux/libclang_rt.<name>-x86_64.a`, with the architecture in the file name, rather than in
  a directory per target triple. clang's driver looks in both places.
- `COMPILER_RT_INSTALL_PATH="$_resdir"` — Installs everything under clang's resource directory,
  `/usr/lib/clang/<major>`, which the recipe's `_resdir` helper derives from the version, so it
  moves with a major-version bump of the LLVM ports.
- `COMPILER_RT_DEFAULT_TARGET_ONLY=ON` — Builds for this machine's triple alone, rather than for
  every target the build could compile for.
- `COMPILER_RT_BUILD_BUILTINS=ON` — Builds `libclang_rt.builtins`, the low-level arithmetic and
  support routines `--rtlib=compiler-rt` links.
- `COMPILER_RT_BUILD_CRT=ON` — Builds `clang_rt.crtbegin` and `clang_rt.crtend`, the start-up and
  shutdown objects that go with the builtins.
- `COMPILER_RT_BUILD_PROFILE=ON` — Builds the profile runtime, which clang's
  instrumentation-based profiling and source-based coverage link (`llvm-profdata` and `llvm-cov`
  read what it writes).
- `COMPILER_RT_BUILD_SANITIZERS=ON` — Builds the sanitizer runtimes, limited by the next switch.
- `COMPILER_RT_SANITIZERS_TO_BUILD="ubsan_minimal"` — Of the sanitizers listed by name, builds only
  the minimal UBSan runtime, which `-fsanitize-minimal-runtime` links. The full UBSan runtime,
  `ubsan_standalone`, is not in that list and is built with the sanitizer support whatever it says.
- `COMPILER_RT_BUILD_XRAY=OFF` — No XRay function-call tracing runtime.
- `COMPILER_RT_BUILD_LIBFUZZER=OFF` — No libFuzzer, the in-process fuzzing engine `-fsanitize=fuzzer` links.
- `COMPILER_RT_BUILD_MEMPROF=OFF` — No heap profiling runtime.
- `COMPILER_RT_BUILD_ORC=OFF` — No ORC runtime, the support library of LLVM's ORC JIT.
- `COMPILER_RT_BUILD_CTX_PROFILE=OFF` — No contextual profiling runtime.
- `COMPILER_RT_BUILD_GWP_ASAN=OFF` — No GWP-ASan, the sampling allocator that is linked into
  Scudo.

## Notes
What the sanitizer switches leave installed, measured on the built tree: `ubsan_standalone` and
`ubsan_standalone_cxx` (`-fsanitize=undefined`), `ubsan_minimal`, `ubsan_loop_detect`, `lsan`,
`stats` and `stats_client`, all under
`/usr/lib/clang/<major>/lib/linux`. The headers of every sanitizer are installed whatever was
built, so `<sanitizer/asan_interface.h>` exists although no AddressSanitizer runtime does, and a
`-fsanitize=address` link fails for want of `libclang_rt.asan`.

The resource directory belongs to clang's major version. A clang and a compiler-rt of different
major versions do not see each other: clang looks under its own number only.

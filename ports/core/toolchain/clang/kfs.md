## About
[Clang]{.idx}, LLVM's C, C++ and Objective-C compiler, with its own tools, among them
`clang-format` and the static analyzer's `scan-build`, and those of `clang-tools-extra`, among
them the language server `clangd` and `clang-tidy`. It also installs `libclang`, the C interface to clang's parser, and
`libclang-cpp.so`, the C++ libraries as one shared object.

The system C and C++ compiler stays GCC; nothing in the environment makes clang the default, and a
port uses clang only when its recipe asks for it. Ports that do are the browsers
([chromium](#port-chromium), [firefox-esr](#port-firefox-esr), [librewolf](#port-librewolf)) and
[thunderbird](#port-thunderbird), the BPF tools ([bpftrace](#port-bpftrace), [bcc](#port-bcc),
[bpftool](#port-bpftool)), and [mesa](#port-mesa), whose OpenCL driver, rusticl, compiles OpenCL C
with clang's libraries. Everything that runs [bindgen](#port-bindgen) loads `libclang` at build time.

The build is standalone: it configures the `clang` directory of the `llvm-project` archive
against the installed [llvm](#port-llvm), finds it through `/usr/lib/cmake/llvm`, and builds
nothing of LLVM itself.

## Flags
- `CMAKE_INSTALL_LIBEXECDIR=/usr/lib/clang` — Installs the helper scripts of the static analyzer
  and of `intercept-build` (`ccc-analyzer`, `c++-analyzer`, `analyze-cc`, `intercept-cc` and the
  rest) in `/usr/lib/clang`, beside the resource directory, instead of `/usr/libexec`. See the
  note on `scan-build` below.
- `CMAKE_C_FLAGS_RELEASE` — Replaces CMake's Release flags, `-O3 -DNDEBUG`, with the tree's
  `CFLAGS` and `-DNDEBUG`.
- `CMAKE_CXX_FLAGS_RELEASE` — The same for C++.
- `LLVM_ENABLE_RTTI=ON` — Builds clang with C++ run-time type information, matching the llvm port.
- `LLVM_ENABLE_EH=ON` — Asks for C++ exception support. In this standalone build the installed
  `LLVMConfig.cmake` sets `LLVM_ENABLE_EH` to the value the llvm port was built with, which does
  not enable it, and that value takes the place of the command line's: clang is compiled with
  `-fno-exceptions`, and the installed `libclang-cpp.so` has no `.gcc_except_table` section.
- `CLANG_ENABLE_LIBXML2=ON` — Lets clang use libxml2. It reaches only `c-index-test`, which checks
  libclang's comment XML against its schema.
- `CMAKE_REQUIRE_FIND_PACKAGE_LibXml2=ON` — clang looks for libxml2 with a `QUIET`
  `find_package`; this makes a missing libxml2 a configure error instead of a `c-index-test`
  built without it.
- `CLANG_BUILD_EXAMPLES=OFF` — Builds none of clang's example plugins.
- `CLANG_INCLUDE_DOCS=ON` — Generates the documentation targets. It defaults to
  `LLVM_INCLUDE_DOCS`, which a standalone build never defines, so without it `docs-clang-man` does
  not exist and the build after the install stops.
- `CLANG_TOOLS_EXTRA_INCLUDE_DOCS=ON` — The same for `clang-tools-extra`, whose target is
  `docs-clang-tools-man`.
- `LLVM_ENABLE_SPHINX=ON` — Makes the Sphinx targets available, with
  [python3-sphinx](#port-python3-sphinx) in `depends`.
- `SPHINX_OUTPUT_HTML=OFF` — No HTML documentation targets.
- `SPHINX_OUTPUT_MAN=ON` — The manual-page targets.
- `SPHINX_WARNINGS_AS_ERRORS=OFF` — Keeps a Sphinx warning in the release's own documents from
  failing the manual-page build.
- `CLANG_INCLUDE_TESTS=OFF` — Generates no test targets; with `LLVM_INCLUDE_TESTS=OFF`, no tests
  are configured at all.
- `LIBCLANG_BUILD_STATIC=ON` — Builds `libclang.a` beside `libclang.so`, for a consumer that links
  libclang statically.
- `CLANG_LINK_CLANG_DYLIB=OFF` — Links the clang tools against clang's component libraries rather
  than `libclang-cpp.so`, as the llvm port links its tools against its components.
  `libclang-cpp.so` is built and installed either way.
- `CLANG_BUILT_STANDALONE=ON` — clang sets this itself when its directory is the top of the CMake
  tree, as `-S clang` makes it; naming it changes nothing.
- `CLANG_ENABLE_CLANGD=ON` — Builds `clangd`, the language server editors talk to.
- `CLANGD_BUILD_XPC=OFF` — No XPC transport for clangd. XPC is macOS's inter-process mechanism,
  and the default is already off on Linux.
- `LLVM_EXTERNAL_CLANG_TOOLS_EXTRA_SOURCE_DIR` — Where `clang-tools-extra` is. A standalone clang
  build reaches clangd, clang-tidy and the other extra tools only through this variable; without
  it the same build produces a compiler and none of them, and reports success.
- `LLVM_TARGETS_TO_BUILD` — Names five targets, but in this standalone build the installed
  `LLVMConfig.cmake` sets `LLVM_TARGETS_TO_BUILD` to the list llvm was built with, every target,
  and that takes the place of the command line's. The installed `clang` links the code generators
  of every target LLVM has, and the target-specific headers such as `arm_neon.h` and
  `riscv_vector.h` are installed in the resource directory.

## Notes
The manual pages are built by target after the install and copied by hand, because nothing puts
them in the default build without `LLVM_BUILD_DOCS`, and that would bring in every other
documentation target too. `docs-clang-man` makes `clang(1)` and `diagtool(1)`;
`docs-clang-tools-man` makes `extraclangtools(1)`, the `clang-tools-extra` manual as one page.

`CLANG_DEFAULT_OPENMP_RUNTIME` is left at its default, `libomp`, which [openmp](#port-openmp)
installs, so `clang -fopenmp` compiles against its `omp.h` and links `libomp`. The driver passes
`-fopenmp` on to the front end only for `libomp` and `libiomp5`: pointed at GCC's `libgomp`, a
program would link, but every OpenMP pragma would be dropped and it would run serially with no
diagnostic.

`scan-build` looks for `ccc-analyzer` and `c++-analyzer` in `../libexec` beside its own
directory and then in that directory itself, `/usr/bin`. With the helpers installed in
`/usr/lib/clang`, it finds them in neither place and stops with
`'ccc-analyzer' does not exist at '/usr/bin/ccc-analyzer'`.

The LLVM options named here that `LLVMConfig.cmake` also sets (`LLVM_ENABLE_RTTI`,
`LLVM_ENABLE_EH`, `LLVM_TARGETS_TO_BUILD`) follow the llvm port, whatever this command line
says. A change to one of them belongs in the llvm recipe.

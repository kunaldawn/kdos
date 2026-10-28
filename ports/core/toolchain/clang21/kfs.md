## About
Clang of the 21 series, installed under `/usr/lib/llvm21` beside [llvm21](#port-llvm21) for
[zig](#port-zig). Zig compiles C and C++ with clang's libraries and links `libclang-cpp.so` of
the same series, so the slot needs a clang as well as an LLVM. Nothing under the prefix is on
`PATH`, so `clang` on the command line is still the system [clang](#port-clang). How the slot is
kept apart from the system copy is in
[A second version beside the first](../../../../docs/kdos/05-developer/writing-ports.md#a-second-version-beside-the-first).

The 21 series is published as one tarball per component. This port carries clang's and the
shared `cmake` tarball, and builds standalone against llvm21's CMake package.

## Flags
- `CMAKE_INSTALL_PREFIX=$_prefix` — Installs everything under `/usr/lib/llvm21`.
- `CMAKE_C_FLAGS_RELEASE` — Replaces CMake's Release flags, `-O3 -DNDEBUG`, with the tree's
  `CFLAGS` and `-DNDEBUG`.
- `CMAKE_CXX_FLAGS_RELEASE` — The same for C++.
- `LLVM_DIR=$_prefix/lib/cmake/llvm` — The LLVM CMake package to build against: the slot's, not
  the system's in `/usr/lib/cmake/llvm`, which a plain search would find first.
- `LLVM_ENABLE_RTTI=ON` — Built with C++ run-time type information, matching llvm21.
- `CLANG_ENABLE_LIBXML2=ON` — Lets clang use libxml2, which reaches only `c-index-test`'s check of
  libclang's comment XML against its schema.
- `CMAKE_REQUIRE_FIND_PACKAGE_LibXml2=ON` — clang looks for libxml2 with a `QUIET`
  `find_package`; this makes a missing one a configure error rather than a `c-index-test` built
  without it.
- `CLANG_BUILD_EXAMPLES=OFF` — No example plugins.
- `CLANG_INCLUDE_DOCS=OFF` — No documentation targets, so no manual page is generated; the one page
  under the prefix is `scan-build.1`, which the static analyzer installs as a plain file.
- `CLANG_INCLUDE_TESTS=OFF` — No test targets; with `LLVM_INCLUDE_TESTS=OFF`, no tests at all.
- `CLANG_LINK_CLANG_DYLIB=OFF` — Links the clang tools against clang's component libraries, not
  `libclang-cpp.so`, which is built and installed all the same.
- `BUILD_SHARED_LIBS=ON` — Builds each clang component as its own shared library, as llvm21 builds
  LLVM's. This port installs no static archives; its programs and libraries find each other
  through their `$ORIGIN/../lib` run path.
- `CLANG_BUILT_STANDALONE=ON` — clang sets this itself when its directory is the top of the CMake
  tree, as it is here; naming it changes nothing.

## Notes
`clang-tools-extra` is not built here, so the slot has no `clangd` or `clang-tidy`; those are the
system clang's.

The `mv` line gives the unpacked `cmake-<version>.src` the name `cmake`, beside clang's own
directory, where clang's `CMakeLists.txt` reads the shared CMake modules from `../cmake`.

#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

cmake -S clang -B build -G Ninja \
    -D CMAKE_INSTALL_PREFIX=/usr \
    -D CMAKE_INSTALL_LIBEXECDIR=/usr/lib/clang \
    -D CMAKE_BUILD_TYPE=Release \
    -D CMAKE_C_FLAGS_RELEASE="$CFLAGS -DNDEBUG" \
    -D CMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS -DNDEBUG" \
    -D LLVM_ENABLE_RTTI=ON \
    -D LLVM_ENABLE_EH=ON \
    -D LLVM_INCLUDE_TESTS=OFF \
    -D CLANG_ENABLE_LIBXML2=ON \
    -D CMAKE_REQUIRE_FIND_PACKAGE_LibXml2=ON \
    -D CLANG_BUILD_EXAMPLES=OFF \
    -D CLANG_INCLUDE_DOCS=ON \
    -D CLANG_TOOLS_EXTRA_INCLUDE_DOCS=ON \
    -D LLVM_ENABLE_SPHINX=ON \
    -D SPHINX_OUTPUT_HTML=OFF \
    -D SPHINX_OUTPUT_MAN=ON \
    -D SPHINX_WARNINGS_AS_ERRORS=OFF \
    -D CLANG_INCLUDE_TESTS=OFF \
    -D LIBCLANG_BUILD_STATIC=ON \
    -D CLANG_LINK_CLANG_DYLIB=OFF \
    -D CLANG_BUILT_STANDALONE=ON \
    -D CLANG_ENABLE_CLANGD=ON \
    -D CLANGD_BUILD_XPC=OFF \
    -D LLVM_EXTERNAL_CLANG_TOOLS_EXTRA_SOURCE_DIR="$SRC/clang-tools-extra" \
    -D LLVM_TARGETS_TO_BUILD="AMDGPU;BPF;NVPTX;WebAssembly;X86" \
    -Wno-dev

cmake --build build
DESTDIR=$PKG cmake --install build

cmake --build build --target docs-clang-man docs-clang-tools-man
install -Dm644 build/docs/man/*.1 build/tools/extra/docs/man/*.1 \
    -t "$PKG/usr/share/man/man1"

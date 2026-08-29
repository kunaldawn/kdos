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

mv $SRC_ROOT/cmake-${version}.src $SRC_ROOT/cmake

# clangd COSTS ONE TARBALL AND NO NEW PORT, and it is the language server for
# the language most of this tree is written in. A standalone clang build finds
# clang-tools-extra at `tools/extra` and nowhere else — there is no cmake
# variable for it in this configuration — so the move IS the wiring, and
# without it the same build produces a compiler and no language server while
# reporting success.
mv $SRC_ROOT/clang-tools-extra-${version}.src $SRC/tools/extra


cmake -B build -G Ninja \
    -D CMAKE_INSTALL_PREFIX=/usr \
    -D CMAKE_INSTALL_LIBEXECDIR=/usr/lib/clang \
    -D CMAKE_BUILD_TYPE=Release \
    -D CMAKE_C_FLAGS_RELEASE="$CFLAGS" \
    -D CMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS" \
    -D LLVM_ENABLE_RTTI=ON \
    -D LLVM_ENABLE_EH=ON \
    -D LLVM_INCLUDE_TESTS=OFF \
    -D CLANG_BUILD_EXAMPLES=OFF \
    -D CLANG_INCLUDE_DOCS=OFF \
    -D CLANG_INCLUDE_TESTS=OFF \
    -D LIBCLANG_BUILD_STATIC=ON \
    -D CLANG_LINK_CLANG_DYLIB=OFF \
    -D CLANG_BUILT_STANDALONE=ON \
    -D CLANG_ENABLE_CLANGD=ON \
    -D CLANGD_BUILD_XPC=OFF \
    -D LLVM_TARGETS_TO_BUILD="AMDGPU;BPF;NVPTX;WebAssembly;X86" \
    -Wno-dev

cmake --build build
DESTDIR=$PKG cmake --install build

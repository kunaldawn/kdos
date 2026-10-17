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

export TREE_SITTER_JS_RUNTIME=native
cmake -S lldb -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_C_FLAGS_RELEASE="$CFLAGS -DNDEBUG" \
	-D CMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS -DNDEBUG" \
	-D LLVM_DIR=/usr/lib/cmake/llvm \
	-D Clang_DIR=/usr/lib/cmake/clang \
	-D LLVM_ENABLE_RTTI=ON \
	-D LLDB_ENABLE_SWIG=ON \
	-D LLDB_ENABLE_PYTHON=ON \
	-D LLDB_ENABLE_CURSES=ON \
	-D LLDB_ENABLE_LZMA=ON \
	-D LLDB_ENABLE_LIBXML2=ON \
	-D LLDB_ENABLE_LUA=OFF \
	-D LLDB_ENABLE_LIBEDIT=ON \
	-D LLDB_ENABLE_TREESITTER=ON \
	-D LLDB_INCLUDE_TESTS=OFF \
	-D LLVM_INCLUDE_TESTS=OFF \
	-D LLVM_ENABLE_SPHINX=ON \
	-D SPHINX_OUTPUT_HTML=OFF \
	-D SPHINX_OUTPUT_MAN=ON \
	-D SPHINX_WARNINGS_AS_ERRORS=OFF \
	-D LLVM_MAIN_SRC_DIR="$SRC/llvm" \
	-Wno-dev

cmake --build build
DESTDIR=$PKG cmake --install build

cmake --build build --target docs-lldb-man
install -Dm644 build/docs/man/*.1 -t "$PKG/usr/share/man/man1"

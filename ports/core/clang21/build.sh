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

# clang looks for libxml2 with a QUIET find_package;
# CMAKE_REQUIRE_FIND_PACKAGE_LibXml2 makes a missing one a configure error
# rather than a libclang built without it.
cmake -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=$_prefix \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_C_FLAGS_RELEASE="$CFLAGS" \
	-D CMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS" \
	-D LLVM_DIR=$_prefix/lib/cmake/llvm \
	-D LLVM_ENABLE_RTTI=ON \
	-D LLVM_INCLUDE_TESTS=OFF \
	-D CLANG_ENABLE_LIBXML2=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LibXml2=ON \
	-D CLANG_BUILD_EXAMPLES=OFF \
	-D CLANG_INCLUDE_DOCS=OFF \
	-D CLANG_INCLUDE_TESTS=OFF \
	-D CLANG_LINK_CLANG_DYLIB=OFF \
	-D BUILD_SHARED_LIBS=ON \
	-D CLANG_BUILT_STANDALONE=ON \
	-Wno-dev

cmake --build build
DESTDIR=$PKG cmake --install build

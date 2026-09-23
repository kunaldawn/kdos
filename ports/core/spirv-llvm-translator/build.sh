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

# The headers directory is a source-tree root: the library adds /include to
# it, so /usr is what reaches /usr/include/spirv/unified1.
cmake -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_BUILD_TYPE=Release \
	-D LLVM_EXTERNAL_SPIRV_HEADERS_SOURCE_DIR=/usr \
	-D LLVM_SPIRV_INCLUDE_TESTS=OFF \
	-D LLVM_SPIRV_ENABLE_LIBSPIRV_DIS=OFF \
	-D CCACHE_ALLOWED=OFF \
	-Wno-dev

cmake --build build
DESTDIR=$PKG cmake --install build

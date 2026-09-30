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
	-D LLVM_SPIRV_ENABLE_LIBSPIRV_DIS=ON \
	-D CCACHE_ALLOWED=OFF \
	-Wno-dev

cmake --build build
DESTDIR=$PKG cmake --install build

# --spirv-tools-dis is compiled in only when SPIRV-Tools is found, and a miss
# is a status line; the fallback's message in the binary is the tell.
test -x $PKG/usr/bin/llvm-spirv
if grep -q "built without --spirv-tools-dis" $PKG/usr/bin/llvm-spirv; then
	exit 1
fi

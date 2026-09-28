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

# PREFER_EXTERNAL_*: without them blosc compiles its own bundled lz4, zlib and
# zstd into the library, and no update to those ports reaches it. Snappy is
# not a port.
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED=ON \
	-DBUILD_STATIC=OFF \
	-DBUILD_TESTS=OFF \
	-DBUILD_FUZZERS=OFF \
	-DBUILD_BENCHMARKS=OFF \
	-DDEACTIVATE_LZ4=OFF \
	-DDEACTIVATE_ZLIB=OFF \
	-DDEACTIVATE_ZSTD=OFF \
	-DDEACTIVATE_SNAPPY=ON \
	-DPREFER_EXTERNAL_LZ4=ON \
	-DPREFER_EXTERNAL_ZLIB=ON \
	-DPREFER_EXTERNAL_ZSTD=ON
ninja
DESTDIR=$PKG ninja install

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

mkdir build
cd build
cmake .. \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_BUILD_TYPE=Release \
	-Dprotobuf_BUILD_TESTS=OFF \
	-Dprotobuf_BUILD_SHARED_LIBS=ON \
	-DFETCHCONTENT_SOURCE_DIR_ABSL="$SRC_ROOT/abseil-cpp-20250512.1"
make
make DESTDIR=$PKG install

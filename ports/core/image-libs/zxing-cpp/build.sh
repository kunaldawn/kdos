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

cmake -B build -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DZXING_READERS=ON \
	-DZXING_WRITERS=OLD \
	-DZXING_USE_BUNDLED_ZINT=OFF \
	-DZXING_EXPERIMENTAL_API=OFF \
	-DZXING_C_API=OFF \
	-DZXING_DEPENDENCIES=LOCAL \
	-DZXING_EXAMPLES=OFF \
	-DZXING_UNIT_TESTS=OFF \
	-DZXING_BLACKBOX_TESTS=OFF \
	-DZXING_PYTHON_MODULE=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

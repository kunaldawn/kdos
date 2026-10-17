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

patch -p1 -i "$PORT_SRC/permission.patch"
mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_RTTR_DYNAMIC=ON \
	-DBUILD_STATIC=OFF \
	-DBUILD_WITH_STATIC_RUNTIME_LIBS=OFF \
	-DBUILD_UNIT_TESTS=OFF \
	-DBUILD_BENCHMARKS=OFF \
	-DBUILD_EXAMPLES=OFF \
	-DBUILD_DOCUMENTATION=OFF \
	-DBUILD_INSTALLER=ON \
	-DBUILD_PACKAGE=OFF
ninja
DESTDIR=$PKG ninja install
find "$PKG/usr/share/rttr" -maxdepth 1 -type f -exec chmod 644 {} +

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

patch -p1 -i "$PORT_SRC/cmake-config-dir.patch"
patch -p1 -i "$PORT_SRC/hdf5-1.14.patch"

mkdir -p build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_SKIP_RPATH=ON \
	-DMEDFILE_BUILD_SHARED_LIBS=ON \
	-DMEDFILE_BUILD_STATIC_LIBS=OFF \
	-DMEDFILE_BUILD_TESTS=OFF \
	-DMEDFILE_BUILD_PYTHON=OFF \
	-DMEDFILE_BUILD_DOC=OFF \
	-DMEDFILE_INSTALL_DOC=OFF \
	-DDESTDIR="$PKG"
ninja
DESTDIR=$PKG ninja install
test -L "$PKG/usr/bin/mdump" && test -L "$PKG/usr/bin/xmdump"

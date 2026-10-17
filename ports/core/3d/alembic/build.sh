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

mkdir build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_SKIP_INSTALL_RPATH=ON \
	-DALEMBIC_SHARED_LIBS=ON \
	-DALEMBIC_BUILD_LIBS=ON \
	-DUSE_HDF5=OFF \
	-DUSE_BINARIES=ON \
	-DUSE_EXAMPLES=OFF \
	-DUSE_TESTS=OFF \
	-DUSE_PYALEMBIC=OFF \
	-DUSE_ARNOLD=OFF \
	-DUSE_MAYA=OFF \
	-DUSE_PRMAN=OFF
ninja
DESTDIR=$PKG ninja install

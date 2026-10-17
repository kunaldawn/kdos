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

patch -p1 --no-backup-if-mismatch -i "$PORT_SRC/install-rpath.patch"

export CFLAGS="$CFLAGS -Wno-error"
export CXXFLAGS="$CXXFLAGS -Wno-error"
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_SHARED_LIBS=ON \
	-D WITH_GNUCASH=ON \
	-D WITH_SQL=OFF \
	-D WITH_AQBANKING=OFF \
	-D WITH_OFX=ON \
	-D WITH_PYTHON=OFF \
	-D DISABLE_NLS=ON \
	-D COMPILE_GSCHEMAS=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
chmod 755 "$PKG/usr/bin/gnucash-valgrind"

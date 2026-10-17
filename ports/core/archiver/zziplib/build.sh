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

cmake -B build -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_TESTS=OFF \
	-DMSVC_STATIC_RUNTIME=OFF \
	-DZZIPSDL=OFF \
	-DZZIPWRAP=OFF \
	-DZZIPTEST=OFF \
	-DZZIPBINS=ON \
	-DZZIPDOCS=ON
cmake --build build
DESTDIR=$PKG cmake --install build

test -e "$PKG"/usr/lib/pkgconfig/zziplib.pc
test -x "$PKG"/usr/bin/unzzip

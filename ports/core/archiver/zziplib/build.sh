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

# The libraries, the unzzip programs and the manual pages, which the docs
# directory generates with Python. The SDL example, the zzipwrap example
# library and the self-extracting test are examples upstream never installs.
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

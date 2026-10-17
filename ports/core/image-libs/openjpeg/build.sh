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

mkdir -p build && cd build
cmake .. \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_STATIC_LIBS=OFF \
	-DBUILD_TESTING=OFF \
	-DBUILD_CODEC=ON \
	-DBUILD_THIRDPARTY=OFF \
	-DCMAKE_REQUIRE_FIND_PACKAGE_ZLIB=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_PNG=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_TIFF=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_LCMS2=ON
make
make DESTDIR=$PKG install

install -Dm644 ../doc/man/man1/*.1 -t $PKG/usr/share/man/man1
install -Dm644 ../doc/man/man3/*.3 -t $PKG/usr/share/man/man3

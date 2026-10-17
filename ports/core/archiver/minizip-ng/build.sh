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

patch -p1 -i "$PORT_SRC/zstd-pkgconfig.patch"

mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DMZ_COMPAT=OFF \
	-DMZ_FETCH_LIBS=OFF \
	-DMZ_FORCE_FETCH_LIBS=OFF \
	-DMZ_ZLIB=ON -DMZ_ZLIB_FLAVOR=zlib \
	-DMZ_BZIP2=ON \
	-DMZ_LZMA=ON \
	-DMZ_PPMD=ON \
	-DFETCHCONTENT_SOURCE_DIR_PPMD="$SRC_ROOT/7zip-$_7zip" \
	-DFETCHCONTENT_FULLY_DISCONNECTED=ON \
	-DMZ_ZSTD=ON \
	-DMZ_PKCRYPT=ON \
	-DMZ_WZAES=ON \
	-DMZ_OPENSSL=ON \
	-DMZ_LIBBSD=OFF \
	-DMZ_ICONV=ON \
	-DMZ_BUILD_TESTS=OFF \
	-DMZ_BUILD_UNIT_TESTS=OFF
ninja
DESTDIR=$PKG ninja install

readelf -d "$PKG/usr/lib/libminizip-ng.so" | grep -q "NEEDED.*libzstd"

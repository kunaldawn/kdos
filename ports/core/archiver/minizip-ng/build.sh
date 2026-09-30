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


# MZ_COMPAT=OFF installs libminizip-ng and <minizip-ng/…> under their own
# names; the compatibility layer would install a libminizip and <unzip.h> that
# collide with zlib's minizip. MZ_ZSTD is off because the zstd port installs no
# CMake package configuration, which is the only way this project finds it.
# getrandom() is musl's, so libbsd is not needed for random bytes. With
# MZ_FETCH_LIBS=OFF a missing library disables its method instead of
# downloading it.
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
	-DMZ_ZSTD=OFF \
	-DMZ_PKCRYPT=ON \
	-DMZ_WZAES=ON \
	-DMZ_OPENSSL=ON \
	-DMZ_LIBBSD=OFF \
	-DMZ_ICONV=ON \
	-DMZ_BUILD_TESTS=OFF \
	-DMZ_BUILD_UNIT_TESTS=OFF
ninja
DESTDIR=$PKG ninja install

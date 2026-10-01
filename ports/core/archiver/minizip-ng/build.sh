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
# downloading it. PPMd has no library to find: CMake clones 7-Zip's sources
# for it whatever MZ_FETCH_LIBS says, so FETCHCONTENT_SOURCE_DIR_PPMD points it
# at the 7-Zip release the 7zip port builds, and FULLY_DISCONNECTED makes any
# other download an error instead of a network call.
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

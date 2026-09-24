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


# THE SECOND BUILD OF gcc-arm-none-eabi's SOURCE. libstdc++ compiles against
# the target C library, and picolibc-arm-none-eabi is built with that
# compiler, so the three cannot be one recipe. The gcc tree is configured
# again with gcc-arm-none-eabi's own options — the prefix, the sysroot and
# the rmprofile multilib set must match, or the libraries land in directories
# the installed driver never searches — and only libstdc++-v3 is installed.
#
# --with-picolibc --without-newlib select libstdc++'s picolibc configuration:
# os/picolibc and a fixed list of what picolibc provides, taken without link
# tests a bare-metal target cannot run. --with-newlib in its place selects
# newlib's list, which claims functions picolibc does not have.
# --with-headers=no keeps libtool's dlopen probe, a link test, out of the
# configure; it also makes libstdc++ default to a freestanding build, so
# --enable-hosted-libstdcxx asks for the hosted one picolibc supports.
#
# picolibc's headers are in /usr/arm-none-eabi/include, not the sysroot's
# /usr/include: --with-native-system-header-dir=/include names them, or the gcc
# build stops at fixincludes looking for a directory that does not exist.
#
# --disable-libstdcxx-verbose keeps the verbose terminate handler, and the
# demangler and stdio it drags into every image that can throw, out of the
# library. Threads, shared libraries and precompiled headers stay off as they
# are in gcc-arm-none-eabi.
mkdir -p build && cd build
../configure \
	--target=arm-none-eabi \
	--prefix=/usr \
	--libexecdir=/usr/lib \
	--with-sysroot=/usr/arm-none-eabi \
	--with-native-system-header-dir=/include \
	--enable-languages=c,c++ \
	--with-picolibc \
	--without-newlib \
	--with-headers=no \
	--disable-nls \
	--disable-shared \
	--disable-threads \
	--disable-libssp \
	--disable-libgomp \
	--disable-libquadmath \
	--disable-libatomic \
	--disable-libstdcxx-pch \
	--disable-libstdcxx-verbose \
	--enable-hosted-libstdcxx \
	--disable-decimal-float \
	--with-gnu-as --with-gnu-ld \
	--with-zstd=/usr \
	--without-isl \
	--enable-multilib \
	--with-multilib-list=rmprofile \
	--with-pkgversion="KDOS"
make all-target-libstdc++-v3
# The install runs in the libstdc++ directory, not through the top-level
# install-target-libstdc++-v3, which installs libgcc first and would give this
# package every file gcc-arm-none-eabi owns. The pretty-printers land in
# /usr/share/gcc-$version/python, the path the host gcc of the same version
# installs its own; the -gdb.py loaders beside each libstdc++.a point there
# and are never auto-loaded for a static archive.
make -C arm-none-eabi/libstdc++-v3 DESTDIR=$PKG install
find "$PKG" \( -name '*.la' -o -name '*-gdb.py' \) -delete
rm -rf "$PKG/usr/share"

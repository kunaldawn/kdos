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

patch -p1 -i "$PORT_SRC/picolibc-long-double-math.patch"
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
	--with-pkgversion="KDOS" \
	CFLAGS_FOR_TARGET="-Os -pipe -ffunction-sections -fdata-sections" \
	CXXFLAGS_FOR_TARGET="-Os -pipe -ffunction-sections -fdata-sections"
make all-target-libstdc++-v3 2>&1 | tee ../libstdcxx-build.log
test "${PIPESTATUS[0]}" -eq 0
if grep -F 'stamp-modules-bits] Error' ../libstdcxx-build.log; then
	exit 1
fi
make -C arm-none-eabi/libstdc++-v3 DESTDIR=$PKG install
find "$PKG" \( -name '*.la' -o -name '*-gdb.py' \) -delete
rm -rf "$PKG/usr/share"

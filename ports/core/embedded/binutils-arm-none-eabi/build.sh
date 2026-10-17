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
../configure \
	--target=arm-none-eabi \
	--prefix=/usr \
	--with-sysroot=/usr/arm-none-eabi \
	--disable-nls \
	--disable-werror \
	--disable-gdb \
	--disable-sim \
	--enable-multilib \
	--with-system-zlib \
	--with-zstd \
	--with-xxhash \
	--without-debuginfod \
	--without-msgpack \
	--with-pkgversion="KDOS"
make
make DESTDIR=$PKG install
rm -rf "$PKG/usr/share/info" "$PKG/usr/lib/bfd-plugins"
rm -f "$PKG"/usr/share/man/man1/arm-none-eabi-{dlltool,windmc,windres}.1

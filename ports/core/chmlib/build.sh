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

# Debian's patch adds the headers chm_http.c calls into, which a C99 compiler
# otherwise rejects as implicit declarations. --disable-io64: musl's off_t is
# 64-bit, so plain pread and lseek already reach every offset, and musl exports
# no pread64 or lseek64 symbol for the LFS64 path to link. --enable-examples
# builds extract_chmLib, enum_chmLib, enumdir_chmLib, chm_http and test_chmLib.
patch -p1 -i "$PORT_SRC/0001-chmlib-implicit-declarations.patch"

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--enable-pthread \
	--enable-pread \
	--disable-io64 \
	--enable-examples \
	--disable-static
make
make DESTDIR=$PKG install

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

export CFLAGS="$CFLAGS -Wno-unterminated-string-initialization"
# Each --enable here only permits the probe: a missing header or library turns
# the feature off without an error, so acl and zlib in depends are what keep
# ACLs and zisofs in the image. libjte (jigdo templates) is not a port.
./configure --prefix=/usr \
	--enable-libacl \
	--enable-xattr \
	--enable-zlib \
	--enable-lfa-flags \
	--enable-projid \
	--disable-libjte
make
make -j1 DESTDIR=$PKG install

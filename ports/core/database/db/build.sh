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

patch -p1 -i "$PORT_SRC/atomic.patch"

export CFLAGS="$CFLAGS -fpermissive"

cd build_unix
../dist/configure --prefix=/usr --libdir=/usr/lib \
	--enable-shared \
	--disable-static \
	--enable-compat185 \
	--enable-cxx
make

make DESTDIR=$PKG install_include install_lib install_utilities
install -Dm644 ../LICENSE "$PKG/usr/share/licenses/db/LICENSE"
test -e "$PKG/usr/include/db.h"
test -e "$PKG/usr/lib/libdb.so"

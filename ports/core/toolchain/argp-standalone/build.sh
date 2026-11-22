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

autoreconf -fi
./configure \
	--prefix=/usr \
	--sysconfdir=/etc

make CFLAGS="$CFLAGS -g0"

mkdir -p $PKG/usr/{include,lib}
install -Dm644 argp.h $PKG/usr/include/argp.h
install -Dm644 libargp.a $PKG/usr/lib/libargp.a

install -Dm644 "$SRC/README.md" "$PKG/usr/share/licenses/$name/README.md"
install -Dm644 /usr/share/licenses/gcc/COPYING.LIB "$PKG/usr/share/licenses/$name/COPYING.LIB"

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

patch -p1 -i "$PORT_SRC/rnnoise-fix-compilation-errors.patch"

case "$(uname -m)" in
	x86_64) _rtcd=--enable-x86-rtcd ;;
	*) _rtcd= ;;
esac
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--disable-examples --disable-doc $_rtcd
make
make DESTDIR=$PKG install

test -f "$PKG/usr/lib/librnnoise.so"
test -f "$PKG/usr/lib/pkgconfig/rnnoise.pc"

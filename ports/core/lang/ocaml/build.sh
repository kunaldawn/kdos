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

./configure \
	--prefix=/usr \
	--libdir=/usr/lib/ocaml \
	--mandir=/usr/share/man \
	--disable-ocamltest \
	--disable-debug-runtime \
	--disable-instrumented-runtime \
	CFLAGS="$CFLAGS -g0"
make
make DESTDIR=$PKG install
for f in "$PKG"/usr/bin/*; do
	if [ ! -L "$f" ] && [ "$(head -c4 "$f")" = $'\x7fELF' ]; then
		strip --strip-debug "$f"
	fi
done

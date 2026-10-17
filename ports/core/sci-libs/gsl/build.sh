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

patch -p1 -i "$PORT_SRC/openblas-default-cblas.patch"
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static
make
make DESTDIR=$PKG install

find "$PKG" -name '*.h' | while read -r h; do
	iconv -f UTF-8 -t UTF-8 "$h" >/dev/null 2>&1 && continue
	iconv -f ISO-8859-1 -t UTF-8 "$h" > "$h.utf8" && mv -f "$h.utf8" "$h"
done

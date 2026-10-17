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

for p in 10-fix-compilation-with-cpp-headers 30-fix-utf16 \
	50-remove-outdated-check 60-id3lib-missing-nullpointer-check \
	61-fix_vbr_stack_smash CVE-2007-4460 bool-typedef; do
	patch -p1 -i "$PORT_SRC/$p.patch"
done

libtoolize --force --copy --install
aclocal -I m4
autoconf
automake --add-missing --copy

./configure --prefix=/usr --libdir=/usr/lib --disable-static
make
make DESTDIR=$PKG install
nm -D "$PKG/usr/lib/libid3-3.8.so.3" | grep -q " U iconv_open"

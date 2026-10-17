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

for p in format-security musl-fix-includes jfsutils-include-sysmacros \
	missing-stdinth gcc-10 fflush-stdout gnu-basename; do
	patch -p1 -i "$PORT_SRC/$p.patch"
done

./configure --prefix=/usr --sbindir=/usr/sbin --mandir=/usr/share/man
make
make -j1 DESTDIR=$PKG install

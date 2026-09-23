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
	--shared-cares \
	--shared-zlib \
	--shared-libuv \
	--with-intl=system-icu
make
make DESTDIR=$PKG install
for d in "$PKG"/usr/lib/node_modules/npm/man/man*; do
	install -Dm644 -t "$PKG/usr/share/man/${d##*/}" "$d"/*
done

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

./configure --prefix=/usr     \
  	            --sysconfdir=/etc \
  	            --enable-utf8     \
	    --enable-libmagic \
	    --disable-nls
make
make DESTDIR=$PKG install

install -Dm644 /dev/stdin "$PKG/etc/nanorc" <<'NANORC'
include "/usr/share/nano/*.nanorc"
include "/usr/share/nano/extra/*.nanorc"
NANORC

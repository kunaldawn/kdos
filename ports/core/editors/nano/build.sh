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

# THE SYSTEM nanorc IS WHAT LOADS THE SYNTAX FILES. `make install` puts them
# in /usr/share/nano and writes nothing to /etc, and nano reads no definition
# it is not told to include: without this file every buffer is uncoloured,
# and --enable-libmagic has nothing to act on, because the `magic` lines that
# match a file by its contents are inside those definitions.
install -Dm644 /dev/stdin "$PKG/etc/nanorc" <<'NANORC'
include "/usr/share/nano/*.nanorc"
include "/usr/share/nano/extra/*.nanorc"
NANORC

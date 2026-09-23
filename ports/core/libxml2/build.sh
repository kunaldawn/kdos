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

export PYTHON=/usr/bin/python3
./configure \
	--prefix=/usr \
	--with-icu \
	--with-history \
	--with-zlib \
	--with-python
make
make DESTDIR=$PKG install
install -Dm644 doc/xml2-config.1 dist-doc/xmllint.1 dist-doc/xmlcatalog.1 \
	-t "$PKG/usr/share/man/man1"

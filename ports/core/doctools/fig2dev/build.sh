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

./configure --prefix=/usr \
	--with-png \
	--with-rgbfile=/usr/share/X11/rgb.txt \
	--disable-transfig \
	GSEXE=gs
make
make DESTDIR=$PKG install
install -d "$PKG/usr/share/licenses/$name"
head -n 17 fig2dev/fig2dev.c > "$PKG/usr/share/licenses/$name/LICENSE"

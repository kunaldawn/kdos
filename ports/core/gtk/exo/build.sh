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

# Xfce's extension library, which Xfburn 0.8 links against libxfce4ui 4.20
# (the 4.21 series folds it in). exo-desktop-item-edit and exo-open install
# with it; neither has a menu entry of its own.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--enable-gio-unix \
	--disable-gtk-doc
make
make DESTDIR=$PKG install

# Bundled data is English only; the library falls back to its source strings.
rm -rf "$PKG/usr/share/locale"

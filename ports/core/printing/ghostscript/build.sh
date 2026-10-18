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

rm -rf brotli freetype jbig2dec jpeg libpng tiff zlib openjpeg
patch -p1 -i "$PORT_SRC/sprintf-guard.patch"

CC="gcc -std=gnu11" CFLAGS="${CFLAGS/-std=gnu11/}" ./configure --prefix=/usr --libdir=/usr/lib \
	--with-system-libtiff --disable-gtk \
	--with-drivers=ALL --without-x \
	--enable-fontconfig \
	--enable-cups --with-pdftoraster \
	--enable-openjpeg \
	--with-jbig2dec \
	--with-libidn \
	--with-libpaper \
	--disable-dbus
make so
make DESTDIR=$PKG soinstall
ln -sf gsc "$PKG/usr/bin/gs"

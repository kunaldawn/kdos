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
	--disable-nls \
	--disable-java \
	--disable-csharp \
	--without-git \
	--with-xz \
	--without-emacs \
	--with-included-libxml \
	--with-libunistring-prefix=/usr \
	--with-libncurses-prefix=/usr \
	--without-selinux \
	--without-libsmack
make
make DESTDIR=$PKG install

rm -f "$PKG/usr/share/gettext/javaversion.class"

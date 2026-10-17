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

patch -p1 -i "$PORT_SRC/utmps-paths.patch"
./configure \
	--prefix=/usr \
	--disable-chfn-chsh \
	--disable-login \
	--disable-nologin \
	--disable-su \
	--enable-setpriv \
	--with-cap-ng \
	--disable-runuser \
	--disable-pylibmount \
	--without-python \
	--without-systemd \
	--without-systemdsystemunitdir \
	--without-udev \
	--disable-libmount-udev-support \
	--with-libz \
	--with-btrfs \
	--with-readline \
	--with-ncursesw \
	--enable-liblastlog2 \
	--without-libmagic \
	--disable-pam-lastlog2 \
	--without-econf \
	--without-user \
	--disable-asciidoc \
	--disable-poman \
	--disable-nls \
	--disable-makeinstall-chown \
	--disable-makeinstall-setuid \
	CPPFLAGS=-I/usr/include/utmps
make LIBS="-lutmps -lskarnet"
make LIBS="-lutmps -lskarnet" DESTDIR=$PKG install

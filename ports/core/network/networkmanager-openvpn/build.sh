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

./autogen.sh \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--libexecdir=/usr/lib \
	--localstatedir=/var \
	--without-gnome \
	--with-gtk4=no \
	--disable-static
make
make DESTDIR=$PKG install

# sysusers.d and tmpfiles.d land in $(prefix)/lib whatever the options, and
# nothing here reads either: postinstall.sh makes the account and its chroot.
rm -rf "$PKG/usr/lib/sysusers.d" "$PKG/usr/lib/tmpfiles.d"

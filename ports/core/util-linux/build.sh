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
	--disable-chfn-chsh \
	--disable-login \
	--disable-nologin \
	--disable-su \
	--disable-setpriv \
	--disable-runuser \
	--disable-pylibmount \
	--without-python \
	--without-systemd \
	--without-systemdsystemunitdir \
	--disable-makeinstall-chown \
	--disable-makeinstall-setuid
make
make DESTDIR=$PKG install

# A COPY UNDER A NAME NOTHING ELSE CLAIMS. toybox owns /usr/sbin/switch_root on
# the finished image and is installed after this, so the file the initramfs
# copies is toybox's applet — which chroot()s into the new root and never does
# mount(newroot, "/", MS_MOVE). Every process on the booted machine is then
# chrooted for ever, and a chrooted caller cannot create a user namespace at
# all: no container starts, for root as much as for anybody.
install -Dm755 "$PKG/sbin/switch_root" "$PKG/usr/sbin/switch_root.real"

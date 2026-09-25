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

# THE SHIPPED BUILD IS THE 03_phase3 ONE. 04_phase4 names this port again, but
# kpkg skips a package whose recorded recipe hash matches the tree, so every
# probe is pinned against what 03_phase3 can reach.
#
# --without-udev: eudev depends on util-linux for libblkid, so libudev cannot be
# declared and the bootstrap build never finds it. lsblk then reads a
# filesystem's type, label and UUID only by probing the device, which it
# attempts only as root, so for anybody else those columns are empty.
#
# --disable-asciidoc: the release tarball carries every manual page already
# generated, and they install without asciidoctor. Regenerating them would pull
# ruby into the bootstrap.
#
# libmagic (file), pam_lastlog2 (pam) and NLS (gettext) all come later than this
# port in 03_phase3; declaring any of them would drag it into the bootstrap.
# libmount's own udev reader is built on libsystemd.
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

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

# THE SAME RECIPE BUILDS IN 03_phase3 AND AGAIN IN 04_phase4, so every probe is
# pinned against what 03_phase3 can reach.
#
# udev is the one feature that follows the phase, and it has to: eudev depends
# on util-linux, so it cannot be a depend here, and 03_phase3 builds this port
# before eudev while 04_phase4 builds it after. The shipped build is the
# phase-4 one, and it must have udev — without it lsblk, run by anybody but
# root, reports no filesystem type, label or UUID. The choice is stated from
# libudev's presence rather than left to the probe.
#
# --disable-asciidoc: the release tarball carries every manual page already
# generated, and they install without asciidoctor. Regenerating them would pull
# ruby into the bootstrap.
#
# libmagic (file), pam_lastlog2 (pam) and NLS (gettext) all come later than this
# port in 03_phase3; declaring any of them would drag it into the bootstrap.
# libmount's own udev reader is off in both phases: it is built on libsystemd.
if pkg-config --exists libudev; then
	_udev=--with-udev
else
	_udev=--without-udev
fi

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
	$_udev \
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

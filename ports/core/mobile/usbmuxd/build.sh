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

# The snapshot has no configure. AC_INIT reads the version through
# git-version-gen, which with no .git and no .tarball-version returns whatever
# RELEASE_VERSION holds; left empty, configure stops for want of a version.
RELEASE_VERSION=$version autoreconf -fi

# --without-systemd makes the udev rule start the daemon itself
# (`usbmuxd --user usbmux --udev`) rather than asking systemd to. That rule is
# the only thing that starts usbmuxd: the first iPhone plugged in starts it,
# each later one tells the running instance to look, and it exits with the
# last device unplugged. postinstall.sh makes the usbmux account it drops to,
# and the same rule gives that account the device node.
# --with-preflight is the default and is spelled out: without it a newly
# plugged device is never paired and every client sees it locked.
./configure --prefix=/usr --sbindir=/usr/sbin --sysconfdir=/etc \
	--localstatedir=/var \
	--without-systemd \
	--with-preflight \
	--with-udevrulesdir=/usr/lib/udev/rules.d
make
make DESTDIR=$PKG install

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

RELEASE_VERSION=$version autoreconf -fi

./configure --prefix=/usr --sbindir=/usr/sbin --sysconfdir=/etc \
	--localstatedir=/var \
	--without-systemd \
	--with-preflight \
	--with-udevrulesdir=/usr/lib/udev/rules.d
make
make DESTDIR=$PKG install

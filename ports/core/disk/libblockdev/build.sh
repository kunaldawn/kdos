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

export CFLAGS="$CFLAGS -Wno-error"

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--enable-introspection=yes \
	--disable-tests \
	--without-python3 \
	--without-gtk-doc \
	--without-tools \
	--without-s390 \
	--with-nvdimm \
	--without-lvm_dbus \
	--with-escrow \
	--with-nvme \
	--with-smart \
	--with-smartmontools \
	--with-drivedb=/usr/share/smartmontools
make PYTHON=python3
make PYTHON=python3 DESTDIR=$PKG install

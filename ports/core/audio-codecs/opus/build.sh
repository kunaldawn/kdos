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

export CFLAGS="$CFLAGS -flto=auto -frandom-seed=opus"
export LDFLAGS="$LDFLAGS -flto=auto"

./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--disable-doc \
	--disable-extra-programs \
	--enable-deep-plc \
	--enable-dred \
	--enable-osce
make
make DESTDIR=$PKG install

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

patch -p1 -i $PORT_SRC/musl-stdio-freopen.patch

CONFIG_SHELL=/bin/bash  \
./configure --prefix=/usr \
	--libdir=/usr/lib \
	--libexecdir=/usr/lib \
	--exec-prefix= \
	--enable-cmdlib \
	--enable-pkgconfig \
	--enable-udev_sync
make 
make DESTDIR=$PKG install_lvm2 install_device-mapper

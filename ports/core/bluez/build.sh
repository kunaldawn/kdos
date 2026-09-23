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

./configure --prefix=/usr \
            --sysconfdir=/etc \
            --libexecdir=/usr/lib \
            --localstatedir=/var \
            --enable-library \
            --disable-systemd \
            --without-libical \
            --disable-obex
make
make DESTDIR=$PKG install

install -d $PKG/usr/sbin
ln -svf ../lib/bluetooth/bluetoothd $PKG/usr/sbin/bluetoothd

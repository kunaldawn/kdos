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
            --enable-obex \
            --enable-cups \
            --enable-midi \
            --enable-deprecated \
            --enable-sixaxis \
            --enable-hid2hci \
            --disable-mesh \
            --disable-btpclient \
            --disable-external-ell
make
make DESTDIR=$PKG install

install -d $PKG/usr/sbin
ln -svf ../lib/bluetooth/bluetoothd $PKG/usr/sbin/bluetoothd

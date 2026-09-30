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

# --enable-cups INSTALLS THE BLUETOOTH PRINTER BACKEND only when configure can
# read cups' serverbin from pkg-config, and says nothing when it cannot, so
# cups is in `depends` to make the answer the same in every build order.
#
# OBEX (obexd, on the session bus) needs libical's libicalvcal; its phonebook
# stays the default dummy plugin, since the alternative is evolution-data-server.
# --enable-deprecated installs hciattach, rfcomm, hcitool and the rest of the
# pre-D-Bus tools, which UART adapters and serial-over-Bluetooth still need.
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

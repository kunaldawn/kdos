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

# main configuration
install -v -dm755 $PKG/etc/bluetooth
install -v -m644 src/main.conf $PKG/etc/bluetooth/main.conf

cat > $PKG/etc/bluetooth/rfcomm.conf << "EOF"
# Start rfcomm.conf
# Set up the RFCOMM configuration of the Bluetooth subsystem in the Linux kernel.
# Use one line per command
# See the rfcomm man page for options

# End of rfcomm.conf
EOF

cat > $PKG/etc/bluetooth/uart.conf << "EOF"
# Start uart.conf
# Attach serial devices via UART HCI to BlueZ stack
# Use one line per device
# See the hciattach man page for options
#
# End of uart.conf
EOF

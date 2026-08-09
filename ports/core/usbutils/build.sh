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

meson setup build \
    --prefix=/usr \
    --datadir=/usr/share/hwdata \
    --buildtype=release
meson compile -C build
meson install -C build --destdir=$PKG

# Install usb.ids manually as it is not part of the source tarball
install -Dm644 $PORT_SRC/usb.ids $PKG/usr/share/hwdata/usb.ids

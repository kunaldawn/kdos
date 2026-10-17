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

autoreconf -fi

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-x11 \
	--disable-systemd \
	--enable-drm \
	--enable-udev \
	--enable-dbus \
	--disable-usb

make
make DESTDIR=$PKG install

rm -f "$PKG/usr/lib/udev/rules.d/60-ddcutil-i2c.rules"
rmdir --ignore-fail-on-non-empty "$PKG/usr/lib/udev/rules.d" 2>/dev/null || true

rm -f "$PKG/usr/lib/modules-load.d/ddcutil.conf"
rmdir --ignore-fail-on-non-empty "$PKG/usr/lib/modules-load.d" 2>/dev/null || true

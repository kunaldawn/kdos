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

meson setup build --buildtype=release \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	-Dinitscriptdir= \
	-Dudevrulesdir=/lib/udev/rules.d \
	-Denable-io-uring=true \
	-Dexamples=false \
	-Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
readelf -d "$PKG"/usr/lib/libfuse3.so.*.*.* | grep -q 'NEEDED.*\[liburing\.so'

rm -rf "$PKG/dev"

ln -s fusermount3 "$PKG/usr/bin/fusermount"

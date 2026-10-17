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

patch -p1 -i "$PORT_SRC/dont-assume-all-GdkDisplays-are-GdkX11Displays.patch"

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--enable-alsa --enable-pulse --enable-gstreamer --enable-null \
	--enable-udev --disable-oss --enable-tdb \
	--enable-gtk3 --disable-gtk \
	--with-systemdsystemunitdir=no --disable-lynx --disable-gtk-doc
make
make -j1 DESTDIR=$PKG install

rm -rf "$PKG/usr/share/gtk-doc"
rm -rf "$PKG/usr/share/gdm" "$PKG/usr/share/gnome" \
	"$PKG/usr/lib/gnome-settings-daemon-3.0" "$PKG/usr/bin/canberra-boot"
readelf -d "$PKG/usr/lib/libcanberra.so" | grep -q "NEEDED.*libtdb"

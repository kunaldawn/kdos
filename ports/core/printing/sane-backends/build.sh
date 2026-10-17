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

echo "$version" > .tarball-version

./autogen.sh
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static --with-usb --without-snmp --disable-locking \
	--with-avahi --with-libcurl --with-usb-record-replay --with-v4l \
	--without-gphoto2 --with-poppler-glib --without-systemd
make
make DESTDIR=$PKG install

descs=doc/descriptions:doc/descriptions-external
install -d "$PKG/usr/lib/udev/rules.d" "$PKG/usr/lib/udev/hwdb.d"
tools/sane-desc -m udev+hwdb -s $descs -d 0 > "$PKG/usr/lib/udev/rules.d/65-libsane.rules"
tools/sane-desc -m hwdb -s $descs -d 0 > "$PKG/usr/lib/udev/hwdb.d/20-sane.hwdb"

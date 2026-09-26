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

# The udev directory is /usr/lib/udev, where eudev reads rules and where kpkg
# rebuilds the hwdb from: 65-libwacom.hwdb is what tags a tablet's event nodes
# ID_INPUT_TABLET, and a hwdb file that never reaches hwdb.bin tags nothing.
#
# libwacom-update-db ends by running systemd-hwdb, which this host does not
# have: a tablet described in /etc/libwacom is registered with
# `libwacom-update-db --skip-systemd-hwdb-update` and then
# `sudo udevadm hwdb --update`. libwacom-show-stylus imports the pyudev and
# libevdev python modules, which are not ports, so it would fail on first use
# and is not shipped.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib --buildtype=release \
	-Dudev-dir=/usr/lib/udev \
	-Dtests=disabled \
	-Ddocumentation=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
rm "$PKG/usr/bin/libwacom-show-stylus" "$PKG/usr/share/man/man1/libwacom-show-stylus.1"

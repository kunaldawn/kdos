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

# The PCI and USB id databases are hwdata's. Given no path, setup probes four
# locations and, finding none, downloads pci.ids and usb.ids from the
# network, which fails here with no network; naming hwdata's files makes the
# build independent of what setup can see. libsoup fetches install media and
# tree metadata by URL when an application asks; 3.0 is the ABI ported.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Denable-introspection=enabled \
	-Denable-vala=enabled \
	-Denable-gtk-doc=false \
	-Denable-tests=false \
	-Dlibsoup-abi=3.0 \
	-Dwith-pci-ids-path=/usr/share/hwdata/pci.ids \
	-Dwith-usb-ids-path=/usr/share/hwdata/usb.ids
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

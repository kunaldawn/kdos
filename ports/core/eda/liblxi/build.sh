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

patch -p1 -i "$PORT_SRC/link-tirpc.patch"

meson setup build \
	--prefix=/usr \
	--libdir=lib \
	--buildtype=release \
	-Db_lundef=true
grep -q -- -DHAVE_AVAHI build/build.ninja || {
	echo 'liblxi: avahi-client/client.h not found, mDNS discovery would be missing' >&2
	exit 1
}
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

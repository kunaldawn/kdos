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

tar xzf "$PORT_SRC/${name}-${version}.tar.gz"

patch -p1 -i "$PORT_SRC/musl-statx.patch"

patch -p1 -i "$PORT_SRC/locate-path-replaces-default.patch"

meson setup build --buildtype=release \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--localstatedir=/var \
	-Dinstall_systemd=false \
	-Dinstall_cron=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

chmod 0755 "$PKG/usr/bin/plocate"

ln -s plocate "$PKG/usr/bin/locate"

rm -rf "$PKG/var/lib/plocate"
rmdir --ignore-fail-on-non-empty "$PKG/var/lib" 2>/dev/null || true

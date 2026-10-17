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

patch -p1 -i "$PORT_SRC/no-agent.patch"
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--libexecdir=/usr/lib \
	--buildtype=release \
	-Dlibgeoclue=true \
	-Dintrospection=false \
	-Dvapi=false \
	-Dgtk-doc=false \
	-Dwifi-source=true \
	-D3g-source=true \
	-Dcdma-source=true \
	-Dmodem-gps-source=true \
	-Dnmea-source=true \
	-Dstatic-source=true \
	-Dip-source=true \
	-Dcompass=true \
	-Denable-backend=true \
	-Ddemo-agent=false \
	-Ddbus-srv-user=geoclue
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

rm -rf "$PKG/usr/lib/sysusers.d" "$PKG/usr/lib/systemd"

rm -f "$PKG/usr/share/applications/geoclue-where-am-i.desktop"
rmdir "$PKG/usr/share/applications" 2>/dev/null || true

install -Dm644 /dev/stdin "$PKG/etc/geoclue/conf.d/90-kdos.conf" <<'CONF'
[agent]
whitelist=

[xdg-desktop-portal]
allowed=true
system=true
users=
CONF

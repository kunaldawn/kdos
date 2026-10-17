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

patch -p1 -i "$PORT_SRC/fix_amd_npu_libc_type.patch"

tar xf "$PORT_SRC/$name-vendor-$version.tar.xz"
export CARGO_NET_OFFLINE=true
export RUSTFLAGS="-C target-feature=-crt-static"

meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dprofile=default
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s data/icons/net.nokyan.Resources.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/net.nokyan.Resources.png"
done

cat > "$PKG/usr/share/applications/net.nokyan.Resources.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Resources (GNOME)
GenericName=System Monitor
Comment=Keep an eye on system resources
Exec=resources
Icon=net.nokyan.Resources
Terminal=false
StartupNotify=true
StartupWMClass=net.nokyan.Resources
Categories=GTK;System;Monitor;
Keywords=system;resources;monitor;processes;usage;task;manager;cpu;ram;memory;gpu;npu;performance;power;battery;
DESKTOP
chmod 644 "$PKG/usr/share/applications/net.nokyan.Resources.desktop"

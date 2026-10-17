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

tar xf "$PORT_SRC/$name-vendor-$version.tar.xz"
export CARGO_NET_OFFLINE=true
export OPENSSL_NO_VENDOR=1
export RUSTFLAGS="-C target-feature=-crt-static"

meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dprofile=default
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s \
		data/resources/icons/hicolor/scalable/apps/io.gitlab.adhami3310.Impression.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/io.gitlab.adhami3310.Impression.png"
done

cat > "$PKG/usr/share/applications/io.gitlab.adhami3310.Impression.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Impression
GenericName=Media Writer
Comment=Create bootable drives
TryExec=impression
Exec=impression %u
Icon=io.gitlab.adhami3310.Impression
Terminal=false
StartupNotify=true
StartupWMClass=io.gitlab.adhami3310.Impression
Categories=GTK;GNOME;Utility;System;DiscBurning;
Keywords=usb;flash;writer;bootable;drive;iso;img;disk;image;media;creator;live;
DESKTOP
chmod 644 "$PKG/usr/share/applications/io.gitlab.adhami3310.Impression.desktop"

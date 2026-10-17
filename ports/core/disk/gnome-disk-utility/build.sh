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

meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release --wrap-mode=nodownload \
	-Dlogind=none \
	-Dgsd_plugin=false \
	-Dman=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s data/icons/hicolor/scalable/apps/org.gnome.DiskUtility.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/org.gnome.DiskUtility.png"
done

rm -f "$PKG/usr/share/applications/gnome-disk-image-mounter.desktop" \
	"$PKG/usr/share/applications/gnome-disk-image-writer.desktop"

cat > "$PKG/usr/share/applications/org.gnome.DiskUtility.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Disks (GNOME)
GenericName=Disk Utility
Comment=Manage drives and media
Exec=gnome-disks
Icon=org.gnome.DiskUtility
Terminal=false
StartupNotify=true
StartupWMClass=gnome-disks
Categories=GTK;Utility;System;
Keywords=disk;drive;volume;harddisk;hdd;disc;partition;format;iso;image;backup;restore;benchmark;raid;luks;encryption;smart;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.gnome.DiskUtility.desktop"

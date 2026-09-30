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

# Built with no logind: it only lets the program inhibit suspend while a job
# runs, through a session manager this system does not have. The
# gnome-settings-daemon plug-in (the SMART warning notifier) needs that daemon
# and is not built. Every disk operation goes through udisksd, and polkit
# decides who may. The schema, icon and desktop caches are shared indexes
# that kpkg rebuilds on install.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release --wrap-mode=nodownload \
	-Dlogind=none \
	-Dgsd_plugin=false \
	-Dman=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# Bundled data is English only; GTK falls back to the source strings.
rm -rf "$PKG/usr/share/locale"

# THE MENU ICON: upstream ships it as SVG only, which the panel never reads.
for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s data/icons/hicolor/scalable/apps/org.gnome.DiskUtility.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/org.gnome.DiskUtility.png"
done

# The image mounter and image writer entries are hidden handlers that exist
# for their claim on disc and raw images, which K3b and the archive opener
# already make; a second claim would open those files in whichever entry
# sorted first. The programs stay: gnome-disk-image-mounter and
# gnome-disks --restore-disk-image.
rm -f "$PKG/usr/share/applications/gnome-disk-image-mounter.desktop" \
	"$PKG/usr/share/applications/gnome-disk-image-writer.desktop"

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: GTK 3 takes the Wayland
# app_id from the program name, gnome-disks, not from the GtkApplication id.
# DBusActivatable is dropped, so the launcher starts the program named in
# Exec. The name is qualified: the Disks role is KDOS's own.
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

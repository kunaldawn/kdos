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
	--buildtype=release \
	-Dpackagekit=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

find "$PKG/usr/share/help" -mindepth 1 -maxdepth 1 ! -name C -exec rm -rf {} +

rm -rf "$PKG/etc"

for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s data/icons/org.gnome.DejaDup.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/org.gnome.DejaDup.png"
done

cat > "$PKG/usr/share/applications/org.gnome.DejaDup.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Déjà Dup Backups
GenericName=Backup
Comment=Protect yourself from data loss
TryExec=deja-dup
Exec=deja-dup %u
Icon=org.gnome.DejaDup
Terminal=false
StartupNotify=true
StartupWMClass=org.gnome.DejaDup
SingleMainWindow=true
Categories=GTK;GNOME;Utility;Archiving;
Keywords=deja;dup;backup;backups;back up;restore;snapshot;restic;
Actions=backup;

[Desktop Action backup]
Name=Back Up
Exec=deja-dup --backup
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.gnome.DejaDup.desktop"

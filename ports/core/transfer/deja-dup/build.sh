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

# Déjà Dup is a front end: restic stores the backups, rclone reaches cloud
# remotes, and fusermount mounts a snapshot for browsing. Each is found on
# PATH at run time. Duplicity, the previous default, is not a port, so an
# existing Duplicity backup cannot be restored here; new backups use restic.
# PackageKit, which would offer to install a missing tool over the network, is
# off. The Google Drive and OneDrive locations sign in through a browser and
# work only with a network; local disks, folders and restic repositories do
# not need one. The passphrase is kept through libsecret, in whatever answers
# org.freedesktop.secrets.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dpackagekit=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# Bundled data is English only: GTK falls back to the source strings, and the
# help keeps its C pages.
rm -rf "$PKG/usr/share/locale"
find "$PKG/usr/share/help" -mindepth 1 -maxdepth 1 ! -name C -exec rm -rf {} +

# The scheduler, /usr/libexec/deja-dup/deja-dup-monitor, is started by
# kdos-desktop-start: nothing here reads /etc/xdg/autostart, so upstream's
# entry there, the only file upstream installs under /etc, would name a start
# path that does not exist.
rm -rf "$PKG/etc"

# THE MENU ICON: upstream ships it as SVG only, which the panel never reads.
for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s data/icons/org.gnome.DejaDup.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/org.gnome.DejaDup.png"
done

# UPSTREAM'S ENTRY IS REPLACED: the GtkApplication id is the Wayland app_id.
# MimeType is left out: it claims the two OAuth callback schemes, which only
# a signed-in cloud location uses. DBusActivatable is dropped with it, so the
# launcher starts the program named in Exec.
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

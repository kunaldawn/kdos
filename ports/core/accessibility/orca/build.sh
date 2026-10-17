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

meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dmathcat=false \
	-Dspiel=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

rm -rf "$PKG/usr/lib/systemd"

find "$PKG/usr/share/help" -mindepth 1 -maxdepth 1 ! -name C -exec rm -rf {} +

cat > "$PKG/usr/share/applications/orca.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Orca Screen Reader
GenericName=Screen Reader
Comment=Read the screen aloud and in braille
TryExec=orca
Exec=orca --replace
Icon=orca
Terminal=false
StartupWMClass=orca
Categories=GTK;Utility;Accessibility;
Keywords=screen reader;speech;braille;blind;accessibility;a11y;orca;
Actions=preferences;

[Desktop Action preferences]
Name=Preferences
Exec=orca --setup
DESKTOP
chmod 644 "$PKG/usr/share/applications/orca.desktop"

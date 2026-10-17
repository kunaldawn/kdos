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
	-Dbrasero=enabled \
	-Ddaap=disabled \
	-Dfm_radio=enabled \
	-Dgrilo=disabled \
	-Dgudev=enabled \
	-Dipod=enabled \
	-Dlibnotify=enabled \
	-Dlibsecret=enabled \
	-Dlirc=enabled \
	-Dmtp=enabled \
	-Dplugins_python=enabled \
	-Dplugins_vala=enabled \
	-Dsample-plugins=false \
	-Dhelp=true \
	-Dapidoc=false \
	-Dtests=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

find "$PKG/usr/share/help" -mindepth 1 -maxdepth 1 ! -name C -exec rm -rf {} +

for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s data/icons/hicolor/scalable/apps/org.gnome.Rhythmbox3.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/org.gnome.Rhythmbox3.png"
done

cat > "$PKG/usr/share/applications/org.gnome.Rhythmbox3.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Rhythmbox
GenericName=Music Player
Comment=Play and organize your music collection
TryExec=rhythmbox
Exec=rhythmbox %U
Icon=org.gnome.Rhythmbox3
Terminal=false
StartupNotify=true
StartupWMClass=org.gnome.Rhythmbox3
Categories=GTK;AudioVideo;Audio;Player;
Keywords=audio;song;mp3;cd;podcast;mtp;playlist;radio;music;rhythmbox;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.gnome.Rhythmbox3.desktop"

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
	--prefix=/usr \
	--libdir=lib \
	--buildtype=release \
	-Dqt=true \
	-Dqt5=false \
	-Dgtk=false \
	-Dgtk2=false \
	-Ddbus=true \
	-Dlibarchive=true \
	-Dvalgrind=false \
	-Dbuildstamp=KDOS
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

cat > "$PKG/usr/share/applications/audacious.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Audacious
GenericName=Music Player
Comment=Playlist-oriented music player
TryExec=audacious
Exec=audacious %U
Icon=audacious
Terminal=false
StartupNotify=false
StartupWMClass=audacious
SingleMainWindow=true
Categories=AudioVideo;Audio;Player;Qt;
Keywords=music;player;playlist;audio;mp3;flac;winamp;audacious;
DESKTOP
chmod 644 "$PKG/usr/share/applications/audacious.desktop"

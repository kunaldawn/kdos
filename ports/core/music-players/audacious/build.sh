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

# The Qt 6 interface only: GTK is off, so the player and its plugins link one
# toolkit. D-Bus is the MPRIS and remote-control interface that
# audacious-plugins' mpris2 plugin and the media keys reach. libarchive lets a
# playlist entry name a file inside an archive. The build stamp is fixed so
# two builds of the same source print the same About text.
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

# Bundled data is English only; Qt falls back to the source strings.
rm -rf "$PKG/usr/share/locale"

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: the Wayland app_id is the
# executable's name, audacious. MimeType is left out: Strawberry is the music
# player that claims audio files, and a second claim would pick whichever
# entry sorted first. The 48x48 hicolor PNG it names is upstream's, installed
# above.
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

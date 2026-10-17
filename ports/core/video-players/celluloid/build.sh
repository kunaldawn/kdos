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
	--buildtype=release
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s data/io.github.celluloid_player.Celluloid.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/io.github.celluloid_player.Celluloid.png"
done

cat > "$PKG/usr/share/applications/io.github.celluloid_player.Celluloid.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Celluloid
GenericName=Video Player
Comment=Play movies and videos
TryExec=celluloid
Exec=celluloid %U
Icon=io.github.celluloid_player.Celluloid
Terminal=false
StartupNotify=true
StartupWMClass=io.github.celluloid_player.Celluloid
Categories=GTK;AudioVideo;Player;Video;
Keywords=video;movie;film;clip;player;dvd;mpv;celluloid;
Actions=new-window;

[Desktop Action new-window]
Name=New Window
Exec=celluloid --new-window
DESKTOP
chmod 644 "$PKG/usr/share/applications/io.github.celluloid_player.Celluloid.desktop"

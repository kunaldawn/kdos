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

# A GTK 4 and libadwaita front end over libmpv: mpv decodes and renders into a
# GtkGLArea through the render API, so it plays whatever the mpv port plays,
# with its hardware decoding, and reads ~/.config/mpv. The project has no
# build options; its tests validate the metadata and are not run.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# Bundled data is English only; GTK falls back to the source strings.
rm -rf "$PKG/usr/share/locale"

# THE MENU ICON: upstream ships it as SVG only, which the panel never reads.
for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s data/io.github.celluloid_player.Celluloid.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/io.github.celluloid_player.Celluloid.png"
done

# UPSTREAM'S ENTRY IS REPLACED: the GtkApplication id is the Wayland app_id.
# MimeType is left out: mpv's entry claims the video and audio types, and
# mimeapps.list is where the default player is chosen. DBusActivatable is
# dropped with it, so the launcher starts the program named in Exec.
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

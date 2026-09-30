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

# Online playback goes through yt-dlp, found at run time; with none installed
# the player plays local files and streams mpv opens itself.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# English only: KDE apps fall back to their source strings.
rm -rf "$PKG/usr/share/locale"

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: the Wayland app_id is the
# desktop file name Haruna sets, org.kde.haruna. It keeps upstream's MimeType
# list, which mpv's entry shares, so the desktop's mimeapps.list is what makes
# Haruna the player a video opens in. The hicolor PNGs are upstream's.
cat > "$PKG/usr/share/applications/org.kde.haruna.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Haruna
GenericName=Video Player
Comment=Play video and audio files
Exec=haruna %U
Icon=haruna
Terminal=false
StartupWMClass=org.kde.haruna
Categories=Qt;KDE;AudioVideo;Player;Video;
MimeType=video/mp4;video/x-matroska;video/mpeg;video/ogg;video/quicktime;video/vnd.avi;video/mp2t;video/webm;video/x-ms-wmv;audio/aac;audio/ac3;audio/flac;audio/mp4;audio/mpeg;audio/ogg;audio/vnd.wave;audio/webm;audio/x-matroska;audio/x-mpegurl;
Keywords=video;movie;player;mpv;film;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.haruna.desktop"

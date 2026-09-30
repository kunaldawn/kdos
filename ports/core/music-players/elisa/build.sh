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

# KDE's music library player. Playback is libVLC: upstream falls back to
# QtMultimedia when libVLC is missing, so VLC is required here rather than
# probed, and every other optional component upstream marks OPTIONAL or
# RECOMMENDED (MPRIS over D-Bus, crash handling, KFileMetaData tag reading,
# the XmlGui shortcuts dialog) is required with it. UPnP support is marked
# broken upstream and needs UPNPQT, not a port, so it is disabled.
#
# The radio list it ships points at internet streams and plays nothing
# offline; the music collection is local. KF_SKIP_PO_PROCESSING: bundled data
# is English only.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LIBVLC=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6DBus=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DBusAddons=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Crash=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6FileMetaData=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6XmlGui=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_UPNPQT=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: KAboutData derives the
# desktop file name org.kde.elisa, which is the Wayland app_id, not upstream's
# "elisa". MimeType is left out: Strawberry is the music player that claims
# audio files. The hicolor PNGs it names are upstream's, installed above.
cat > "$PKG/usr/share/applications/org.kde.elisa.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Elisa
GenericName=Music Player
Comment=Play your local music collection
TryExec=elisa
Exec=elisa %U
Icon=elisa
Terminal=false
StartupWMClass=org.kde.elisa
SingleMainWindow=true
Categories=Qt;KDE;AudioVideo;Audio;Player;
Keywords=music;player;audio;library;playlist;elisa;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.elisa.desktop"

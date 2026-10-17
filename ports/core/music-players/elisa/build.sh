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

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
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

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

# ENABLE_GTK=OFF: the Qt client is the one built; the GTK one needs gtkmm 4.
# REBUILD_WEB=OFF: rebuilding the web client runs npm against the network;
# the release carries it built.
cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_SHARED_LIBS=OFF \
	-D ENABLE_DAEMON=ON \
	-D ENABLE_CLI=ON \
	-D ENABLE_UTILS=ON \
	-D ENABLE_QT=ON \
	-D USE_QT_VERSION=6 \
	-D ENABLE_GTK=OFF \
	-D ENABLE_MAC=OFF \
	-D REBUILD_WEB=OFF \
	-D INSTALL_WEB=ON \
	-D INSTALL_DOC=ON \
	-D INSTALL_LIB=OFF \
	-D ENABLE_NLS=ON \
	-D ENABLE_TESTS=OFF \
	-D ENABLE_UTP=ON \
	-D ENABLE_WERROR=OFF \
	-D RUN_CLANG_TIDY=OFF \
	-D WITH_CRYPTO=openssl \
	-D WITH_SYSTEMD=OFF \
	-D WITH_INOTIFY=ON \
	-D WITH_APPINDICATOR=OFF \
	-D USE_SYSTEM_EVENT2=ON \
	-D USE_SYSTEM_DEFLATE=ON \
	-D USE_SYSTEM_PSL=ON \
	-D USE_SYSTEM_MINIUPNPC=ON \
	-D USE_SYSTEM_DHT=OFF \
	-D USE_SYSTEM_UTP=OFF \
	-D USE_SYSTEM_B64=OFF \
	-D USE_SYSTEM_NATPMP=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

find "$PKG/usr/share/transmission/translations" -name '*.qm' ! -name '*_en*.qm' -delete

# The panel draws only PNG icons, and upstream installs the logo as SVG.
for s in 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x$s/apps"
	rsvg-convert -w $s -h $s icons/hicolor_apps_scalable_transmission.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x$s/apps/transmission-qt.png"
done

# The window's app_id is the binary's name. qBittorrent is the torrent and
# magnet handler, so this entry claims no MIME type.
cat > "$PKG/usr/share/applications/transmission-qt.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Transmission
GenericName=BitTorrent Client
Comment=Download and share files over BitTorrent
Exec=transmission-qt %U
Icon=transmission-qt
Terminal=false
StartupWMClass=transmission-qt
Categories=Network;FileTransfer;P2P;Qt;
Keywords=bittorrent;torrent;magnet;download;p2p;transmission;
DESKTOP
chmod 644 "$PKG/usr/share/applications/transmission-qt.desktop"

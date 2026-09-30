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

patch -p1 -i "$PORT_SRC/libtorrent-2.1.patch"
# Peer country lookup downloads the DB-IP database each month; the patch
# makes it opt-in, so a fresh profile makes no request of its own.
patch -p1 -i "$PORT_SRC/geoip-off-by-default.patch"

# STACKTRACE=OFF: the crash handler prints a backtrace to a dialog, and
# nothing on this system collects one.
for _gui in ON OFF; do
	cmake -S . -B build-gui-$_gui -G Ninja \
		-D CMAKE_INSTALL_PREFIX=/usr \
		-D CMAKE_INSTALL_LIBDIR=lib \
		-D CMAKE_BUILD_TYPE=Release \
		-D GUI=$_gui \
		-D WEBUI=ON \
		-D STACKTRACE=OFF \
		-D TESTING=OFF \
		-D SYSTEMD=OFF \
		-Wno-dev
	cmake --build build-gui-$_gui
done
DESTDIR=$PKG cmake --install build-gui-ON
install -Dm755 build-gui-OFF/qbittorrent-nox "$PKG/usr/bin/qbittorrent-nox"
install -Dm644 doc/en/qbittorrent-nox.1 "$PKG/usr/share/man/man1/qbittorrent-nox.1"
find "$PKG/usr/share/man" -mindepth 1 -maxdepth 1 ! -name 'man[0-9]*' -exec rm -rf {} +

# The window's app_id is org.qbittorrent.qBittorrent, which StartupWMClass
# must repeat; upstream's entry carries the X11 class.
cat > "$PKG/usr/share/applications/org.qbittorrent.qBittorrent.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=qBittorrent
GenericName=BitTorrent Client
Comment=Download and share files over BitTorrent
Exec=qbittorrent %U
Icon=qbittorrent
Terminal=false
StartupWMClass=org.qbittorrent.qBittorrent
SingleMainWindow=true
MimeType=application/x-bittorrent;x-scheme-handler/magnet;
Categories=Network;FileTransfer;P2P;Qt;
Keywords=bittorrent;torrent;magnet;download;p2p;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.qbittorrent.qBittorrent.desktop"

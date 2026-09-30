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

patch -p1 -i "$PORT_SRC/app-id.patch"

# Baloo and its widgets are required: without both, Dolphin drops the
# information panel, the metadata columns and the tag places with only a
# warning. KUserFeedback is telemetry and PackageKit a network installer for
# service menus; neither is a port and both are refused so a stray copy cannot
# be picked up. musl has no fts(3): the folder-size counter finds fts.h and
# libfts from musl-fts, and the configure links it when libc lacks fts_open.
# Konsole is the terminal panel's part and kio-extras the thumbnailers; both
# are run-time only. Devices in the Places panel come from Solid over udisks2.
# KF_SKIP_PO_PROCESSING leaves the interface catalogues out: bundled data is
# English only. kdoctools_install() builds every translated handbook and
# manual page too, and those are removed after the install.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Baloo=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6BalooWidgets=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_KF6UserFeedback=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_PackageKitQt6=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

if [ -d "$PKG/usr/share/doc/HTML" ]; then
	find "$PKG/usr/share/doc/HTML" -mindepth 1 -maxdepth 1 ! -name en -exec rm -rf {} +
fi
if [ -d "$PKG/usr/share/man" ]; then
	find "$PKG/usr/share/man" -mindepth 1 -maxdepth 1 ! -name 'man*' -exec rm -rf {} +
fi

# THE MENU ICON: the entry names org.kde.dolphin, which Dolphin does not ship;
# Breeze draws it as system-file-manager, in SVG, and the panel reads only
# hicolor PNGs.
for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s /usr/share/icons/breeze/apps/48/system-file-manager.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/org.kde.dolphin.png"
done

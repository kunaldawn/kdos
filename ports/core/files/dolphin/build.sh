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

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
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
rm -rf "$PKG/usr/lib/systemd"

if [ -d "$PKG/usr/share/man" ]; then
	find "$PKG/usr/share/man" -mindepth 1 -maxdepth 1 ! -name 'man*' -exec rm -rf {} +
fi

for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s /usr/share/icons/breeze/apps/48/system-file-manager.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/org.kde.dolphin.png"
done

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

tar -xf "$PORT_SRC/$name-$version.tar.zst" --strip-components=1

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D ENABLE_KCM=ON \
	-D ENABLE_CONFIG_QT=ON \
	-D ENABLE_TEST=OFF \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DBusAddons=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6WindowSystem=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Svg=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

desktop-file-edit --set-key=NoDisplay --set-value=true \
	"$PKG/usr/share/applications/org.fcitx.fcitx5-migrator.desktop"

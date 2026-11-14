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

patch -p1 -i "$PORT_SRC/potd-default-provider.patch"
tar xf "$PORT_SRC/$name-vendor-$version.tar.xz" -C kdeds/kameleon/qmk/kameleon-qmk-helper
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_ICU=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_Qt6WebEngineQuick=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Quick3D=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6KirigamiAddons=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6ItemModels=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
rm "$PKG/usr/lib/qt6/plugins/plasma/applets/org.kde.plasma.comic.so" \
	"$PKG/usr/lib/qt6/plugins/kf6/packagestructure/plasma_comic.so" \
	"$PKG/usr/share/knsrcfiles/comic.knsrc" \
	"$PKG/usr/lib/qt6/plugins/potd/plasma_potd_apodprovider.so"

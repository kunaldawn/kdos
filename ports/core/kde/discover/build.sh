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

patch -p1 -i "$PORT_SRC/odrs-offline.patch"
patch -p1 -i "$PORT_SRC/sysupdate-errors.patch"
patch -p1 -i "$PORT_SRC/ratings-need-reviews.patch"
patch -p1 -i "$PORT_SRC/sysupdate-size.patch"
patch -p1 -i "$PORT_SRC/update-failed-stays-listed.patch"

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D BUILD_SystemdSysupdateBackend=ON \
	-D BUILD_FwupdBackend=ON \
	-D BUILD_FlatpakBackend=OFF \
	-D BUILD_SnapBackend=OFF \
	-D BUILD_PackageKitBackend=OFF \
	-D BUILD_RpmOstreeBackend=OFF \
	-D BUILD_HoloBackend=OFF \
	-D BUILD_DummyBackend=OFF \
	-D BUILD_AlpineApkBackend=OFF \
	-D BUILD_AppStreamPreviewBackend=OFF \
	-D CMAKE_DISABLE_FIND_PACKAGE_KF6NewStuff=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_KF6UserFeedback=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_Qt6WebView=ON \
	-D WITH_KCM=ON \
	-D WITH_NOTIFIER=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
cd libdiscover
find . \( -path ./backends -o -path ./tests \) -prune -o -name '*.h' -print | while read -r h; do
	install -Dm644 "$h" "$PKG/usr/include/discover/$h"
done
cd ..
install -Dm644 -t "$PKG/usr/include/discover" build/DiscoverConfig.h \
	build/libdiscover/discovercommon_export.h \
	build/libdiscover/notifiers/discovernotifiers_export.h

rm "$PKG/usr/share/applications/org.kde.discover.snap.desktop" \
	"$PKG/usr/share/applications/org.kde.discover.appstreampreview.desktop"

test "$(ls "$PKG/usr/lib/qt6/plugins/discover")" = "$(printf '%s\n' fwupd-backend.so systemdsysupdate-backend.so)"

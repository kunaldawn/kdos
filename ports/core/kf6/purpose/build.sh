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
	-D KF_SKIP_PO_PROCESSING=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_KAccounts6=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

for t in imgur youtube nextcloud reviewboard phabricator bluetooth; do
	[ -n "$(find "$PKG" -name "${t}plugin.so")" ] || {
		echo "purpose: no ${t}plugin.so to remove" >&2
		exit 1
	}
	find "$PKG" \( -name "${t}plugin.so" -o -name "${t}plugin_config.qml" \
		-o -name "${t}-purpose6.png" \) -delete
	find "$PKG" -depth -type d -path "*/org/kde/purpose/$t" -exec rm -rf {} +
done

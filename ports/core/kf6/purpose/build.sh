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

# KF_SKIP_PO_PROCESSING leaves every translation out: bundled data is English
# only, and without it each language's catalogue is compiled and installed.
# KAccounts has no port; the YouTube and Nextcloud targets are then built
# without account support.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D BUILD_QCH=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_KAccounts6=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# The share targets that upload to an online service (imgur, YouTube, Nextcloud,
# Review Board, Phabricator) are built whatever the options and are removed,
# with their settings pages, QML modules and icons; so is Bluetooth, which runs
# Plasma's bluedevil-sendfile. A removed target is absent from every share menu.
# A target whose plugin is not found fails the build: a renamed plugin would
# otherwise ship its upload target silently.
for t in imgur youtube nextcloud reviewboard phabricator bluetooth; do
	[ -n "$(find "$PKG" -name "${t}plugin.so")" ] || {
		echo "purpose: no ${t}plugin.so to remove" >&2
		exit 1
	}
	find "$PKG" \( -name "${t}plugin.so" -o -name "${t}plugin_config.qml" \
		-o -name "${t}-purpose6.png" \) -delete
	find "$PKG" -depth -type d -path "*/org/kde/purpose/$t" -exec rm -rf {} +
done

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


# KF_SKIP_PO_PROCESSING leaves the interface catalogues out: bundled data is
# English only. kdoctools_install() builds every translated handbook and
# manual page too, and those are removed after the install. KWrite is not
# built: it is Kate's editor in a smaller window, and its entry would be a
# second row claiming text/plain. The Compiler Explorer plugin only posts the
# buffer to godbolt.org, so it is left out; every other plugin works on local
# files. KUserFeedback has no port, and without it the telemetry page and its
# code are compiled out. The SQL plugin needs Qt Keychain and the GPG plugin
# gpgmepp; each is skipped silently when its library is absent, so both are
# required here and a missing one stops the configure.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D USE_DBUS=ON \
	-D BUILD_kwrite=OFF \
	-D BUILD_compiler-explorer=OFF \
	-D CMAKE_DISABLE_FIND_PACKAGE_KF6UserFeedback=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Keychain=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Gpgmepp=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

if [ -d "$PKG/usr/share/doc/HTML" ]; then
	find "$PKG/usr/share/doc/HTML" -mindepth 1 -maxdepth 1 ! -name en -exec rm -rf {} +
fi
if [ -d "$PKG/usr/share/man" ]; then
	find "$PKG/usr/share/man" -mindepth 1 -maxdepth 1 ! -name 'man*' -exec rm -rf {} +
fi

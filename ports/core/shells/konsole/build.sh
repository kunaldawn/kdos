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
# manual page too, and those are removed after the install; the handbook is
# required, so a missing KDocTools stops the configure. The entry's Icon= is
# utilities-terminal, which the icon atlas does not carry; INSTALL_ICONS puts
# Konsole's own PNGs of that name in hicolor, where the panel's fallback finds
# them, and without it the Start menu row has no picture. libssh is REQUIRED by WITH_LIBSSH and only reads ~/.ssh/config for
# the SSH manager. Kapsule is a container front end with no port.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D USE_DBUS=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON \
	-D WITH_X11=ON \
	-D WITH_LIBSSH=ON \
	-D WITH_KAPSULE=OFF \
	-D ENABLE_PLUGIN_SSHMANAGER=ON \
	-D ENABLE_PLUGIN_QUICKCOMMANDS=ON \
	-D INSTALL_ICONS=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

if [ -d "$PKG/usr/share/doc/HTML" ]; then
	find "$PKG/usr/share/doc/HTML" -mindepth 1 -maxdepth 1 ! -name en -exec rm -rf {} +
fi
if [ -d "$PKG/usr/share/man" ]; then
	find "$PKG/usr/share/man" -mindepth 1 -maxdepth 1 ! -name 'man*' -exec rm -rf {} +
fi

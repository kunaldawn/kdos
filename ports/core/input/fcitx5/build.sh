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
cp "$PORT_SRC/$_endict" src/modules/spell/

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_INSTALL_SYSCONFDIR=/etc \
	-D CMAKE_BUILD_TYPE=Release \
	-D ENABLE_X11=Off \
	-D ENABLE_WAYLAND=On \
	-D ENABLE_DBUS=On \
	-D ENABLE_KEYBOARD=On \
	-D ENABLE_EMOJI=On \
	-D ENABLE_LIBUUID=On \
	-D ENABLE_SERVER=On \
	-D USE_SYSTEM_YOGA=Off \
	-D ENABLE_ENCHANT=On \
	-D ENABLE_DOC=Off \
	-D ENABLE_TEST=Off \
	-D ENABLE_TESTING_ADDONS=On \
	-D ENABLE_XDGAUTOSTART=Off \
	-D BUILD_SPELL_DICT=On \
	-D USE_SYSTEMD=Off \
	-D EVENT_LOOP_BACKEND=libuv \
	-D USE_SYSTEM_PLASMA_WAYLAND_PROTOCOLS=On \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

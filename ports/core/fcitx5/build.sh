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

# kpkg copies a .tar.zst rather than unpacking it, the same as linux-firmware.
tar -xf "$PORT_SRC/$name-$version.tar.zst" --strip-components=1

# ENABLE_X11=Off is the hard rule, not a size choice: X11 support here would
# pull xcb-imdkit, cairo-xcb, xkbfile and seven xcb components onto the host for
# an XIM frontend nothing on KDOS can use. X11 applications reach fcitx5 through
# Xwayland and text-input-v3 like every other client.
#
# EVENT_LOOP_BACKEND=libuv, because the alternative is systemd. USE_SYSTEMD=Off
# alone would still let an `auto` search find one if it ever appeared.
#
# BUILD_SPELL_DICT downloads en_dict.tar.gz at build time — the one thing in
# this tree that would reach the network during `make build --network none`.
#
# ENABLE_XDGAUTOSTART installs a .desktop into /etc/xdg/autostart, which nothing
# on KDOS reads; kdos-desktop-start launches fcitx5 by name.
#
# ENABLE_TESTING_ADDONS stays ON even though ENABLE_TEST is off, and that is not
# an oversight: every engine port -- fcitx5-hangul, -anthy, -chinese-addons --
# does an unconditional find_package(Fcitx5Module REQUIRED COMPONENTS
# TestFrontend), so turning the testing addons off makes all of them fail to
# configure. Three small addons.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_INSTALL_SYSCONFDIR=/etc \
	-D CMAKE_BUILD_TYPE=Release \
	-D ENABLE_X11=Off \
	-D ENABLE_WAYLAND=On \
	-D ENABLE_DBUS=On \
	-D ENABLE_KEYBOARD=On \
	-D ENABLE_ENCHANT=Off \
	-D ENABLE_DOC=Off \
	-D ENABLE_TEST=Off \
	-D ENABLE_TESTING_ADDONS=On \
	-D ENABLE_XDGAUTOSTART=Off \
	-D BUILD_SPELL_DICT=Off \
	-D USE_SYSTEMD=Off \
	-D EVENT_LOOP_BACKEND=libuv \
	-D USE_SYSTEM_PLASMA_WAYLAND_PROTOCOLS=On \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

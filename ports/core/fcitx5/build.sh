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
cp "$PORT_SRC/$_endict" src/modules/spell/

# ENABLE_X11=Off is the hard rule, not a size choice: X11 support here would
# pull xcb-imdkit, cairo-xcb, xkbfile and seven xcb components onto the host for
# an XIM frontend. The cost is that an X11 application under Xwayland, host or
# boxed, has no input method: Xwayland carries no text-input for its X
# clients, and XIM is the only route an X client has to an engine.
#
# EVENT_LOOP_BACKEND=libuv, because the alternative is systemd. USE_SYSTEMD=Off
# alone would still let an `auto` search find one if it ever appeared.
#
# BUILD_SPELL_DICT compiles the English word list for keyboard hints, and the
# spell module `file(DOWNLOAD)`s that list at BUILD time, which under
# `--network none` is a dead build. The port carries it, and Fcitx5Download's
# cmake skips the fetch when the file already sits where it would have put it
# with the expected hash — the same fix as fcitx5-chinese-addons.
#
# ENABLE_ENCHANT gives the spell module enchant as a second provider beside
# en_dict, fcitx's own built-in English word list. Enchant's Aspell provider
# and the aspell-en dictionary are the ones weechat, profanity, mc and recoll
# use. Upstream's `pkg_check_modules(... REQUIRED)` makes enchant a
# hard configure dependency once the option is on.
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

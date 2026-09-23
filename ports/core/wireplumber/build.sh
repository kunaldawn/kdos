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


# SYSTEM LUA RATHER THAN THE BUNDLED COPY. wireplumber statically links a
# vendored Lua when it cannot find one, which would be another Lua on an
# image that already carries one — another CVE surface, and a subproject
# fetch this build must never make.
#
# The version is named, not `auto`: auto walks lua5.5, lua5.4, … and settles
# for whatever it reaches, so with lua54 installed a missing lua5.5.pc builds
# against 5.4 and says nothing. Named, it is an error. 5.5 is ports/core/lua,
# found through the lua5.5.pc that port ships.
#
# `elogind` IS OFF: seat and session tracking here is seatd's, and the option
# left to `auto` finds nothing and disables itself silently, which is the
# shape of a dependency that must be explicit.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dsystem-lua=true \
	-Dsystem-lua-version=5.5 \
	-Delogind=disabled \
	-Dsystemd=disabled \
	-Ddoc=disabled \
	-Dtests=false \
	-Dintrospection=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

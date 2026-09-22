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
# vendored Lua when it cannot find one, which would be a second Lua on an
# image that already carries 5.4 — two interpreters, two CVE surfaces, and a
# subproject fetch this build must never make.
#
# `elogind` IS OFF: seat and session tracking here is seatd's, and the option
# left to `auto` finds nothing and disables itself silently, which is the
# shape of a dependency that must be explicit.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dsystem-lua=true \
	-Dsystem-lua-version=5.4 \
	-Delogind=disabled \
	-Dsystemd=disabled \
	-Ddoc=disabled \
	-Dtests=false \
	-Dintrospection=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

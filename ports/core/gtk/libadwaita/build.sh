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

# ministream, the metainfo reader behind the about dialog, is a subproject the
# release carries and links statically; nodownload keeps meson on that copy.
# The stylesheet ships compiled in the release, so sassc is never run.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Db_ndebug=if-release \
	--wrap-mode=nodownload \
	-Dintrospection=enabled \
	-Dvapi=true \
	-Ddocumentation=false \
	-Dtests=false \
	-Dexamples=false \
	-Dprofiling=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

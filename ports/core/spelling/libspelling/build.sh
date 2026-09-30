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

# Dictionaries come from enchant's providers, as for gspell.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release -Db_ndebug=if-release \
	--wrap-mode=nodownload \
	-Denchant=enabled \
	-Dintrospection=enabled \
	-Dvapi=true \
	-Ddocs=false \
	-Dsysprof=false \
	-Dinstall-static=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

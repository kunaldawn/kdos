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

# with_server stays off: the server half is under a proprietary licence.
meson setup build --prefix=/usr --libdir=lib \
	--buildtype=release \
	-Db_ndebug=if-release \
	--wrap-mode=nodownload \
	-Dwith_server=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

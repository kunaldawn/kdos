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

# -Dbashcompdir names the directory outright: left empty, meson asks the
# bash-completion package for it, and the completion file for secret-tool is
# then installed or silently dropped depending on build order.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dintrospection=false \
	-Dgtk_doc=false \
	-Dmanpage=true \
	-Dvapi=false \
	-Dbashcompdir=/usr/share/bash-completion/completions \
	-Dtest_setup=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

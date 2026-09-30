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

# Dictionaries are enchant's business: gspell offers whichever languages the
# enchant providers find, so an empty menu means enchant has no dictionary.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	--wrap-mode=nodownload \
	-Dgspell_app=false \
	-Dgobject_introspection=true \
	-Dvapi=true \
	-Dgtk_doc=false \
	-Dtests=false \
	-Dinstall_tests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

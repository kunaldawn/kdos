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

# The GTK 3 series. It installs under the gtksourceview-4 API name and data
# directory, so it sits beside gtksourceview5 without a shared file.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Db_ndebug=if-release \
	--wrap-mode=nodownload \
	-Dgir=true \
	-Dvapi=true \
	-Dgtk_doc=false \
	-Dglade_catalog=false \
	-Dinstall_tests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

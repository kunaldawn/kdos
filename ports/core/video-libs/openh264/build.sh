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

export CFLAGS="${CFLAGS/-O2/-O3}" CXXFLAGS="${CXXFLAGS/-O2/-O3}"

# Compiled here, so this is not the binary Cisco distributes and pays the
# H.264 patent licence for; the library is BSD-2-Clause either way.
#
# The unit tests pull GoogleTest through a wrap; with them disabled and
# downloads refused, the offline build cannot reach for it.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Db_ndebug=if-release \
	--wrap-mode=nodownload \
	-Dtests=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

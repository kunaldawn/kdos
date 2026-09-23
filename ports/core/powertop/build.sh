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

# meson rather than autotools: the GitHub archive ships no `configure`, so the
# autotools path would need autoreconf, and upstream's meson.build asks for
# exactly the same libraries.
#
# 2.15 made libtracefs a hard requirement — there is no flag to turn it off,
# which is why libtraceevent and libtracefs are ports now. 2.14 carried its own
# copy of traceevent in-tree; pinning that instead would have dodged two ports
# and frozen powertop at 2022.
#
# -Dnls=false: the translation catalogues are compiled by gettext's msgfmt,
# which is not a dependency here, and the image is English-only. pciutils and libnl are `required: false` upstream
# with no option, so the `depends` line is what keeps device names and the
# wireless tunables in.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dnls=false \
	-Dtest-framework=false \
	-Denable-tests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

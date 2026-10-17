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

meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Ddefault_library=shared \
	-Dbdj_jar=disabled \
	-Dfreetype=enabled \
	-Dfontconfig=enabled \
	-Dlibxml2=enabled \
	-Denable_tools=true \
	-Denable_devtools=false \
	-Denable_examples=false \
	-Denable_docs=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

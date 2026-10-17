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
	-Dtools=enabled -Dgpioset-interactive=enabled \
	-Dtests=disabled -Dexamples=disabled \
	-Dbindings-cxx=disabled -Dbindings-python=disabled \
	-Dbindings-rust=disabled -Dbindings-glib=disabled \
	-Ddbus=disabled -Dintrospection=disabled -Dsystemd=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

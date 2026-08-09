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

meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	-Ddocumentation=disabled \
	-Dman-pages=disabled \
	-Dgeoclue=disabled \
	-Dgudev=disabled \
	-Dsystemd=disabled \
	-Dflatpak-interfaces=disabled \
	-Dtests=disabled \
	-Dinstalled-tests=false \
	-Dsandboxed-image-validation=disabled \
	-Dsandboxed-sound-validation=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

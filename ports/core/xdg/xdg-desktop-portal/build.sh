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

patch -p1 --no-backup-if-mismatch -i "$PORT_SRC/camera-leave-pipewire-loop.patch"

meson setup build --buildtype=release \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	-Ddocumentation=disabled \
	-Dman-pages=enabled \
	-Dgeoclue=enabled \
	-Dgudev=enabled \
	-Dsystemd=disabled \
	-Dflatpak-interfaces=disabled \
	-Dtests=disabled \
	-Dinstalled-tests=false \
	-Dsandboxed-image-validation=enabled \
	-Dsandboxed-sound-validation=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

rm -rf "$PKG/usr/lib/systemd"

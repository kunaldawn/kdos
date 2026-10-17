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

patch -p1 -i "$PORT_SRC/fix-W_EXITCODE.patch"
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	--wrap-mode=nodownload \
	-Dgtk3=true \
	-Dgtk4=true \
	-Dgir=true \
	-Dvapi=true \
	-Da11y=true \
	-Dfribidi=true \
	-Dgnutls=true \
	-Dicu=true \
	-D_systemd=false \
	-Dterminfo=false \
	-Dapp=false \
	-Dglade=false \
	-Ddocs=false \
	-Ddbg=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

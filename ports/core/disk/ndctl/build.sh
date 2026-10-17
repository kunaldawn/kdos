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

patch -p1 -i "$PORT_SRC/strerror.patch"

meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dversion-tag=$version \
	-Dsystemd=disabled \
	-Ddocs=enabled \
	-Dasciidoctor=enabled \
	-Dlibtracefs=enabled \
	-Dkeyutils=enabled \
	-Dtest=disabled \
	-Dbashcompletiondir=/usr/share/bash-completion/completions
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

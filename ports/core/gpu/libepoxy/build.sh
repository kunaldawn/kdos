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

# glx=yes resolves glX* through libglvnd's libGL.so.1 at run time, which
# dispatches to mesa's libGLX_mesa; EGL goes through libglvnd's libEGL.
meson setup build --buildtype=release --prefix=/usr --sysconfdir=/etc --libdir=lib \
	-Degl=yes \
	-Dglx=yes \
	-Dx11=true \
	-Ddocs=false \
	-Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

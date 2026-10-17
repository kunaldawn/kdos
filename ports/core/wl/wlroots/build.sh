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

patch -p1 -i "$PORT_SRC/idle-capture-frame.patch"

meson setup build \
	--prefix=/usr \
	--libdir=lib \
	--buildtype=release \
	-Dexamples=false \
	-Dbackends=drm,libinput \
	-Drenderers=gles2,vulkan \
	-Dallocators=gbm,udmabuf \
	-Dsession=enabled \
	-Dxwayland=enabled \
	-Dcolor-management=enabled \
	-Dlibliftoff=disabled \
	-Dxcb-errors=disabled

meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

install -d "$PKG/usr/share/wlroots/protocols"
install -m644 protocol/*.xml "$PKG/usr/share/wlroots/protocols/"

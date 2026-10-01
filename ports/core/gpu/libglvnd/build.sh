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

# x11 and glx are on so libGL.so and libGLX.so exist, with gl.pc and glx.pc:
# an X11 client under Xwayland, and every build that asks for OpenGL::GL or
# -lGL, needs them. libGLX dispatches to mesa's libGLX_mesa. A Wayland client
# reaches desktop GL through libOpenGL and libEGL and never loads libGLX.
meson setup build --buildtype=release \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	-D x11=enabled \
	-D glx=enabled \
	-D gles1=false \
	-D egl=true \
	-D tls=false \
	-D hgl=false \
	-D asm=enabled \
	-D entrypoint-patching=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

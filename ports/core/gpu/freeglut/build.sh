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

# THE WAYLAND BACKEND IS OFF BECAUSE IT CANNOT OPEN A WINDOW HERE. freeglut's
# Wayland code binds wl_shell and nothing else, and glutInit fails when the
# compositor does not offer it; wlroots, and so kdos-comp, offers xdg-shell
# only. The backend is chosen at compile time, one per library, so the X11
# backend is built and GLUT programs run under Xwayland through GLX.
#
# XRandR, XF86VidMode and XInput2 have no switch of their own: each is used
# when its headers are found, which the depends line guarantees. Without
# XInput2 there is no multi-touch, and without the other two game mode cannot
# change the video mode.
#
# The library is installed as libglut with GL/glut.h, the name every GLUT
# program links against.
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DFREEGLUT_BUILD_SHARED_LIBS=ON \
	-DFREEGLUT_BUILD_STATIC_LIBS=OFF \
	-DFREEGLUT_BUILD_DEMOS=OFF \
	-DFREEGLUT_WAYLAND=OFF \
	-DFREEGLUT_GLES=OFF \
	-DFREEGLUT_REPLACE_GLUT=ON \
	-DFREEGLUT_INSTALL_MAN_PAGES=ON \
	-DOpenGL_GL_PREFERENCE=GLVND
ninja -C build
DESTDIR=$PKG ninja -C build install

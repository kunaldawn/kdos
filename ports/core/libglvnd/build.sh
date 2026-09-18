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
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	-D x11=disabled \
	-D glx=disabled \
	-D gles1=false \
	-D egl=true \
	-D tls=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# A FILENAME alias, and the spelling is the whole point. glx=disabled builds no
# libGL at all, and an EGL client asking for desktop GL opens the library by one
# of the two legacy names: glmark2's wayland-gl flavour tries libGL.so, then
# libGL.so.1, and reports "Error loading GL library" if neither answers.
# libOpenGL.so.0 serves it — the same dispatch table, every gl* entrypoint, no
# glX*.
#
# THE .1 SPELLING MUST NOT EXIST HERE. libepoxy takes a libGL.so.1 it can open
# as its GLX provider and then resolves glXGetCurrentContext from that handle
# with abort-on-missing; against a glvnd libOpenGL that is
# `glXGetCurrentContext() not found` and SIGABRT, on the second GL entrypoint
# the client bootstraps. Xwayland is what links libepoxy on this image. Only
# glmark2 and eglinfo name the unsuffixed spelling, and neither can abort on a
# missing glX*.
#
# The SONAME is untouched, so `-lGL` links against this symlink and records
# DT_NEEDED libOpenGL.so.0 — a program that runs, or a link error naming the
# glX* symbol it wanted.
ln -s libOpenGL.so.0 "$PKG/usr/lib/libGL.so"

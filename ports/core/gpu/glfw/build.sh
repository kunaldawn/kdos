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

# BOTH BACKENDS ARE BUILT AND WAYLAND IS TRIED FIRST. glfwInit follows
# XDG_SESSION_TYPE when it names a display that is set, and otherwise walks the
# compiled platforms in a fixed order, Wayland before X11, taking the first
# that connects. A program on kdos-comp is a Wayland client; one started with
# no WAYLAND_DISPLAY, or one that asks for GLFW_PLATFORM_X11 through
# glfwInitHint, runs under Xwayland.
#
# EVERY WINDOWING LIBRARY IS OPENED AT RUN TIME, not linked: libwayland-*,
# libxkbcommon, libdecor-0, libX11-xcb, libXi, libXrandr, libXcursor,
# libXinerama, libXxf86vm, libXrender, libXext, libEGL, libGL and libvulkan are
# each dlopened by soname. A missing one is not a link error but a platform
# that fails to initialise, which is why each is in depends although only the
# headers are read here. libdecor is what gives a window a frame on a
# compositor that refuses server-side decoration.
#
# The protocol XML is bundled under deps/wayland and generated here with
# wayland-scanner, so wayland-protocols is not read.
cmake -B build -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DGLFW_BUILD_WAYLAND=ON \
	-DGLFW_BUILD_X11=ON \
	-DGLFW_BUILD_EXAMPLES=OFF \
	-DGLFW_BUILD_TESTS=OFF \
	-DGLFW_BUILD_DOCS=OFF \
	-DGLFW_INSTALL=ON
ninja -C build
DESTDIR=$PKG ninja -C build install

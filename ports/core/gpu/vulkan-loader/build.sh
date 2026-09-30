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

# Every platform an ICD advertises must be compiled in here too: the loader
# drops VK_KHR_xcb_surface and VK_KHR_xlib_surface from the instance extension
# list when it was built without them, and an X11 client under Xwayland
# (winex11, SDL and GLFW on their X11 path) then cannot create a surface.
cmake -GNinja -B build \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_INSTALL_SYSCONFDIR=/etc \
	-DCMAKE_INSTALL_DATADIR=/usr/share \
	-DCMAKE_PREFIX_PATH=/usr \
	-DVULKAN_HEADERS_INSTALL_DIR=/usr \
	-DBUILD_WSI_WAYLAND_SUPPORT=ON \
	-DBUILD_WSI_XCB_SUPPORT=ON \
	-DBUILD_WSI_XLIB_SUPPORT=ON \
	-DBUILD_WSI_XLIB_XRANDR_SUPPORT=ON \
	-DBUILD_TESTS=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

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

# The Wayland client and its xdg-shell and bradient decoration plugins are in
# qt6-qtbase. This is the rest: the QtWaylandCompositor library and its QML
# module, and the adwaita decoration a Qt program draws for itself when the
# compositor offers no server-side decorations, which needs Qt D-Bus and Qt SVG.
#
# TEST_dmabuf_client_buffer=ON: the configure runs that test before it has
# looked EGL up, so it reports EGL missing and the test fails however the
# headers stand. What the test checks, EGL_LINUX_DMA_BUF_EXT and
# EGL_EXT_image_dma_buf_import_modifiers, is in Mesa's eglext.h.
mkdir -p build && cd build
cmake .. -G Ninja -Wno-dev \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DQT_BUILD_EXAMPLES=OFF \
	-DQT_BUILD_TESTS=OFF \
	-DFEATURE_wayland_server=ON \
	-DFEATURE_wayland_compositor_quick=ON \
	-DTEST_dmabuf_client_buffer=ON \
	-DFEATURE_wayland_dmabuf_client_buffer=ON \
	-DFEATURE_wayland_decoration_adwaita=ON \
	-DFEATURE_wayland_client_qt_shell=ON \
	-DFEATURE_wayland_client_ivi_shell=ON
ninja
DESTDIR=$PKG ninja install

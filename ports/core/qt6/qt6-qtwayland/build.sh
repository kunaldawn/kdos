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

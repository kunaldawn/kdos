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

patch -p1 -i "$PORT_SRC/select.patch"

mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DQT_BUILD_TESTS=OFF \
	-DQT_BUILD_EXAMPLES=OFF \
	-DFEATURE_ffmpeg=ON \
	-DFEATURE_gstreamer=OFF \
	-DFEATURE_pipewire=ON \
	-DFEATURE_pipewire_screencapture=ON \
	-DFEATURE_pulseaudio=ON \
	-DFEATURE_alsa=OFF \
	-DFEATURE_vaapi=ON \
	-DFEATURE_linux_v4l=ON \
	-DFEATURE_spatialaudio=ON \
	-DFEATURE_spatialaudio_quick3d=ON
ninja
DESTDIR=$PKG ninja install

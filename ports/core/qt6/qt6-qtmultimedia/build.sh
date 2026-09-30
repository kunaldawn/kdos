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

# The install layout (bin, lib/qt6, include/qt6, mkspecs) is inherited from
# qt6-qtbase through Qt6BuildInternals, so no INSTALL_* path is passed here: a
# path set only in this module would scatter it away from every other one.
#
# FFMPEG IS THE MEDIA BACKEND AND GSTREAMER IS OFF. Qt picks FFmpeg by default,
# so a GStreamer plugin would only be a second backend nobody selects.
#
# AUDIO GOES TO PIPEWIRE, WITH PULSE BEHIND IT. ALSA is refused: upstream makes
# it mutually exclusive with PulseAudio.
#
# X11 screen and window capture is built because qt6-qtbase has xlib; it links
# libX11, libXrandr and libXext. Wayland capture goes through the PipeWire
# screencast portal.
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

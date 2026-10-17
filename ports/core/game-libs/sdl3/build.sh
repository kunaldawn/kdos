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
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DSDL_SHARED=ON \
	-DSDL_STATIC=OFF \
	-DSDL_TEST_LIBRARY=OFF \
	-DSDL_TESTS=OFF \
	-DSDL_EXAMPLES=OFF \
	-DSDL_RPATH=OFF \
	-DSDL_X11=OFF \
	-DSDL_KMSDRM=OFF \
	-DSDL_WAYLAND=ON \
	-DSDL_WAYLAND_LIBDECOR=ON \
	-DSDL_ALSA=ON \
	-DSDL_PIPEWIRE=ON \
	-DSDL_PULSEAUDIO=ON \
	-DSDL_JACK=ON \
	-DSDL_SNDIO=OFF \
	-DSDL_OSS=OFF \
	-DSDL_DBUS=ON \
	-DSDL_IBUS=OFF \
	-DSDL_LIBUDEV=ON \
	-DSDL_HIDAPI_LIBUSB=ON \
	-DSDL_LIBURING=ON \
	-DSDL_OPENGL=ON \
	-DSDL_OPENGLES=ON \
	-DSDL_VULKAN=ON \
	-DSDL_INSTALL_DOCS=ON
ninja
DESTDIR=$PKG ninja install

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
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DALSOFT_UPDATE_BUILD_VERSION=OFF \
	-DALSOFT_ENABLE_MODULES=OFF \
	-DALSOFT_UTILS=ON \
	-DALSOFT_NO_CONFIG_UTIL=ON \
	-DALSOFT_EXAMPLES=OFF \
	-DALSOFT_TESTS=OFF \
	-DALSOFT_INSTALL_EXAMPLES=OFF \
	-DALSOFT_EMBED_HRTF_DATA=ON \
	-DALSOFT_RTKIT=ON \
	-DALSOFT_REQUIRE_RTKIT=ON \
	-DALSOFT_BACKEND_PIPEWIRE=ON \
	-DALSOFT_REQUIRE_PIPEWIRE=ON \
	-DALSOFT_BACKEND_PULSEAUDIO=ON \
	-DALSOFT_REQUIRE_PULSEAUDIO=ON \
	-DALSOFT_BACKEND_ALSA=ON \
	-DALSOFT_REQUIRE_ALSA=ON \
	-DALSOFT_BACKEND_JACK=ON \
	-DALSOFT_REQUIRE_JACK=ON \
	-DALSOFT_BACKEND_OSS=OFF \
	-DALSOFT_BACKEND_SNDIO=OFF \
	-DALSOFT_BACKEND_PORTAUDIO=OFF \
	-DALSOFT_BACKEND_SDL2=OFF \
	-DALSOFT_BACKEND_SDL3=OFF \
	-DALSOFT_BACKEND_WAVE=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_MySOFA=ON
ninja
DESTDIR=$PKG ninja install
install -m755 uhjdecoder uhjencoder sofa-info "$PKG/usr/bin"

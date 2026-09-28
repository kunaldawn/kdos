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

# PIPEWIRE FIRST, PULSE AND ALSA BEHIND IT, and each one required: a backend
# whose library is missing would otherwise configure away and leave a program
# that opens no device. JACK, OSS, sndio and PortAudio are off; there is no
# JACK on this image and the rest reach the same card by a longer road.
#
# RTKit is on for a realtime mixer thread over D-Bus, which is how pipewire's
# own threads get theirs. ALSOFT_ENABLE_MODULES is off, so the library is
# compiled from its headers rather than as C++20 modules.
#
# ALSOFT_NO_CONFIG_UTIL: alsoft-config is a Qt program, and its settings are
# plain text in ~/.alsoftrc. openal-info, the UHJ tools and makemhr are
# built; makemhr reads SOFA HRTF sets through libmysofa, whose probe is
# required so the tool cannot configure away. The example players are not
# built. The HRTF data is embedded in the library.
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
	-DALSOFT_BACKEND_JACK=OFF \
	-DALSOFT_BACKEND_OSS=OFF \
	-DALSOFT_BACKEND_SNDIO=OFF \
	-DALSOFT_BACKEND_PORTAUDIO=OFF \
	-DALSOFT_BACKEND_SDL2=OFF \
	-DALSOFT_BACKEND_SDL3=OFF \
	-DALSOFT_BACKEND_WAVE=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_MySOFA=ON
ninja
DESTDIR=$PKG ninja install

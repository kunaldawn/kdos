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

mkdir -p gcem
tar xf "$PORT_SRC/gcem-$_gcem.tar.gz" --strip-components=1 -C gcem

mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-Denable-alsa=ON \
	-Denable-pipewire=ON \
	-Denable-pulseaudio=OFF \
	-Denable-jack=ON \
	-Denable-sdl3=OFF \
	-Denable-oss=OFF \
	-Denable-readline=ON \
	-Denable-libsndfile=ON \
	-Denable-dbus=ON \
	-Denable-openmp=ON \
	-Denable-signalsmith=OFF \
	-Denable-systemd=OFF \
	-Denable-ladspa=ON \
	-Denable-midishare=OFF
grep -q '^#define JACK_SUPPORT 1' config.h
grep -q '^#define LADSPA 1' config.h
ninja
DESTDIR=$PKG ninja install

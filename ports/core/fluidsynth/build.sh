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

# A MIDI FILE IS A SCORE, NOT A RECORDING, and nothing else on this machine can
# perform one. It is a few kilobytes of "which note, how hard, when" — which is
# why a decades-old archive of them is tiny and why it is worthless without a
# synthesiser. fluidsynth is that, and it needs a SoundFont: none is shipped
# (they are tens to hundreds of megabytes and picking one is picking a sound),
# and `fluidsynth -a alsa file.sf2 tune.mid` is the whole interface.
#
# -Denable-pipewire=ON as well as ALSA, because the desktop session runs
# pipewire while tty1 does not — the same split kdos-bb's audio notes describe.
#
# Every other probe is pinned: an `enable-*` switch upstream only means "if it
# is available", so each is named ON with its port declared or OFF. dbus is
# what reaches rtkit for a realtime audio thread; OpenMP is gcc's libgomp and
# parallelises SoundFont loading and voice mixing. The signalsmith reverb and
# limiter are a git submodule the release archive leaves empty, and systemd,
# LADSPA and MidiShare have no port.
#
# GCEM IS A SUBMODULE FLUIDSYNTH DOWNLOADS AT CONFIGURE TIME, and this build
# has no network. cmake_admin/FindGCEM.cmake looks for gcem/include/gcem.hpp in
# the source root before reaching for github, so the pinned revision is carried
# as a second `source =` and put where the probe already looks. Header-only, so
# nothing is compiled or installed from it.
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
	-Denable-jack=OFF \
	-Denable-sdl3=OFF \
	-Denable-oss=OFF \
	-Denable-readline=ON \
	-Denable-libsndfile=ON \
	-Denable-dbus=ON \
	-Denable-openmp=ON \
	-Denable-signalsmith=OFF \
	-Denable-systemd=OFF \
	-Denable-ladspa=OFF \
	-Denable-midishare=OFF
ninja
DESTDIR=$PKG ninja install

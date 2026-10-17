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
	-DBUILD_SHARED_LIBS=ON \
	-DSDL2MIXER_VENDORED=OFF \
	-DSDL2MIXER_DEPS_SHARED=OFF \
	-DSDL2MIXER_SAMPLES=OFF \
	-DSDL2MIXER_CMD=OFF \
	-DSDL2MIXER_FLAC=ON \
	-DSDL2MIXER_FLAC_LIBFLAC=ON \
	-DSDL2MIXER_FLAC_DRFLAC=OFF \
	-DSDL2MIXER_GME=ON \
	-DSDL2MIXER_MOD=ON \
	-DSDL2MIXER_MOD_XMP=ON \
	-DSDL2MIXER_MOD_XMP_LITE=OFF \
	-DSDL2MIXER_MOD_MODPLUG=OFF \
	-DSDL2MIXER_MP3=ON \
	-DSDL2MIXER_MP3_MPG123=ON \
	-DSDL2MIXER_MP3_MINIMP3=OFF \
	-DSDL2MIXER_MIDI=ON \
	-DSDL2MIXER_MIDI_FLUIDSYNTH=ON \
	-DSDL2MIXER_MIDI_TIMIDITY=ON \
	-DSDL2MIXER_OPUS=ON \
	-DSDL2MIXER_VORBIS=VORBISFILE \
	-DSDL2MIXER_WAVE=ON \
	-DSDL2MIXER_WAVPACK=ON
ninja
DESTDIR=$PKG ninja install

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

# EVERY CODEC IS A LINKED SYSTEM LIBRARY, not a bundled single-header decoder
# (minimp3, drflac, stb_vorbis) and not a dlopen: DEPS_SHARED off puts each
# library in the ELF, where a missing one is a load error rather than a format
# that quietly stops playing. Each switch is named because upstream's default
# picks the bundled decoder.
#
# MOD music goes through libxmp; modplug stays off, since both claim the same
# formats and the first one SDL_mixer finds wins. MIDI is FluidSynth, plus the
# built-in Timidity player that needs no library. There is no JACK anywhere.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
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

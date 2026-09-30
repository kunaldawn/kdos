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
# (dr_mp3, drflac, stb_vorbis) and not a dlopen: DEPS_SHARED off puts each
# library in the ELF, where a missing one is a load error rather than a format
# that quietly stops playing. STRICT makes a codec whose library is absent fail
# the configure instead of dropping the format. There is no JACK anywhere.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DSDLMIXER_VENDORED=OFF \
	-DSDLMIXER_STRICT=ON \
	-DSDLMIXER_DEPS_SHARED=OFF \
	-DSDLMIXER_TESTS=OFF \
	-DSDLMIXER_EXAMPLES=OFF \
	-DSDLMIXER_INSTALL_MAN=ON \
	-DSDLMIXER_FLAC=ON \
	-DSDLMIXER_FLAC_LIBFLAC=ON \
	-DSDLMIXER_FLAC_DRFLAC=OFF \
	-DSDLMIXER_GME=ON \
	-DSDLMIXER_MOD=ON \
	-DSDLMIXER_MOD_XMP=ON \
	-DSDLMIXER_MOD_XMP_LITE=OFF \
	-DSDLMIXER_MP3=ON \
	-DSDLMIXER_MP3_MPG123=ON \
	-DSDLMIXER_MP3_DRMP3=OFF \
	-DSDLMIXER_MIDI=ON \
	-DSDLMIXER_MIDI_FLUIDSYNTH=ON \
	-DSDLMIXER_MIDI_TIMIDITY=ON \
	-DSDLMIXER_OPUS=ON \
	-DSDLMIXER_VORBIS_VORBISFILE=ON \
	-DSDLMIXER_VORBIS_STB=OFF \
	-DSDLMIXER_VORBIS_TREMOR=OFF \
	-DSDLMIXER_WAVPACK=ON
ninja
DESTDIR=$PKG ninja install

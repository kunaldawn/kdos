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

# FFT_LIB=fftw3 names the transform: left unset, the library links FFmpeg's
# avtx whenever FFmpeg is found and fftw otherwise. BUILD_TOOLS gives fpcalc,
# the fingerprinter MusicBrainz Picard runs, and is what needs FFmpeg to
# decode; AUDIO_PROCESSOR_LIB=swresample makes a missing libswresample a
# configure error rather than an fpcalc that converts nothing. BUILD_TESTS
# defaults on and wants GoogleTest.
cmake -S . -B build -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DFFT_LIB=fftw3 \
	-DBUILD_TOOLS=ON \
	-DAUDIO_PROCESSOR_LIB=swresample \
	-DBUILD_TESTS=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

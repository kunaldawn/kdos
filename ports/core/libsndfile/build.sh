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

# ENABLE_EXTERNAL_LIBS is what makes this library read more than WAV.
#
# With it OFF, libsndfile compiles and links and handles uncompressed PCM only
# — FLAC, Ogg, Vorbis and Opus are all compiled out, whether or not their
# libraries are installed. Every program on the host that opens a sound file
# goes through here, so this flag decides what formats the whole system can
# read. ENABLE_MPEG adds MP3 in both directions, and only when BOTH lame and
# mpg123 are found: either one missing turns MP3 off entirely.
#
# cmake disables a codec it cannot find without failing, so each
# CMAKE_REQUIRE_FIND_PACKAGE_* switch below makes a missing codec library a
# configure error instead of a library silently narrower than this recipe
# claims. ALSA is required the same way: it is what sndfile-play plays through,
# and without it the program is built against OSS's /dev/dsp instead, which
# bypasses the sound server and exists only while snd-pcm-oss is loaded.

mkdir -p build
cd build
cmake .. \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_PROGRAMS=ON \
	-DBUILD_EXAMPLES=OFF \
	-DBUILD_TESTING=OFF \
	-DENABLE_EXTERNAL_LIBS=ON \
	-DENABLE_MPEG=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_Ogg=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_Vorbis=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_FLAC=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_Opus=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_mp3lame=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_mpg123=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_ALSA=ON
make
make DESTDIR=$PKG install

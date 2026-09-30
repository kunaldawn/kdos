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

# WITH_OPENMP is off for the oversubscription reason OpenBLAS's recipe states:
# every consumer (ffmpeg, mpd) already runs its own threads around the
# resampler. PFFFT is the bundled DFT; WITH_AVFFT would make this library link
# the libavcodec that links it. WITH_LSR_BINDINGS adds libsoxr-lsr, whose
# header and pkg-config name do not collide with libsamplerate's.
cmake -B build -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_TESTS=OFF \
	-DBUILD_EXAMPLES=OFF \
	-DWITH_OPENMP=OFF \
	-DWITH_LSR_BINDINGS=ON \
	-DWITH_VR32=ON \
	-DWITH_CR32=ON \
	-DWITH_CR64=ON \
	-DWITH_CR32S=ON \
	-DWITH_CR64S=ON \
	-DWITH_PFFFT=ON \
	-DWITH_AVFFT=OFF \
	-DWITH_DEV_GPROF=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

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

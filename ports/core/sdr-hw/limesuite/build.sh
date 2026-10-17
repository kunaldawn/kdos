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

cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DLIB_SUFFIX= \
	-DLIME_SUITE_EXTVER=kdos \
	-DENABLE_SIMD_FLAGS=none \
	-DENABLE_LIBRARY=ON \
	-DENABLE_HEADERS=ON \
	-DENABLE_LIME_UTIL=ON \
	-DENABLE_QUICKTEST=ON \
	-DENABLE_SOAPY_LMS7=ON \
	-DENABLE_GUI=OFF \
	-DENABLE_DESKTOP=OFF \
	-DENABLE_OCTAVE=OFF \
	-DENABLE_MCU_TESTBENCH=OFF \
	-DENABLE_API_DOXYGEN=OFF \
	-DENABLE_NEW_GAIN_BEHAVIOUR=OFF
ninja -C build
DESTDIR=$PKG ninja -C build install

test -f "$PKG/usr/lib/libLimeSuite.so"
test -f "$PKG/usr/include/lime/LimeSuite.h"
test -x "$PKG/usr/bin/LimeUtil"
test -n "$(find "$PKG/usr/lib/SoapySDR" -name 'libLMS7Support.so' | head -1)"

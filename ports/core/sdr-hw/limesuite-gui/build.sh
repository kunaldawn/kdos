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
	-DENABLE_GUI=ON \
	-DENABLE_DESKTOP=OFF \
	-DENABLE_HEADERS=OFF \
	-DENABLE_LIME_UTIL=OFF \
	-DENABLE_QUICKTEST=OFF \
	-DENABLE_SOAPY_LMS7=OFF \
	-DENABLE_OCTAVE=OFF \
	-DENABLE_MCU_TESTBENCH=OFF \
	-DENABLE_API_DOXYGEN=OFF \
	-DENABLE_NEW_GAIN_BEHAVIOUR=OFF
ninja -C build LimeSuiteGUI

install -Dm755 build/bin/LimeSuiteGUI "$PKG/usr/bin/LimeSuiteGUI"
install -Dm644 Desktop/lime-suite.desktop "$PKG/usr/share/applications/lime-suite.desktop"
for s in 16 22 32 48 64 128; do
	install -Dm644 "Desktop/lime-suite-$s.png" "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/lime-suite.png"
done

test -z "$(find "$PKG" -name 'libLimeSuite.so*')"
readelf -d "$PKG/usr/bin/LimeSuiteGUI" | grep -q 'NEEDED.*\[libLimeSuite\.so\.23\.11-1\]'

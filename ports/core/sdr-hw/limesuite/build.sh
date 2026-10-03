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

# The 23.11.0 tag predates the C23 and missing-include fixes GCC 15 needs;
# the recipe follows upstream's master at that version. LIME_SUITE_EXTVER
# replaces the git hash the version string would otherwise ask git for.
#
# ENABLE_SIMD_FLAGS picks -march; its default is native, or SSE3 on x86
# installed to /usr. Either lifts the library past the plain x86-64 baseline
# every other port is built for, and the source carries no intrinsics that
# need it, so none leaves the phase's own flags in charge.
# LimeSuiteGUI needs wxWidgets and is off. Gateware and firmware images are
# not fetched at configure (DOWNLOAD_IMAGES unset); LimeUtil --update asks
# the network for them when it is run. Device access is granted by
# /etc/udev/rules.d/70-kdos-sdr.rules, so UDEV_RULES_PATH is left unset and
# upstream's rules are not installed. LIB_SUFFIX places LimeSuite's own
# library; the SoapySDR module's directory comes from SoapySDR's CMake package,
# which includes GNUInstallDirs, and that picks lib64 on a 64-bit system it
# does not recognise unless CMAKE_INSTALL_LIBDIR names lib.
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

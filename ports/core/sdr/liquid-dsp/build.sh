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

# Every SIMD kernel is compiled in, each under its own #pragma GCC target, and
# the one used is chosen at run time from cpuid. FIND_SIMD only probes the
# build machine by running test programs, and what it finds reaches nothing
# but the version report, so it is off: the library is the same wherever it
# is built. ENABLE_TIMESTAMPS would put the build date and host name in it.
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_STATIC_LIBS=OFF \
	-DBUILD_EXAMPLES=OFF \
	-DBUILD_AUTOTESTS=OFF \
	-DBUILD_BENCHMARKS=OFF \
	-DBUILD_SANDBOX=OFF \
	-DBUILD_DOC=OFF \
	-DENABLE_SIMD=ON \
	-DFIND_SIMD=OFF \
	-DFIND_FFTW=ON \
	-DFIND_THREADS=ON \
	-DENABLE_TIMESTAMPS=OFF \
	-DENABLE_STRICT=OFF
ninja -C build
DESTDIR=$PKG ninja -C build install

test -f "$PKG/usr/include/liquid/liquid.h"
test -f "$PKG/usr/lib/pkgconfig/liquid-dsp.pc"

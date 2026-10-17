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
	-DOIDN_STATIC_LIB=OFF \
	-DOIDN_LIBRARY_VERSIONED=ON \
	-DOIDN_DEVICE_CPU=ON \
	-DOIDN_DEVICE_SYCL=OFF \
	-DOIDN_DEVICE_CUDA=OFF \
	-DOIDN_DEVICE_HIP=OFF \
	-DOIDN_FILTER_RT=ON \
	-DOIDN_FILTER_RTLIGHTMAP=ON \
	-DOIDN_INSTALL_DEPENDENCIES=OFF \
	-DOIDN_WARN_AS_ERRORS=OFF \
	-DOIDN_APPS=ON \
	-DISPC_EXECUTABLE=/usr/bin/ispc \
	-DTBB_ROOT=/usr
cmake --build build
DESTDIR=$PKG cmake --install build

test -f "$PKG/usr/lib/libOpenImageDenoise.so"
test -n "$(find "$PKG/usr/lib" -name 'libOpenImageDenoise_device_cpu.so*' | head -1)"
test -x "$PKG/usr/bin/oidnDenoise"

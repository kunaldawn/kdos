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

# The native API only (libz-ng, zlib-ng.h, zlib-ng.pc): with ZLIB_COMPAT on it
# would install libz.so and zlib.h over the zlib port. SIMD paths are chosen
# at run time rather than compiled for the build machine, so the library runs
# on any x86_64.
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DZLIB_COMPAT=OFF \
	-DWITH_GZFILEOP=ON \
	-DWITH_NATIVE_INSTRUCTIONS=OFF \
	-DWITH_RUNTIME_CPU_DETECTION=ON \
	-DBUILD_TESTING=OFF \
	-DINSTALL_UTILS=OFF
ninja -C build
DESTDIR=$PKG ninja -C build install

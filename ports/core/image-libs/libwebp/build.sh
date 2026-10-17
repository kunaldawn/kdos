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

mkdir build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DWEBP_ENABLE_SIMD=ON \
	-DWEBP_BUILD_LIBWEBPMUX=ON \
	-DWEBP_BUILD_EXTRAS=ON \
	-DWEBP_BUILD_VWEBP=OFF \
	-DWEBP_ENABLE_SWAP_16BIT_CSP=ON
ninja
DESTDIR=$PKG ninja install
test -f "$PKG/usr/share/WebP/cmake/WebPConfig.cmake"
for _l in libwebp.so.7 libwebpdecoder.so.3 libwebpdemux.so.2 libwebpmux.so.3 libsharpyuv.so.0; do
	test -e "$PKG/usr/lib/$_l"
done

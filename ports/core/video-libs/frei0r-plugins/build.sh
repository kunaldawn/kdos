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

# FREI0R_VERSION is what frei0r.pc reports; left out, the project calls itself
# 3.0.0 whatever the tarball is. The OpenCV plugins (face detection and blur)
# are not built. gavl serves the RGB parade, vectorscope and scale0tilt
# plugins; the Cairo blend and gradient plugins are on, and shadert0y renders
# its shaders through EGL.
mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DFREI0R_VERSION=$version \
	-DBUILD_TESTING=OFF \
	-DWITHOUT_OPENCV=ON \
	-DWITHOUT_FACERECOGNITION=ON \
	-DWITHOUT_CAIRO=OFF \
	-DWITHOUT_GAVL=OFF
ninja
DESTDIR=$PKG ninja install
test -f "$PKG/usr/lib/frei0r-1/vectorscope.so"

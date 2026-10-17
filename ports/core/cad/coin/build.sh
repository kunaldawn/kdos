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

mkdir -p build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DCOIN_BUILD_SHARED_LIBS=ON \
	-DCOIN_BUILD_TESTS=OFF \
	-DCOIN_BUILD_EXAMPLES=OFF \
	-DCOIN_BUILD_DOCUMENTATION=OFF \
	-DCOIN_BUILD_GLX=ON \
	-DCOIN_BUILD_EGL=ON \
	-DCOIN_THREADSAFE=ON \
	-DHAVE_VRML97=ON \
	-DCOIN_HAVE_JAVASCRIPT=OFF \
	-DHAVE_SOUND=OFF \
	-DUSE_EXTERNAL_EXPAT=ON \
	-DFONTCONFIG_RUNTIME_LINKING=OFF \
	-DFREETYPE_RUNTIME_LINKING=OFF \
	-DZLIB_RUNTIME_LINKING=OFF \
	-DLIBBZIP2_RUNTIME_LINKING=OFF \
	-DSIMAGE_RUNTIME_LINKING=ON \
	-DOPENAL_RUNTIME_LINKING=ON \
	-DSPIDERMONKEY_RUNTIME_LINKING=ON
ninja
DESTDIR=$PKG ninja install
rm -rf "$PKG/usr/share/doc"

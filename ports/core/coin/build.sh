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

# GLX and EGL are both built. With both, Coin picks per context: EGL when an
# EGL context is current (every Wayland toolkit), GLX otherwise, and COIN_EGL
# forces either. Built with one only, offscreen rendering fails under the
# other.
#
# The *_RUNTIME_LINKING switches default to dlopen() by soname. Linked at
# build time instead, a missing library is a missing dependency the loader
# names, not a font or texture feature that silently does nothing. GLU is
# the exception: with EGL built, Coin never links it and always opens it at
# run time for NURBS tessellation, so glu is a run-time dependency.
# Sound (OpenAL), VRML JavaScript (SpiderMonkey) and simage are off: no
# consumer here uses them and the last two are not ports.
mkdir -p build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_TESTING=OFF \
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

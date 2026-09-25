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

# The standard is pinned, not left to the compiler's default. Abseil rewrites
# the installed absl/base/options.h from the standard it was built with, and a
# C++17 build pins its own ordering and source_location types rather than the
# C++20 std ones. That header then compiles and links the same under every
# standard from 17 up, so a consumer that forces c++17 (webrtc-audio-processing,
# gst-plugins-bad's webrtcdsp) and one on the compiler's newer default both
# match the ABI the libraries carry.
mkdir build
cd build
cmake .. \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_CXX_STANDARD=17 \
	-DCMAKE_CXX_STANDARD_REQUIRED=ON \
	-DCMAKE_SKIP_INSTALL_RPATH=ON \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_TESTING=OFF \
	-DABSL_BUILD_TESTING=OFF \
	-DABSL_BUILD_TEST_HELPERS=OFF \
	-DABSL_PROPAGATE_CXX_STD=ON \
	-DABSL_ENABLE_INSTALL=ON
make
make DESTDIR=$PKG install

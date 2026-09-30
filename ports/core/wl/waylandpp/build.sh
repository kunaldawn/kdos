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

# The client, EGL, cursor and server bindings, and the scanner that turns a
# protocol XML into C++ (Kodi generates its extra protocols with it). The
# Doxygen reference is off: it is found or not by a probe, and a build that
# happens to find Doxygen would ship an HTML tree nothing links to.
mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_SCANNER=ON \
	-DBUILD_LIBRARIES=ON \
	-DBUILD_SERVER=ON \
	-DBUILD_EXAMPLES=OFF \
	-DBUILD_DOCUMENTATION=OFF \
	-DINSTALL_UNSTABLE_PROTOCOLS=ON
ninja
DESTDIR=$PKG ninja install

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

# Headers and the CMake package only: tests, examples, docs, extras and the
# Python and Guile modules are off.
mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DDISABLE_WERROR=ON \
	-Dimmer_BUILD_TESTS=OFF \
	-Dimmer_BUILD_PERSIST_TESTS=OFF \
	-Dimmer_BUILD_EXAMPLES=OFF \
	-Dimmer_BUILD_DOCS=OFF \
	-Dimmer_BUILD_EXTRAS=OFF \
	-Dimmer_INSTALL_FUZZERS=OFF \
	-DENABLE_PYTHON=OFF \
	-DENABLE_GUILE=OFF
ninja
DESTDIR=$PKG ninja install

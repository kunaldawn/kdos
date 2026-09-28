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

# NAIF's own build is a set of csh scripts that make static archives. This
# is the CMake wrapper SDRangel's star tracker builds, which compiles the
# same N0067 sources into one shared library; a static archive is not
# position-independent enough to link into a plugin.
#
# The headers go under /usr/include/cspice: the toolkit ships f2c.h, fmt.h,
# fio.h and other generic names that do not belong in /usr/include itself.
# The kernels NAIF includes as examples go to /usr/share/cspice.
#
# The project turns on CMake's user package registry, which writes under
# $HOME at configure; HOME points into the work tree so nothing lands in the
# builder's home directory.
HOME="$SRC_ROOT" cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCSPICE_BUILD_STATIC_LIBRARY=OFF \
	-DINSTALL_LIB_DIR=/usr/lib \
	-DINSTALL_BIN_DIR=/usr/bin \
	-DINSTALL_INCLUDE_DIR=/usr/include/cspice \
	-DINSTALL_DATA_DIR=/usr/share \
	-DINSTALL_CMAKE_DIR=/usr/lib/cmake/cspice
ninja -C build
DESTDIR=$PKG ninja -C build install

test -f "$PKG/usr/lib/libcspice.so"
test -f "$PKG/usr/include/cspice/SpiceUsr.h"

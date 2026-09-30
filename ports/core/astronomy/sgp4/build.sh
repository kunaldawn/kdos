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

# The project declares GoogleTest through FetchContent whether or not the
# tests are built. FETCHCONTENT_SOURCE_DIR_GOOGLETEST points it at the copy
# this recipe carries, so configure never reaches the network; the tests and
# the example programs stay off.
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_TESTS=OFF \
	-DBUILD_EXAMPLES=OFF \
	-DFETCHCONTENT_FULLY_DISCONNECTED=ON \
	-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$SRC_ROOT/googletest-$_gtest"
ninja -C build
DESTDIR=$PKG ninja -C build install

# SDRangel's satellite tracker links the shared libsgp4s and includes the
# headers by bare name from /usr/include/libsgp4.
test -f "$PKG/usr/lib/libsgp4s.so"
test -f "$PKG/usr/include/libsgp4/SGP4.h"

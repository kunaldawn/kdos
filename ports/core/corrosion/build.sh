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

# taskwarrior is the only consumer. It exists as a port at all because
# taskwarrior's own route is FetchContent_Declare with a GIT_REPOSITORY, which
# is a clone in a build that has no network — and it offers
# -DSYSTEM_CORROSION=ON for exactly this case. find_package asks for no
# version, so the newest release serves.
#
# Nothing is compiled: corrosion parses `cargo metadata` in CMake itself, so
# what installs is the CMake package alone — CorrosionConfig under lib/cmake
# and the three modules under share/cmake.
mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DCORROSION_BUILD_TESTS=OFF
make
make DESTDIR=$PKG install

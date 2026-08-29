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

# 0.5.2 AND NOT 0.6.x: taskwarrior pins TASK_CORROSION_VERSION to v0.5.2 and is
# the only consumer here. It exists as a port at all because taskwarrior's own
# route is FetchContent_Declare with a GIT_REPOSITORY, which is a clone in a
# build that has no network — and it offers -DSYSTEM_CORROSION=ON for exactly
# this case.
#
# Nothing is compiled: what installs is the CMake package plus a small helper
# that corrosion builds to parse `cargo metadata`.
mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib
make
make DESTDIR=$PKG install

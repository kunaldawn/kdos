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

# HEADER-ONLY, SO THERE IS NOTHING TO COMPILE and the package is a tree of
# .hpp files plus the cmake config that lets a consumer find them. It is here
# for exactly one consumer — bpftrace, whose `find_package(LibCereal REQUIRED)`
# is unconditional — and adding a header set is cheaper than teaching that
# build to vendor one.
#
# JUST_INSTALL_CEREAL skips the test and sandbox targets, which want boost.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DJUST_INSTALL_CEREAL=ON \
	-DBUILD_DOC=OFF \
	-DBUILD_SANDBOX=OFF \
	-DSKIP_PERFORMANCE_COMPARISON=ON
ninja
DESTDIR=$PKG ninja install

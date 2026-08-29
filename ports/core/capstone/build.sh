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

# THE 5.x LINE, NOT 6.0-Alpha. Upstream's newest tag is an alpha and rizin
# builds against the stable ABI; a disassembler engine is the wrong place to
# take a prerelease, because the failure is a wrong instruction rather than a
# crash and nothing downstream can tell.
#
# Every architecture is built. A curated subset saves a few megabytes and turns
# "open this firmware image" into "open this firmware image if it happens to be
# one of the four I guessed", which on a machine meant for looking at unknown
# binaries is the wrong trade.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DCAPSTONE_BUILD_STATIC_RUNTIME=OFF \
	-DCAPSTONE_BUILD_TESTS=OFF \
	-DCAPSTONE_BUILD_CSTOOL=ON
ninja
DESTDIR=$PKG ninja install

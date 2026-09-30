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

# DEPS=LOCAL: under AUTO, a dependency missing from the system is downloaded,
# and this build has no network. zstd and xxhash are the ports in depends; the
# four DEP_*=BUNDLED are the sources ccache carries, pinned because each finder
# otherwise takes a system copy whenever one happens to be installed — fmt is
# a 41_system port, and a 30_foundation build and a later rebuild would differ.
# CCACHE_DEV_MODE=OFF: dev mode, which a CI variable in the environment turns
# on, adds -Werror and links with lld or gold, whichever the chroot holds.
cmake -S . -B build -G Ninja \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_INSTALL_LIBEXECDIR=lib \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_C_FLAGS_RELEASE="$CFLAGS" \
	-DCMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS" \
	-DDEPS=LOCAL \
	-DDEP_FMT=BUNDLED \
	-DDEP_BLAKE3=BUNDLED \
	-DDEP_CPPHTTPLIB=BUNDLED \
	-DDEP_TLEXPECTED=BUNDLED \
	-DCCACHE_DEV_MODE=OFF \
	-DHTTP_STORAGE_BACKEND=ON \
	-DREDIS_STORAGE_BACKEND=OFF \
	-DENABLE_TESTING=OFF \
	-DENABLE_DOCUMENTATION=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# The masquerade directory: first in PATH, each name is ccache standing in for
# the compiler it is named after. A compiler missing here is one a build
# through the directory runs uncached, silently.
install -d $PKG/usr/lib/ccache
for c in gcc g++ cc c++ clang clang++; do
	ln -s /usr/bin/ccache $PKG/usr/lib/ccache/$c
done

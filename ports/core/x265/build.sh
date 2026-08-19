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

# GPL-2+, same as x264 — see ports/core/x264/build.sh for what that does to the
# shipped ffmpeg.
#
# 8-bit only. The 10- and 12-bit depths are built as separate static archives
# linked into one multilib .so, which triples the build time for depths that
# matter to broadcast mastering and to nothing this distro does.
# CMake 4 removed OLD support for CMP0025 and CMP0054, which this CMakeLists
# sets explicitly — an explicit set beats CMAKE_POLICY_DEFAULT_*, so a flag
# cannot reach it. Both are no-ops for this build: CMP0025 renames Apple's
# compiler id and nothing here is Apple, and the only quoted if() arguments in
# the tree are "${CMAKE_SIZEOF_VOID_P}", which expands to 4 or 8 and so names
# no variable either way.
patch -p1 -i $PORT_SRC/cmake-policies-new.patch

cd source
cmake . \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DENABLE_SHARED=ON \
	-DENABLE_CLI=ON \
	-DENABLE_TESTS=OFF
make
make DESTDIR=$PKG install

# Nothing here links x265 statically.
rm -f "$PKG/usr/lib/libx265.a"

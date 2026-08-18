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

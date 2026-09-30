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

# The GF(256) arithmetic has no scalar path: it is written for SSSE3 on x86
# and NEON on Arm. ENABLE_DISTRIBUTION names that fixed floor instead of
# probing the build machine, which would bake in AVX2 or AVX-512 wherever the
# builder has them. On x86-64 the library therefore needs an SSSE3 processor.
# The unit-test tools are not built.
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DENABLE_DISTRIBUTION=ON \
	-DBUILD_TOOLS=OFF \
	-Wno-dev
ninja -C build
DESTDIR=$PKG ninja -C build install

test -f "$PKG/usr/lib/pkgconfig/libcm256cc.pc"
test -f "$PKG/usr/include/cm256cc/cm256.h"

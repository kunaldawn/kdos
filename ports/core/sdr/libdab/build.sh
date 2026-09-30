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

# The library is the DAB and DAB+ decoder of dab-cmdline, in its own
# directory; SDRangel's DAB demodulator links it. Its install rule takes the
# library directory from LIB_INSTALL_DIR, which it never sets, and would put
# the library in /usr itself. The Viterbi decoder has an SSE2 form, which is
# the x86-64 baseline; elsewhere the portable C one is built, since the NEON
# switch also forces 32-bit Arm code generation.
_simd=
case "$(uname -m)" in
	x86_64) _simd=-DX64_DEFINED=ON ;;
esac

cmake -S library -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DLIB_INSTALL_DIR=lib \
	$_simd \
	-Wno-dev
ninja -C build
DESTDIR=$PKG ninja -C build install

test -f "$PKG/usr/lib/libdab_lib.so"
test -f "$PKG/usr/include/dab_lib/dab-api.h"

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

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

HOME="$SRC_ROOT" cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCSPICE_BUILD_STATIC_LIBRARY=OFF \
	-DINSTALL_LIB_DIR=/usr/lib \
	-DINSTALL_BIN_DIR=/usr/bin \
	-DINSTALL_INCLUDE_DIR=/usr/include/cspice \
	-DINSTALL_DATA_DIR=/usr/share \
	-DINSTALL_CMAKE_DIR=/usr/lib/cmake/cspice
ninja -C build
DESTDIR=$PKG ninja -C build install
install -Dm644 README.md "$PKG/usr/share/licenses/$name/README.md"
sed -n '/^-Disclaimer/,/^-Required_Reading/{/^-Required_Reading/!p}' include/SpiceUsr.h \
	> "$PKG/usr/share/licenses/$name/DISCLAIMER"
grep -q 'WITHOUT WARRANTY OF ANY KIND' "$PKG/usr/share/licenses/$name/DISCLAIMER"

test -f "$PKG/usr/lib/libcspice.so"
test -f "$PKG/usr/include/cspice/SpiceUsr.h"

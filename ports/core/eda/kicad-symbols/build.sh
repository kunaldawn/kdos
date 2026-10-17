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

cmake -S . -B build -G Ninja \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DKICAD_PACK_SYM_LIBRARIES=ON
ninja -C build
DESTDIR=$PKG ninja -C build install

test -f "$PKG/usr/share/kicad/template/sym-lib-table"
test -f "$PKG/usr/share/kicad/symbols/Device.kicad_sym"

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


# The libraries are packed from their per-symbol .kicad_symdir trees into one
# .kicad_sym file each, the form KiCad loads fastest; the packed sym-lib-table
# lands in the template folder, where KiCad copies it on first run.
cmake -S . -B build -G Ninja \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DKICAD_PACK_SYM_LIBRARIES=ON
ninja -C build
DESTDIR=$PKG ninja -C build install

test -f "$PKG/usr/share/kicad/template/sym-lib-table"
test -f "$PKG/usr/share/kicad/symbols/Device.kicad_sym"

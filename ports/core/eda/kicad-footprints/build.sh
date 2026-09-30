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


# The fp-lib-table lands in the template folder, where KiCad copies it on
# first run.
cmake -S . -B build -G Ninja \
	-DCMAKE_INSTALL_PREFIX=/usr
DESTDIR=$PKG ninja -C build install

test -f "$PKG/usr/share/kicad/template/fp-lib-table"
test -d "$PKG/usr/share/kicad/footprints/Resistor_SMD.pretty"

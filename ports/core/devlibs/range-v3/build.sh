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

# Header-only. Tests, examples, benchmarks and docs are off, so nothing is
# compiled; RANGES_NATIVE would otherwise put -march=native into anything that
# is.
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DRANGE_V3_TESTS=OFF \
	-DRANGE_V3_EXAMPLES=OFF \
	-DRANGE_V3_PERF=OFF \
	-DRANGE_V3_DOCS=OFF \
	-DRANGE_V3_HEADER_CHECKS=OFF \
	-DRANGES_NATIVE=OFF \
	-DRANGES_ENABLE_WERROR=OFF
make
make DESTDIR=$PKG install

# A module map at the top of /usr/include would claim every system header for
# range-v3 in any clang build with -fmodules; it only serves upstream's own
# module test build.
rm "$PKG/usr/include/module.modulemap"

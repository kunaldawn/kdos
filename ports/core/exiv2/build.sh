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

# THE THING THAT MAKES A PHOTO ARCHIVE SEARCHABLE OFFLINE. Every camera writes
# the date, the lens and often the position into the file; without something
# that reads it, 40 000 photos on a stick are 40 000 files named DSC_*.
# recoll's image filter and any thumbnailer call this.
#
# EXIV2_ENABLE_INIH=ON uses the `inih` port rather than exiv2's vendored copy.
# Two inih implementations on one machine is two parsers of the same config
# format, and the one nobody is looking at is the one that drifts.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DEXIV2_ENABLE_PNG=ON \
	-DEXIV2_ENABLE_BROTLI=ON \
	-DEXIV2_ENABLE_INIH=ON \
	-DEXIV2_BUILD_SAMPLES=OFF \
	-DEXIV2_BUILD_UNIT_TESTS=OFF \
	-DEXIV2_BUILD_EXIV2_COMMAND=ON
ninja
DESTDIR=$PKG ninja install

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

# THE 2.x LINE, because mupdf is written against it. mupdf 1.28 includes the
# headers by bare name ("ReadBarcode.h"), which 2.x's zxing.pc puts on the
# include path and 3.x's does not, and its own copy under thirdparty/ is 2.3.
#
# ZXING_WRITERS=OLD is the encoder mupdf's barcode creation calls when the
# library has no experimental API. The NEW writer is libzint, which zxing-cpp
# bundles and which is not a port. ZXING_DEPENDENCIES=LOCAL stops CMake from
# fetching anything from GitHub, and the examples, which would fetch stb, are
# off.
cmake -B build -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DZXING_READERS=ON \
	-DZXING_WRITERS=OLD \
	-DZXING_USE_BUNDLED_ZINT=OFF \
	-DZXING_EXPERIMENTAL_API=OFF \
	-DZXING_C_API=OFF \
	-DZXING_DEPENDENCIES=LOCAL \
	-DZXING_EXAMPLES=OFF \
	-DZXING_UNIT_TESTS=OFF \
	-DZXING_BLACKBOX_TESTS=OFF \
	-DZXING_PYTHON_MODULE=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

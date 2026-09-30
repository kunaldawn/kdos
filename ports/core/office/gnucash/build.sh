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


# GnuCash prepends -Werror to the compiler flags on every Unix build, and a
# newer compiler's new warning then stops it; -Wno-error comes after it on
# the command line and wins. The book is kept in XML: libdbi has no port, so
# the SQLite and database backends are off. AqBanking (online banking) and
# libofx have no port, so HBCI and OFX import are off; QIF and CSV import are
# built in. The Python bindings are off. GTest is
# REQUIRED at configure time whether or not the tests are run. The GSettings
# schemas are compiled by kpkg's shared-index step. DISABLE_NLS leaves out
# every translation: bundled data is English only. Reports render in
# WebKitGTK's 4.1 API.
export CFLAGS="$CFLAGS -Wno-error"
export CXXFLAGS="$CXXFLAGS -Wno-error"
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_SHARED_LIBS=ON \
	-D WITH_GNUCASH=ON \
	-D WITH_SQL=OFF \
	-D WITH_AQBANKING=OFF \
	-D WITH_OFX=OFF \
	-D WITH_PYTHON=OFF \
	-D DISABLE_NLS=ON \
	-D COMPILE_GSCHEMAS=OFF \
	-D ENABLE_BINRELOC=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

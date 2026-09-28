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

# THE 3.6 LONG-TERM LINE, NOT 4.x: OBS Studio and RetroArch are written to the
# 3.x API, and ImHex builds against either.
#
# THREADING IS PATCHED INTO THE INSTALLED CONFIG HEADER, not passed as a
# define: MBEDTLS_THREADING_C adds a mutex to the RNG and SSL contexts, so a
# consumer compiled without it would see smaller structures than the library
# fills. threading-pthread.patch turns on MBEDTLS_THREADING_C and
# MBEDTLS_THREADING_PTHREAD in include/mbedtls/mbedtls_config.h.
#
# GEN_FILES is off because the release tarball carries the generated sources,
# so neither Python nor Perl runs. MBEDTLS_FATAL_WARNINGS is upstream's
# -Werror, off so a newer compiler's warning is not a failed build.
patch -p1 -i "$PORT_SRC/threading-pthread.patch"

mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DUSE_SHARED_MBEDTLS_LIBRARY=ON \
	-DUSE_STATIC_MBEDTLS_LIBRARY=OFF \
	-DGEN_FILES=OFF \
	-DENABLE_PROGRAMS=OFF \
	-DENABLE_TESTING=OFF \
	-DMBEDTLS_FATAL_WARNINGS=OFF
ninja
DESTDIR=$PKG ninja install

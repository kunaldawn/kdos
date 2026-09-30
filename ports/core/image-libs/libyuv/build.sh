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

# Upstream publishes no release archive: googlesource generates a different
# tarball on every request, so no checksum could hold. The source is Debian's
# orig tarball of the commit that set LIBYUV_VERSION.
#
# The static archive is always built and installed; nothing here links it.
mkdir -p build && cd build
cmake .. \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DUNIT_TEST=OFF \
	-DLIBYUV_DISABLE_JPEG=OFF \
	-DCMAKE_REQUIRE_FIND_PACKAGE_JPEG=ON
make
make DESTDIR=$PKG install
rm -f "$PKG/usr/lib/libyuv.a"

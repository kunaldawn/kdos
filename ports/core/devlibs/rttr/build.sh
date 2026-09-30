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

# rttr installs every public header with mode 0400, which no user but root can
# read, so nothing built as anybody else could include them; the patch drops
# that mode from the header install. The licence and README keep their 0400
# from a separate install rule and are made readable after the install. Only
# the shared library is built: no static copy, tests, examples or
# documentation.
patch -p1 -i "$PORT_SRC/permission.patch"
mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_RTTR_DYNAMIC=ON \
	-DBUILD_STATIC=OFF \
	-DBUILD_WITH_STATIC_RUNTIME_LIBS=OFF \
	-DBUILD_UNIT_TESTS=OFF \
	-DBUILD_BENCHMARKS=OFF \
	-DBUILD_EXAMPLES=OFF \
	-DBUILD_DOCUMENTATION=OFF \
	-DBUILD_INSTALLER=ON \
	-DBUILD_PACKAGE=OFF
ninja
DESTDIR=$PKG ninja install
find "$PKG/usr/share/rttr" -maxdepth 1 -type f -exec chmod 644 {} +

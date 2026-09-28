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

# output.h names uint8_t without including <stdint.h>, which libstdc++
# from gcc 15 on no longer pulls in by accident. The upstream CMakeLists installs
# the three tools only in a static build; the second patch installs them
# always. CMAKE_SKIP_RPATH drops the relative run path the CMakeLists sets.
patch -p1 -i "$PORT_SRC/gcc15.patch"
patch -p1 -i "$PORT_SRC/install-executables.patch"

cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_SKIP_RPATH=ON \
	-DBUILD_SHARED_LIBS=ON
cmake --build build
DESTDIR=$PKG cmake --install build

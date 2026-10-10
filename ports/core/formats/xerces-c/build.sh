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

# ICU's headers need C++17, and CMakeLists.txt sets CMAKE_CXX_STANDARD 14 as a
# plain variable, which a -D on the command line cannot override.
patch -p1 -i "$PORT_SRC/cxx17.patch"

# network=OFF: with it on, a parser resolving an external DTD or schema
# fetches it over HTTP at run time, which here fails late and silently.
mkdir -p build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF \
	-Dnetwork=OFF \
	-Dtranscoder=icu \
	-Dmessage-loader=inmemory \
	-Dthreads=ON
ninja
DESTDIR=$PKG ninja install
rm -rf "$PKG/usr/share/doc"

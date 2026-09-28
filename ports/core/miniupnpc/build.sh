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

# includes.patch (Alpine's) makes upnpcommands.h include <stddef.h> for the
# size_t it declares with, which musl's headers do not pull in on the way.
# The sample tools are off: built against the shared library they install as
# upnpc-shared and upnp-listdevices-shared. The Python module is not built.
patch -p1 -i "$PORT_SRC/includes.patch"

mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DUPNPC_BUILD_SHARED=ON \
	-DUPNPC_BUILD_STATIC=OFF \
	-DUPNPC_BUILD_TESTS=OFF \
	-DUPNPC_BUILD_SAMPLE=OFF
ninja
DESTDIR=$PKG ninja install

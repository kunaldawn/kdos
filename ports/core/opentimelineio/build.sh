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

# The release archive carries its submodule directories empty. RapidJSON is
# the commit upstream pins, unpacked beside the source; the tree's rapidjson
# port is the 1.1.0 release, years older than the API this code is written
# against. Imath is the imath port. The Python bindings and the Python
# command-line tools are off: the consumer is Kdenlive's C++.
rmdir src/deps/rapidjson
mv "$SRC_ROOT/rapidjson-$_rjcommit" src/deps/rapidjson

mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_TESTING=OFF \
	-DOTIO_SHARED_LIBS=ON \
	-DOTIO_CXX_INSTALL=ON \
	-DOTIO_PYTHON_INSTALL=OFF \
	-DOTIO_INSTALL_PYTHON_MODULES=OFF \
	-DOTIO_INSTALL_COMMANDLINE_TOOLS=OFF \
	-DOTIO_DEPENDENCIES_INSTALL=OFF \
	-DOTIO_FIND_IMATH=ON \
	-DOTIO_FIND_RAPIDJSON=OFF \
	-DOTIO_AUTOMATIC_SUBMODULES=OFF \
	-DOTIO_CXX_EXAMPLES=OFF \
	-DOTIO_CXX_COVERAGE=OFF
ninja
DESTDIR=$PKG ninja install

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

patch -p1 -i "$PORT_SRC/musl-size_t.patch"

cd Project/CMake
cmake . -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DBUILD_SHARED_LIBS=ON -DBUILD_ZENLIB=0 -DBUILD_ZLIB=0 \
	-DCURL_NO_CURL_CMAKE=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_CURL=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_PkgConfig=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_TinyXML=ON
make
make DESTDIR=$PKG install

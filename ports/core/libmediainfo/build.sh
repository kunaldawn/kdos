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

# A RULE-7 PATCH: the defect is in a header this port INSTALLS, so no flag on
# this build can fix it — every consumer of MediaInfoDLL.h would need the same
# flag, and mediainfo's own CLI is the first to hit it.
patch -p1 -i "$PORT_SRC/musl-size_t.patch"

# curl, pkg-config and tinyxml2 are each an optional find_package: without
# curl the library links nothing and dlopens libcurl at run time, and without
# pkg-config it installs no libmediainfo.pc for mediainfo to build against.
# Both are required here. CURL_NO_CURL_CMAKE sends FindCURL straight to
# pkg-config: its first step asks for a CURLConfig.cmake, and the requirement
# turns that step's miss into a hard stop. tinyxml2 is not a port, so the
# bundled copy is named rather than left to whatever the chroot holds.
cd Project/CMake
cmake . -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON -DBUILD_ZENLIB=0 -DBUILD_ZLIB=0 \
	-DCURL_NO_CURL_CMAKE=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_CURL=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_PkgConfig=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_TinyXML=ON
make
make DESTDIR=$PKG install

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


cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DLIBICAL_STATIC=OFF \
	-DLIBICAL_CXX_BINDINGS=ON \
	-DLIBICAL_JAVA_BINDINGS=OFF \
	-DLIBICAL_GLIB=OFF \
	-DLIBICAL_GOBJECT_INTROSPECTION=OFF \
	-DLIBICAL_GLIB_VAPI=OFF \
	-DLIBICAL_GLIB_BUILD_DOCS=OFF \
	-DLIBICAL_BUILD_DOCS=OFF \
	-DLIBICAL_BUILD_EXAMPLES=OFF \
	-DLIBICAL_BUILD_TESTING=OFF \
	-DLIBICAL_BUILD_VZIC=OFF \
	-DLIBICAL_ENABLE_BUILTIN_TZDATA=OFF \
	-DCMAKE_REQUIRE_FIND_PACKAGE_ICU=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_BerkeleyDB=ON
cmake --build build
DESTDIR=$PKG cmake --install build

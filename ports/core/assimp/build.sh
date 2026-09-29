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

# zlib and minizip are the system's; the other helpers (pugixml, poly2tri,
# clipper, openddl-parser, rapidjson, stb, utf8cpp) are compiled from the
# copies under contrib/, which assimp uses without a package manager.
# Warnings are not errors: upstream's -Werror meets this compiler's newer
# warnings. The version string is the release's, not asked of git.
#
# The zip reader and the IFC importer include <unzip.h>, but minizip installs
# its headers under /usr/include/minizip and its minizip.pc names only
# /usr/include, so the directory is added here. Nothing built includes a
# <zip.h> that the minizip one could shadow.
export CXXFLAGS="$CXXFLAGS -I/usr/include/minizip"
mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DASSIMP_BUILD_ZLIB=OFF \
	-DASSIMP_BUILD_TESTS=OFF \
	-DASSIMP_BUILD_SAMPLES=OFF \
	-DASSIMP_BUILD_ASSIMP_TOOLS=OFF \
	-DASSIMP_BUILD_DOCS=OFF \
	-DASSIMP_BUILD_DRACO=OFF \
	-DASSIMP_BUILD_USE_CCACHE=OFF \
	-DASSIMP_WARNINGS_AS_ERRORS=OFF \
	-DASSIMP_INJECT_DEBUG_POSTFIX=OFF \
	-DASSIMP_IGNORE_GIT_HASH=ON \
	-DASSIMP_HUNTER_ENABLED=OFF
ninja
DESTDIR=$PKG ninja install

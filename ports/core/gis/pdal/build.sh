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

patch -p1 -i "$PORT_SRC/10-zstd.patch"
cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_SHARED_LIBS=ON \
	-D WITH_TESTS=OFF \
	-D BUILD_DOCS=OFF \
	-D ENABLE_CTEST=OFF \
	-D WITH_ABSEIL=OFF \
	-D WITH_BACKTRACE=OFF \
	-D WITH_COMPLETION=OFF \
	-D WITH_GCS=OFF \
	-D WITH_ZLIB=ON \
	-D WITH_ZSTD=ON \
	-D WITH_LZMA=ON \
	-D BUILD_TOOLS_LASDUMP=ON \
	-D BUILD_TOOLS_NITFWRAP=OFF \
	-D BUILD_PLUGIN_DRACO=ON \
	-D BUILD_PLUGIN_HDF=ON \
	-D BUILD_PLUGIN_ICEBRIDGE=ON \
	-D BUILD_PLUGIN_E57=ON \
	-D BUILD_PLUGIN_PGPOINTCLOUD=ON \
	-D BUILD_PGPOINTCLOUD_TESTS=OFF \
	-D BUILD_PLUGIN_TRAJECTORY=ON \
	-D BUILD_PLUGIN_ARROW=OFF \
	-D BUILD_PLUGIN_CPD=OFF \
	-D BUILD_PLUGIN_FBX=OFF \
	-D BUILD_PLUGIN_MATLAB=OFF \
	-D BUILD_PLUGIN_MBIO=OFF \
	-D BUILD_PLUGIN_NITF=OFF \
	-D BUILD_PLUGIN_OPENSCENEGRAPH=OFF \
	-D BUILD_PLUGIN_RDBLIB=OFF \
	-D BUILD_PLUGIN_RIVLIB=OFF \
	-D BUILD_PLUGIN_SPZ=OFF \
	-D BUILD_PLUGIN_TEASER=OFF \
	-D BUILD_PLUGIN_TILEDB=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

install -Dm644 scripts/bash-completion/pdal \
	"$PKG/usr/share/bash-completion/completions/pdal"

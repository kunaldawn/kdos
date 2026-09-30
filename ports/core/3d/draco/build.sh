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

# The patches install headers at their real paths and the CMake package under
# lib/cmake/draco, with DRACO_LIBRARIES set, so find_package(draco) in PDAL,
# Assimp and OpenCASCADE resolves to the shared library.
#
# The transcoder is off: it needs the tinygltf, Eigen and filesystem
# submodules, which the release archive carries empty.
patch -p1 -i "$PORT_SRC/0001-Fix-removal-of-build-dir-prefix-from-include-path.patch"
patch -p1 -i "$PORT_SRC/0002-Install-proper-CMake-targets.patch"
patch -p1 -i "$PORT_SRC/0004-Set-DRACO_LIBRARIES-for-backwards-compatibility.patch"
cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_SHARED_LIBS=ON \
	-D DRACO_TESTS=OFF \
	-D DRACO_TRANSCODER_SUPPORTED=OFF \
	-D DRACO_MESH_COMPRESSION=ON \
	-D DRACO_POINT_CLOUD_COMPRESSION=ON \
	-D DRACO_JS_GLUE=OFF \
	-D DRACO_WASM=OFF \
	-D DRACO_UNITY_PLUGIN=OFF \
	-D DRACO_MAYA_PLUGIN=OFF \
	-D DRACO_ANIMATION_ENCODING=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

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

patch -p1 -i "$PORT_SRC/no_mallinfo.patch"
patch -p1 -i "$PORT_SRC/no_feenableexcept.patch"
patch -p1 -i "$PORT_SRC/no_backtrace.patch"

mkdir -p build && cd build
cmake .. -G Ninja -Wno-dev -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DBUILD_LIBRARY_TYPE=Shared \
	-DINSTALL_DIR_LIB=lib \
	-DINSTALL_DIR_CMAKE=lib/cmake/opencascade \
	-DINSTALL_DIR_SCRIPT=share/opencascade/scripts \
	-DBUILD_MODULE_Draw=OFF \
	-DBUILD_DOC_Overview=OFF \
	-DINSTALL_SAMPLES=OFF \
	-DINSTALL_TEST_CASES=OFF \
	-DUSE_XLIB=OFF \
	-DUSE_OPENGL=ON \
	-DUSE_GLES2=OFF \
	-DUSE_TK=OFF \
	-DUSE_FREETYPE=ON \
	-DUSE_RAPIDJSON=ON \
	-DUSE_FREEIMAGE=OFF \
	-DUSE_FFMPEG=OFF \
	-DUSE_DRACO=ON \
	-DUSE_TBB=OFF \
	-DUSE_EIGEN=OFF \
	-DUSE_VTK=OFF \
	-DUSE_OPENVR=OFF
ninja
DESTDIR=$PKG ninja install
rm -rf "$PKG/usr/share/doc"
readelf -d "$PKG/usr/lib/libTKDEGLTF.so" | grep -q libdraco

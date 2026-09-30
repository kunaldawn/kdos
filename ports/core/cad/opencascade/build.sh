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

# musl has no mallinfo(), feenableexcept() or backtrace(); each patch drops
# the call, so memory statistics, FPE trapping and stack traces report
# nothing instead of failing to link.
patch -p1 -i "$PORT_SRC/no_mallinfo.patch"
patch -p1 -i "$PORT_SRC/no_feenableexcept.patch"
patch -p1 -i "$PORT_SRC/no_backtrace.patch"

# USE_XLIB=OFF builds TKOpenGl against EGL, so the OCCT viewer works in any
# EGL context, Wayland's included; with it on, the viewer is GLX and X11
# only. Draw (DRAWEXE, the Tcl/Tk test shell) is not a consumer's API, and
# its env scripts would land in /usr/bin as env.sh and custom.sh, so the
# script directory is moved out of PATH. Draco decodes and encodes
# compressed glTF meshes. FFmpeg video export needs the pre-5 API,
# FreeImage is not a port, and the VTK bridge serves no consumer here.
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

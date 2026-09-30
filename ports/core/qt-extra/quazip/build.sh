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

# The Qt 6 build only: libquazip1-qt6 and the QuaZip-Qt6 CMake package, which
# is what Krita and TeXstudio look for. QTEXTCODEC decodes legacy file-name
# encodings through Qt6 Core5Compat. QUAZIP_FETCH_LIBS stays off, so a missing
# zlib or bzip2 is an error rather than a download.
mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DQUAZIP_QT_MAJOR_VERSION=6 \
	-DQUAZIP_INSTALL=ON \
	-DQUAZIP_USE_QT_ZLIB=OFF \
	-DQUAZIP_ENABLE_QTEXTCODEC=ON \
	-DQUAZIP_BZIP2=ON \
	-DQUAZIP_BZIP2_STDIO=ON \
	-DQUAZIP_FETCH_LIBS=OFF \
	-DQUAZIP_FORCE_FETCH_LIBS=OFF \
	-DQUAZIP_ENABLE_TESTS=OFF
ninja
DESTDIR=$PKG ninja install

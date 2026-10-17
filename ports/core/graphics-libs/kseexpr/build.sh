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

mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DENABLE_QT6=ON \
	-DENABLE_LLVM_BACKEND=OFF \
	-DUSE_PREGENERATED_FILES=OFF \
	-DBUILD_TRANSLATIONS=OFF \
	-DCMAKE_DISABLE_FIND_PACKAGE_KF6I18n=ON \
	-DBUILD_UTILS=OFF \
	-DBUILD_DEMOS=OFF \
	-DBUILD_DOC=OFF \
	-DBUILD_TESTS=OFF \
	-DENABLE_SLOW_TESTS=OFF \
	-DENABLE_PERFORMANCE_STATS=OFF
ninja
DESTDIR=$PKG ninja install

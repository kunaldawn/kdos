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

# Every path carries the qt5 suffix (libqca-qt5, include/Qca-qt5,
# lib/qca-qt5/crypto, qca2-qt5.pc, Qca-qt5 CMake package), so this installs
# beside qca's Qt 6 build without sharing a file. The ossl provider links the
# system OpenSSL, not the OpenSSL 3 that Qt 5's network module links.
cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_WITH_QT6=OFF \
	-D QCA_SUFFIX=qt5 \
	-D BUILD_TESTS=OFF \
	-D BUILD_TOOLS=OFF \
	-D BUILD_PLUGINS="ossl;gnupg;cyrus-sasl;logger;softstore" \
	-D QCA_FEATURE_INSTALL_DIR=/usr/lib/qt5/mkspecs/features \
	-D CMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

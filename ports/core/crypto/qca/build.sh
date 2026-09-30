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

# Qt 6 only, installed as libqca-qt6 with its providers under
# /usr/lib/qca-qt6/crypto. BUILD_PLUGINS names the providers built; a
# provider left out of the list is never searched for, so a missing library
# fails configure rather than dropping the provider. gnupg drives the gpg
# binary at run time.
#
# The qmake feature file goes beside Qt's own mkspecs; its default is
# /usr/mkspecs, which nothing reads.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_WITH_QT6=ON \
	-D BUILD_TESTS=OFF \
	-D BUILD_TOOLS=OFF \
	-D BUILD_PLUGINS="ossl;gnupg;cyrus-sasl;logger;softstore" \
	-D QCA_FEATURE_INSTALL_DIR=/usr/lib/qt6/mkspecs/features \
	-D CMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

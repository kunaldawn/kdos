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

# libsecret is the Secret Service path. With no Secret Service provider on
# the session bus a job fails, unless the application turned on qtkeychain's
# insecure fallback, which keeps the secret in the application's own settings
# file. Translations are off: bundled data is English only.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_SHARED_LIBS=ON \
	-D BUILD_TESTING=OFF \
	-D BUILD_WITH_QT5=OFF \
	-D LIBSECRET_SUPPORT=ON \
	-D BUILD_TRANSLATIONS=OFF \
	-D BUILD_TEST_APPLICATION=OFF \
	-D BUILD_QTQUICK_DEMO=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

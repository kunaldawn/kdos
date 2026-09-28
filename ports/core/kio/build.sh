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

# KF_SKIP_PO_PROCESSING leaves every translation out: bundled data is English
# only, and without it each language's catalogue is compiled and installed.
# X11 and Wayland are both on: the toolkits carry both backends and a program
# run under Xwayland gets its startup notification and window activation from
# the X11 half. kpasswdserver is built against KWallet and stores passwords only
# where a kwalletd answers; without one they last only the session.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D BUILD_QCH=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D BUILD_DESIGNERPLUGIN=OFF \
	-D WITH_X11=ON \
	-D WITH_WAYLAND=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

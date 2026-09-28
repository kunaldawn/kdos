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
#
# Qt 6 only. The Designer plugin and the settings program are off: the first
# needs Qt Designer's plugin API, the second duplicates the audio settings
# the session already has. libpulse is found explicitly, so device listing
# goes through pipewire-pulse; without it Phonon quietly lists nothing.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D PHONON_BUILD_QT5=OFF \
	-D PHONON_BUILD_QT6=ON \
	-D PHONON_BUILD_EXPERIMENTAL=ON \
	-D PHONON_BUILD_DESIGNER_PLUGIN=OFF \
	-D PHONON_BUILD_SETTINGS=OFF \
	-D PHONON_BUILD_DEMOS=OFF \
	-D PHONON_BUILD_DOC=OFF \
	-D CMAKE_REQUIRE_FIND_PACKAGE_PulseAudio=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_GLIB2=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

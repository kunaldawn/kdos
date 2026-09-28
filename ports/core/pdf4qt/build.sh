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


# The INSTALL_*DEPENDENCIES switches copy the Qt and library files a vcpkg
# build links into the package; on a system build they would install second
# copies of other ports' libraries. The scanner plugin compiles its SANE
# backend only when libsane is found, and is otherwise an empty entry in the
# editor's menu. Every translation catalogue is installed whatever the build
# is told, so all but English are removed from the package: bundled data is
# English only.
cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D PDF4QT_BUILD_TESTS=OFF \
	-D PDF4QT_INSTALL_DEPENDENCIES=OFF \
	-D PDF4QT_INSTALL_QT_DEPENDENCIES=OFF \
	-D PDF4QT_INSTALL_TO_USR=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

find "$PKG/usr/share/pdf4qt/translations" -name '*.qm' ! -name '*_en.qm' -delete

# Each window's app_id is its executable's name, and each entry's file id
# ends in that name except the launch pad's, which ends in Pdf4qt. Its entry
# names the window in StartupWMClass, or the taskbar cannot match the launch
# pad's window to its Start menu row.
printf 'StartupWMClass=Pdf4QtLaunchPad\n' \
	>> "$PKG/usr/share/applications/io.github.JakubMelka.Pdf4qt.desktop"

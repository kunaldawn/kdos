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


# KF_SKIP_PO_PROCESSING leaves the interface catalogues out: bundled data is
# English only. kdoctools_install() builds every translated handbook and
# manual page too, and those are removed after the install. OKULAR_UI=desktop
# builds the widget shell alone; the mobile shell needs Kirigami at run time
# and has no launcher here. Every generator's library is REQUIRED unless named
# in FORCE_NOT_REQUIRED_DEPENDENCIES, so a missing one stops the configure
# instead of dropping the format: only Mobipocket, whose library has no port,
# is downgraded, and those files do not open. The PDF generator needs poppler
# built with its Qt 6 frontend; without it the build fails here rather than
# shipping a viewer that cannot read a PDF.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D OKULAR_UI=desktop \
	-D USE_DBUS=ON \
	-D FORCE_NOT_REQUIRED_DEPENDENCIES=QMobiPocket6 \
	-D CMAKE_DISABLE_FIND_PACKAGE_QMobipocket6=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

if [ -d "$PKG/usr/share/doc/HTML" ]; then
	find "$PKG/usr/share/doc/HTML" -mindepth 1 -maxdepth 1 ! -name en -exec rm -rf {} +
fi
if [ -d "$PKG/usr/share/man" ]; then
	find "$PKG/usr/share/man" -mindepth 1 -maxdepth 1 ! -name 'man*' -exec rm -rf {} +
fi

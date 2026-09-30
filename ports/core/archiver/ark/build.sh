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

patch -p1 -i "$PORT_SRC/app-id.patch"

# libarchive is the engine for tar and its compressions, 7z, rar (read only),
# iso and cpio; libzip is required as well, because without it zip archives
# fall to the unzip/zip command-line plugin, which cannot report progress or
# cancel. The command-line plugins find their tools at run time: 7z, zip and
# unzip are depended on, while unrar, rar, arj and unar are not ports and their
# formats open through libarchive or not at all. Ark runs 7-Zip as `7z`, the
# name the 7zip port links to its 7zz, so 7z archives get the cli7z plugin's
# read and write support. KF_SKIP_PO_PROCESSING leaves the interface
# catalogues out: bundled data is English only. kdoctools_install() builds the
# translated handbooks too, and those are removed after the install.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D BUILD_DOC=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LibZip=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

if [ -d "$PKG/usr/share/doc/HTML" ]; then
	find "$PKG/usr/share/doc/HTML" -mindepth 1 -maxdepth 1 ! -name en -exec rm -rf {} +
fi

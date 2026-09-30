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

# Every codec library is optional upstream and silently skipped when absent.
# Those found through find_package are required here, so a missing one fails
# the configure rather than leaving Qt applications unable to open that type;
# HEIF and JPEG XL are pkg-config probes no flag can require. KArchive is the
# Krita and OpenRaster reader. HEIF is off by default upstream and named on. JPEG
# XR stays off: upstream keeps it behind a flag for known crashes.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KIMAGEFORMATS_DDS=ON \
	-D KIMAGEFORMATS_HEIF=ON \
	-D KIMAGEFORMATS_JXL=ON \
	-D KIMAGEFORMATS_JP2=ON \
	-D KIMAGEFORMATS_WITH_KNOWN_CRASHES_JXR=OFF \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Archive=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_libavif=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_OpenEXR=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_OpenJPEG=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LibRaw=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

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

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D RELEASE_BUILD=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6OpenGL=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Sql=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6WebEngineWidgets=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Imath=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_GSL=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Phonon4Qt6=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KChart6=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Eigen3=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Qca-qt6=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LCMS2=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Fontconfig=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Freetype=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_SharedMimeInfo=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Okular6=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LibRevenge=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LibRevengeStream=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LibOdfGen=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LibWpd=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LibWpg=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LibWps=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LibVisio=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LibEtonyek=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Poppler=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_PopplerXPDFHeaders=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Iconv=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_Cauchy=ON \
	-D CMAKE_DISABLE_FIND_PACKAGE_OOoSDK=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

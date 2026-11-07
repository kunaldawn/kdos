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

tar xf "$PORT_SRC/$name-vendor-$version.tar.xz" -C plugins/webengineurlinterceptor/adblock
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D OPTION_ADD_AUTOGENERATETEXT=OFF \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Mime=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KPim6PimCommonActivities=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_PlasmaActivities=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KPim6ImportWizard=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KPim6Itinerary=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Corrosion=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Discount=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

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

patch -p1 -i "$PORT_SRC/sandbox-policy.patch"
patch -p1 -i "$PORT_SRC/sandbox-badge.patch"
patch -p1 -i "$PORT_SRC/kdos-crt.patch"

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D KWIN_BUILD_X11=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_X11_XCB=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_PlasmaActivities=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Libeis-1.0=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_QAccessibilityClient6=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Python3=ON \
	-Wno-dev
for m in PipeWire libevdev; do
	grep -q "^${m}_FOUND:INTERNAL=1" build/CMakeCache.txt || {
		echo "kwin: pkg-config found no $m" >&2
		exit 1
	}
done
cmake --build build
DESTDIR=$PKG cmake --install build
rm -rf "$PKG/usr/lib/systemd"

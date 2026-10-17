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

patch -p1 -i "$PORT_SRC/vnc-link-gnutls.patch"

LDFLAGS="$LDFLAGS -lintl" \
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D WITH_FREERDP3=ON \
	-D WITH_NEWS=OFF \
	-D WITH_STATS=OFF \
	-D WITH_TIP=OFF \
	-D WITH_WWW=OFF \
	-D WITH_KF5WALLET=OFF \
	-D WITH_GVNC=OFF \
	-D WITH_X2GO=OFF \
	-D WITH_NX=OFF \
	-D WITH_XDMCP=OFF \
	-D WITH_ST=OFF \
	-D WITH_PYTHONLIBS=OFF \
	-D WITH_VTE=ON \
	-D WITH_MANPAGES=ON \
	-D WITH_TRANSLATIONS=OFF \
	-D WITH_KIOSK_SESSION=OFF \
	-D WITH_ICON_CACHE=OFF \
	-D WITH_UPDATE_DESKTOP_DB=OFF \
	-D HAVE_LIBAPPINDICATOR=ON \
	-D WITH_AVAHI=OFF \
	-D CMAKE_REQUIRE_FIND_PACKAGE_WinPR=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_FreeRDP=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_FreeRDP-Client=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_SharedMimeInfo=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LIBSSH=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_LIBVNCSERVER=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Spice=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Libsecret=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_VTE=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_GCRYPT=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_Cups=ON \
	-D VERSION=$version \
	-D prefix=/usr \
	-D exec_prefix=/usr \
	-D libdir=/usr/lib \
	-D includedir=/usr/include \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
grep -q "^includedir=/usr/include$" "$PKG/usr/lib/pkgconfig/remmina.pc"

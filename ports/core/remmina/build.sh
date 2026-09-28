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

# WITH_NEWS, WITH_STATS and WITH_TIP fetch news, send usage statistics and
# download tips from remmina.org. WITH_WWW is a web browser plugin.
# HAVE_LIBAPPINDICATOR=ON: the tray icon is an Ayatana AppIndicator, which
# the panel's tray hosts as a StatusNotifierItem.
# WITH_AVAHI=OFF: host discovery needs avahi-ui-gtk3, which the avahi port
# does not build; the switch, not CMAKE_DISABLE_FIND_PACKAGE, is what turns it
# off, because Remmina searches for every suggested package as REQUIRED.
# The RDP plugin is searched without REQUIRED and dropped silently when
# FreeRDP is missing, so its three packages are made required here.
# WITH_ICON_CACHE and WITH_UPDATE_DESKTOP_DB would write shared indexes into
# the package; kpkg regenerates them on install.
# -lintl: Remmina calls gettext, which musl leaves to libintl, and its link
# lines never name the library.
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
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

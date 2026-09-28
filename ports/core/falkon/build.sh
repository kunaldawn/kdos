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

# StartupWMClass follows setDesktopFileName("org.kde.falkon"), which is the
# Wayland app_id. The MimeType line goes: Firefox ESR's entry is the web
# handler, and a second claim would make the default whichever entry sorts
# first.
patch -p1 -i "$PORT_SRC/app-id.patch"

# The KDE Frameworks integration plugin (KWallet passwords, KIO schemes,
# Purpose sharing) is built only when all six frameworks are found, and is
# silently skipped otherwise, so each is required. The GNOME keyring plugin
# needs libgnome-keyring and the Python plugins need PySide6; neither is a
# port. The XCB link serves the X11 window-manager hints when Falkon runs
# under Xwayland. KF_SKIP_PO_PROCESSING leaves the interface catalogues out:
# bundled data is English only.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D BUILD_TESTING_ONLINE=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D BUILD_KEYRING=OFF \
	-D BUILD_PYTHON_SUPPORT=OFF \
	-D NO_X11=OFF \
	-D DISABLE_DBUS=OFF \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Wallet=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6KIO=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Crash=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6CoreAddons=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Purpose=ON \
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6JobWidgets=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

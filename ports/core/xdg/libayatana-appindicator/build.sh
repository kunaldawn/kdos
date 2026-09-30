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

# The indicator is exported as a StatusNotifierItem with its menu over
# dbusmenu, which the panel's tray hosts. The Mono bindings need a .NET
# runtime and are off; the gtk-doc reference is off.
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DFLAVOUR_GTK2=OFF \
	-DFLAVOUR_GTK3=ON \
	-DENABLE_BINDINGS_VALA=ON \
	-DENABLE_BINDINGS_MONO=OFF \
	-DENABLE_GTKDOC=OFF \
	-DENABLE_TESTS=OFF \
	-DENABLE_COVERAGE=OFF \
	-DENABLE_WERROR=OFF
ninja
DESTDIR=$PKG ninja install

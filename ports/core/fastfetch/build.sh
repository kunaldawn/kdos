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

# ENABLE_XRANDR=OFF and ENABLE_XCB_RANDR=OFF are the hard rule, not a size
# choice: they are the only switches that define FF_HAVE_XRANDR and
# FF_HAVE_XCB_RANDR, the two macros guarding the X display-server probes in
# src/detection/displayserver/linux/xlib.c and xcb.c. Turn either on and
# libX11 or libxcb joins the host. The Wayland backend reports the session.
mkdir -p build
cd build
cmake .. \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DENABLE_XCB_RANDR=OFF \
	-DENABLE_XRANDR=OFF \
	-DENABLE_WAYLAND=ON \
	-DENABLE_SYSTEM_YYJSON=OFF \
	-DBUILD_TESTS=OFF
make
make DESTDIR=$PKG install
cd ..

install -Dm644 $SRC/config.jsonc $PKG/etc/xdg/fastfetch/config.jsonc

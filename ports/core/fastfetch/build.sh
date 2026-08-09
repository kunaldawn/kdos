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

mkdir -p build
cd build
cmake .. \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DENABLE_X11=OFF \
	-DENABLE_XCB=OFF \
	-DENABLE_XCB_RANDR=OFF \
	-DENABLE_XRANDR=OFF \
	-DENABLE_WAYLAND=ON \
	-DENABLE_SYSTEM_YYJSON=OFF \
	-DBUILD_TESTS=OFF
make
make DESTDIR=$PKG install
cd ..

install -Dm644 $SRC/config.jsonc $PKG/etc/xdg/fastfetch/config.jsonc

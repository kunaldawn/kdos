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
	-DENABLE_XCB_RANDR=OFF \
	-DENABLE_XRANDR=OFF \
	-DENABLE_VAX11=OFF \
	-DENABLE_GLX=OFF \
	-DENABLE_WAYLAND=ON \
	-DENABLE_VULKAN=ON \
	-DENABLE_DRM=ON \
	-DENABLE_VADRM=ON \
	-DENABLE_GIO=ON \
	-DENABLE_DBUS=ON \
	-DENABLE_SQLITE3=ON \
	-DENABLE_EGL=ON \
	-DENABLE_IMAGEMAGICK7=ON \
	-DENABLE_CHAFA=ON \
	-DENABLE_ZLIB=ON \
	-DENABLE_PULSE=ON \
	-DENABLE_DDCUTIL=ON \
	-DENABLE_ELF=ON \
	-DENABLE_LUA=ON \
	-DLUA_INCLUDE_DIR=/usr/include/lua5.4 \
	-DLUA_LIBRARY=/usr/lib/liblua5.4.so \
	-DENABLE_VDPAU=OFF \
	-DENABLE_DCONF=OFF \
	-DENABLE_EET=OFF \
	-DENABLE_RPM=OFF \
	-DENABLE_IMAGEMAGICK6=OFF \
	-DENABLE_QUICKJS=OFF \
	-DENABLE_OPENCL=OFF \
	-DENABLE_SYSTEM_YYJSON=OFF \
	-DBUILD_TESTS=OFF
make
make DESTDIR=$PKG install
cd ..

install -Dm644 $SRC/config.jsonc $PKG/etc/xdg/fastfetch/config.jsonc

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
# ENABLE_VAX11 and ENABLE_GLX are the same rule for libva-x11 and glx.pc.
#
# Every other library is named ON or OFF, because an ON switch whose library is
# missing only prints "missing" and builds without it: each ON below has its
# port in depends. Nearly all of them are dlopen()ed at run time, so the build
# needs only headers and a .pc file. OFF are the ones with no port (vdpau,
# dconf, eet, rpm, ImageMagick 6, quickjs) and OpenCL, which has a loader port
# but no driver on the host to report.
#
# LUA IS PINNED TO lua54. fastfetch dlopen()s liblua5.<minor>.so, a name only
# lua54 installs; ports/core/lua names its library liblua.so, so a build that
# found lua 5.5's headers would carry scripting that can never load.
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

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

patch -p1 -i "$PORT_SRC/x11-include-order.patch"

export LDFLAGS="$LDFLAGS -fuse-ld=lld"
cmake -S . -B build -G Ninja \
	-DPORT=GTK \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_SKIP_RPATH=ON \
	-DUSE_GTK4=ON \
	-DENABLE_WAYLAND_TARGET=ON \
	-DENABLE_X11_TARGET=ON \
	-DENABLE_INTROSPECTION=ON \
	-DENABLE_DOCUMENTATION=OFF \
	-DENABLE_JOURNALD_LOG=OFF \
	-DENABLE_BUBBLEWRAP_SANDBOX=ON \
	-DBWRAP_EXECUTABLE=/usr/bin/bwrap \
	-DDBUS_PROXY_EXECUTABLE=/usr/bin/xdg-dbus-proxy \
	-DENABLE_MINIBROWSER=OFF \
	-DENABLE_WEBDRIVER=OFF \
	-DENABLE_API_TESTS=OFF \
	-DENABLE_GAMEPAD=OFF \
	-DENABLE_SPEECH_SYNTHESIS=OFF \
	-DENABLE_SPELLCHECK=ON \
	-DENABLE_VIDEO=ON \
	-DENABLE_WEB_AUDIO=ON \
	-DUSE_GSTREAMER=ON \
	-DUSE_GBM=ON \
	-DUSE_LIBDRM=ON \
	-DUSE_LIBHYPHEN=ON \
	-DUSE_LIBSECRET=ON \
	-DUSE_AVIF=ON \
	-DUSE_JPEGXL=ON \
	-DUSE_LCMS=ON \
	-DUSE_WOFF2=OFF \
	-DUSE_LIBBACKTRACE=OFF \
	-DUSE_SYSTEM_SYSPROF_CAPTURE=OFF \
	-DUSE_SYSTEM_UNIFDEF=ON \
	-DUSE_VULKAN=OFF \
	-DENABLE_SWIFT_DEMO_URI_SCHEME=OFF \
	-DENABLE_BACK_FORWARD_LIST_SWIFT=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

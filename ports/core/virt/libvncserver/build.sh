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

mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DLIBVNCSERVER_INSTALL=ON \
	-DWITH_ZLIB=ON \
	-DWITH_LZO=ON \
	-DWITH_JPEG=ON \
	-DWITH_PNG=ON \
	-DWITH_THREADS=ON \
	-DWITH_OPENSSL=ON \
	-DWITH_GNUTLS=OFF \
	-DWITH_GCRYPT=OFF \
	-DWITH_SASL=ON \
	-DWITH_WEBSOCKETS=ON \
	-DWITH_IPv6=ON \
	-DWITH_24BPP=ON \
	-DWITH_TIGHTVNC_FILETRANSFER=ON \
	-DWITH_SYSTEMD=OFF \
	-DWITH_SDL=OFF \
	-DWITH_GTK=OFF \
	-DWITH_QT=OFF \
	-DWITH_XCB=OFF \
	-DWITH_FFMPEG=OFF \
	-DWITH_LIBSSHTUNNEL=OFF \
	-DWITH_EXAMPLES=OFF \
	-DWITH_TESTS=OFF
ninja
DESTDIR=$PKG ninja install

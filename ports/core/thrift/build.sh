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

# The compiler and the C++ library, with the zlib and libevent transports and
# OpenSSL sockets. The C (GLib), Java, JavaScript, Node and Qt5 libraries are
# off; Qt5 would otherwise be picked up whenever it is installed. The CMake
# Python target only runs setup.py build and installs nothing, so the Python
# library is installed with pip from lib/py below.
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_TESTING=OFF \
	-DBUILD_TUTORIALS=OFF \
	-DBUILD_COMPILER=ON \
	-DBUILD_LIBRARIES=ON \
	-DWITH_CPP=ON \
	-DWITH_ZLIB=ON \
	-DWITH_LIBEVENT=ON \
	-DWITH_OPENSSL=ON \
	-DWITH_QT5=OFF \
	-DWITH_C_GLIB=OFF \
	-DWITH_JAVA=OFF \
	-DWITH_JAVASCRIPT=OFF \
	-DWITH_NODEJS=OFF \
	-DWITH_PYTHON=OFF \
	-Wno-dev
ninja -C build
DESTDIR=$PKG ninja -C build install

# The Python library carries a C++ accelerator for the binary and compact
# protocols; GNU Radio's ControlPort client imports it.
cd lib/py
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
cd ../..

test -x "$PKG/usr/bin/thrift"
test -f "$PKG/usr/lib/pkgconfig/thrift.pc"
test -n "$(find "$PKG/usr/lib" -path '*site-packages/thrift/protocol/fastbinary*.so' | head -1)"

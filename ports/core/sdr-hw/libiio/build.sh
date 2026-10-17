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

cmake -B build -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DWITH_LOCAL_BACKEND=ON \
	-DWITH_USB_BACKEND=ON \
	-DWITH_NETWORK_BACKEND=ON \
	-DHAVE_DNS_SD=ON \
	-DWITH_XML_BACKEND=ON \
	-DWITH_SERIAL_BACKEND=ON \
	-DWITH_IIOD=OFF \
	-DWITH_TESTS=ON \
	-DWITH_MAN=ON \
	-DWITH_DOC=OFF \
	-DWITH_EXAMPLES=OFF \
	-DPYTHON_BINDINGS=OFF \
	-DCSHARP_BINDINGS=OFF \
	-DCPP_BINDINGS=OFF \
	-DWITH_ZSTD=OFF \
	-DINSTALL_UDEV_RULE=OFF \
	-DCOMPILE_WARNING_AS_ERROR=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

mv "$PKG"/usr/share/man/man1/man/*.1 "$PKG"/usr/share/man/man1/
mv "$PKG"/usr/share/man/man3/man/*.3 "$PKG"/usr/share/man/man3/
rmdir "$PKG"/usr/share/man/man1/man "$PKG"/usr/share/man/man3/man

test -e "$PKG"/usr/lib/pkgconfig/libiio.pc
test -x "$PKG"/usr/bin/iio_info
test -e "$PKG"/usr/share/man/man1/iio_info.1

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

# The 0.x line: GNU Radio 3.10's gr-iio and libad9361 0.3 are written against
# its buffer API (iio_device_create_buffer, iio_buffer_refill), which 1.0
# removed.
#
# Local, USB, network (with Avahi discovery) and serial backends, and the
# iio_info, iio_attr, iio_readdev and iio_writedev tools with their manual
# pages. iiod is the daemon that runs on the radio's own processor and is not
# built for a desktop. INSTALL_UDEV_RULE=OFF because upstream's rule runs
# iio_info for every USB device plugged in and opens the matches to everyone;
# fs/etc/udev/rules.d/70-kdos-sdr.rules grants the Pluto's id to dialout.
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
	-DWITH_SYSTEMD=OFF \
	-DWITH_SYSVINIT=OFF \
	-DWITH_UPSTART=OFF \
	-DCOMPILE_WARNING_AS_ERROR=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

# The man install copies the build's share/man directory into man1 and man3
# whole, so each page lands one level down, in man1/man and man3/man, where
# man never looks.
mv "$PKG"/usr/share/man/man1/man/*.1 "$PKG"/usr/share/man/man1/
mv "$PKG"/usr/share/man/man3/man/*.3 "$PKG"/usr/share/man/man3/
rmdir "$PKG"/usr/share/man/man1/man "$PKG"/usr/share/man/man3/man

test -e "$PKG"/usr/lib/pkgconfig/libiio.pc
test -x "$PKG"/usr/bin/iio_info
test -e "$PKG"/usr/share/man/man1/iio_info.1

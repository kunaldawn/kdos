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

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--localstatedir=/var \
	--disable-static \
	--disable-doxygen-doc \
	--enable-fapi \
	--enable-policy \
	--enable-tcti-spidev \
	--enable-tcti-spi-ltt2go \
	--enable-tcti-spi-ftdi \
	--enable-tcti-i2c-ftdi \
	--enable-tcti-libtpms \
	--with-udevrulesdir=/usr/lib/udev/rules.d \
	--with-runstatedir=/run \
	--with-sysusersdir=no \
	--with-tmpfilesdir=no
make
make DESTDIR=$PKG install
rm -rf "$PKG/run"

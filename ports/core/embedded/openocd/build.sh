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

./configure \
	--prefix=/usr \
	--disable-werror \
	--enable-ftdi \
	--enable-stlink \
	--enable-ti-icdi \
	--enable-ulink \
	--enable-usb-blaster-2 \
	--enable-ft232r \
	--enable-vsllink \
	--enable-xds110 \
	--enable-cmsis-dap-v2 \
	--enable-osbdm \
	--enable-opendous \
	--enable-armjtagew \
	--enable-rlink \
	--enable-usbprog \
	--enable-esp-usb-jtag \
	--enable-cmsis-dap \
	--enable-nulink \
	--enable-kitprog \
	--enable-usb-blaster \
	--enable-presto \
	--enable-openjtag \
	--enable-buspirate \
	--enable-jlink \
	--disable-internal-libjaylink \
	--disable-linuxgpiod \
	--enable-jtag_vpi \
	--enable-remote-bitbang \
	--with-capstone \
	--disable-doxygen-html
make
make DESTDIR=$PKG install

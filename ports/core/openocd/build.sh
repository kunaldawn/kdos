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

# THIS IS THE gdb SERVER FOR A CHIP WITH NO OPERATING SYSTEM. Everything else
# on this machine debugs a process; openocd halts a microcontroller through its
# debug port, reads its registers and single-steps it — which is the only way
# to find out why a board does not boot far enough to print anything.
#
# EVERY ADAPTER IS NAMED, BECAUSE configure DEFAULTS EACH ONE TO auto: a
# driver whose library is missing is dropped without a word, so the build
# would be narrower on a tree that happened to install libftdi or hidapi
# late. Naming them turns a missing library into a configure error.
# fs/etc/udev/rules.d/70-kdos-debug.rules and 70-kdos-serial.rules grant the
# dialout group every adapter enabled here, by the ids upstream's
# contrib/60-openocd.rules lists. That file is never a rule here: it grants
# a plugdev group and a uaccess tag, and neither exists on this system. An
# adapter missing from those two files is one only root can open, and a line
# for its id there is what lets a user open it.
#
# J-LINK LINKS THE libjaylink PORT. --disable-internal-libjaylink is what
# makes it: the release tarball carries its own copy, and configure builds
# that one unless told not to.
#
# --with-capstone is what gives `arm disassemble` a disassembler; configure
# otherwise probes for it and leaves the command out when it is missing.
#
# --disable-linuxgpiod: this release's driver is written against libgpiod's
# v1 API and the libgpiod port is v2, so leaving it to auto-detection breaks
# the build whenever libgpiod is installed first. The switch is missing
# from --help, because the adapter list that should declare it lacks a
# comma, but the variable it sets is the one the driver test reads.
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

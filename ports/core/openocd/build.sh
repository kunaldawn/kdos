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
# EVERY ADAPTER THE udev RULES ALREADY GRANT IS ENABLED: CMSIS-DAP, ST-Link,
# the FTDI bit-bang adapters and the RP2040's native USB. That list and
# fs/etc/udev/rules.d/70-kdos-*.rules are the same list, and it is deliberate —
# a driver compiled in with no rule to open the device is a probe that is
# present, enumerated and unusable.
#
# --enable-jlink IS ABSENT AND THE COST IS STATED: it needs libjaylink, which
# is not a port, so a SEGGER J-Link is not driven by openocd here. The udev
# rule for one still ships because `JLinkExe` and pyocd can use it, and the
# flag returns the day libjaylink lands.
./configure \
	--prefix=/usr \
	--disable-werror \
	--enable-ftdi \
	--enable-stlink \
	--enable-cmsis-dap \
	--enable-cmsis-dap-v2 \
	--enable-picoprobe \
	--enable-ti-icdi \
	--enable-ulink \
	--enable-buspirate \
	--enable-jtag_vpi \
	--enable-remote-bitbang \
	--disable-doxygen-html
make
make DESTDIR=$PKG install

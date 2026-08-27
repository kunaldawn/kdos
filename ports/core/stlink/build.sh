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

# openocd DRIVES AN ST-LINK TOO, AND THIS IS STILL WORTH HAVING: `st-flash
# write firmware.bin 0x8000000` is one command against one board, where the
# same operation through openocd is a config file, an interface script and a
# target script. On a bench the short path is the one that gets used.
#
# The ST-Link is on every Nucleo and Discovery board — the probe is soldered to
# the same PCB as the chip — so this is the flashing route for the most common
# development hardware there is.
# Upstream compiles with -Werror, which is a promise about the compilers and
# the libc upstream builds against and not about these. Two diagnostics it has
# never seen fire here: GCC 14's -Wcalloc-transposed-args on chipid.c, where
# `calloc(sizeof(x), n)` is the transposed spelling and is correct; and musl's
# own `#warning` in <sys/poll.h>, which -Werror=cpp makes fatal in a header
# this code does not include directly. -Wno-error keeps every warning printed
# and stops upstream deciding which of them ends this build.
#
# IT GOES IN CMAKE_C_FLAGS_RELEASE, NOT IN CFLAGS. c_flags.cmake APPENDS
# -Werror to CMAKE_C_FLAGS, which is where the environment's CFLAGS already
# landed — so a -Wno-error from there is overridden by the very flag it is
# answering. The per-config variable is emitted after both.

mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG -Wno-error" \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DSTLINK_MODPROBED_DIR=/etc/modprobe.d \
	-DSTLINK_UDEV_RULES_DIR=/etc/udev/rules.d
ninja
DESTDIR=$PKG ninja install

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

# EVERY BINDING AND EVERY EXAMPLE IS OFF. libftdi builds python and C++
# wrappers and a small set of demo programs; what this tree needs is the C
# library that iceprog, openFPGALoader and half the bench tools link.
#
# The kernel's own ftdi_sio driver claims these chips as tty devices, which is
# right for a serial console (picocom) and wrong for bit-banged JTAG — the
# programmer detaches it and reattaches on exit. That is why both routes exist
# and why the same cable is a `/dev/ttyUSB0` one minute and a JTAG adapter the
# next.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DPYTHON_BINDINGS=OFF \
	-DFTDIPP=OFF \
	-DBUILD_TESTS=OFF \
	-DEXAMPLES=OFF \
	-DDOCUMENTATION=OFF \
	-DFTDI_EEPROM=OFF
ninja
DESTDIR=$PKG ninja install

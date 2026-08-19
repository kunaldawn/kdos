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

# FTDIPP is the C++ wrapper and PYTHON_BINDINGS needs swig. Nothing here uses
# either: openocd, openFPGALoader, flashrom and avrdude all take the C API.
#
# FTDI_EEPROM defaults ON and pulls in libConfuse to parse its .conf files.
# That is a config-file parser carried for one utility that rewrites the
# VID/PID and serial in an FTDI chip's own EEPROM — nothing else here would
# link it. Turning the tool off is what keeps this port to the library its
# consumers ask for; adding a libconfuse port is what turning it back on costs.
#
# STATICLIBS is left ON: it is upstream's default and the archive is small.
mkdir -p build && cd build
cmake .. \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_TESTS=OFF \
	-DEXAMPLES=OFF \
	-DFTDIPP=OFF \
	-DFTDI_EEPROM=OFF \
	-DPYTHON_BINDINGS=OFF \
	-DDOCUMENTATION=OFF
make
make DESTDIR=$PKG install

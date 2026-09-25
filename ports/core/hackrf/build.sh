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

# Only host/ is built. The tarball's firmware-bin/ holds the prebuilt images
# for the board's own microcontroller and is not installed: flashing one is a
# decision taken with hackrf_spiflash and a file chosen for the board.
#
# INSTALL_UDEV_RULES=OFF because 53-hackrf.rules grants a plugdev group this
# system does not have; fs/etc/udev/rules.d/70-kdos-sdr.rules grants the
# vendor id to dialout. hackrf_sweep links fftw's single-precision library, and
# ENABLE_HACKRF_SWEEP=ON with fftw in depends keeps it from disappearing when
# fftw is absent.
cd host
mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DINSTALL_UDEV_RULES=OFF \
	-DENABLE_STATIC_LIB=OFF \
	-DENABLE_HACKRF_SWEEP=ON
make
make DESTDIR=$PKG install

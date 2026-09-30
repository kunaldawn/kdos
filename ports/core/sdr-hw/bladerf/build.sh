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

# The release archive carries its two submodules empty, so each is a source of
# its own at the commit the 2025.10 tag pins, unpacked beside the tree and
# linked where the build looks. no-OS is Analog Devices' AD9361 driver, which
# libbladeRF compiles in for the bladeRF 2.0's RFIC after applying Nuand's
# patches to a copy in the build directory. bladeRF-fsk must exist because
# host/utilities stops configure without it, and runs `git submodule update`
# when git is on the path — a network fetch; the modem demo it holds is not
# built.
rmdir thirdparty/analogdevicesinc/no-OS host/utilities/bladeRF-fsk
ln -s "$SRC_ROOT/no-OS-$_noos" thirdparty/analogdevicesinc/no-OS
ln -s "$SRC_ROOT/bladeRF-fsk-$_fsk" host/utilities/bladeRF-fsk

# The FPGA bitstream and the FX3 firmware are the board's own code and are
# not in the tarball: bladeRF-cli loads a bitstream from ~/.config/Nuand/bladeRF
# or /etc/Nuand/bladeRF, and a bladeRF 2.0 can autoload one from its flash.
#
# INSTALL_UDEV_RULES=OFF because 88-nuand-bladerf*.rules grant plugdev;
# fs/etc/udev/rules.d/70-kdos-sdr.rules grants the vendor id to dialout.
# TREAT_WARNINGS_AS_ERRORS=OFF: the default puts -Werror on every target, so a
# warning a newer gcc adds stops the build. VERSION_INFO_OVERRIDE fixes the
# version suffix, so whatever git says about a build tree that is not a
# checkout never reaches the library's version string. BUILD_DOCUMENTATION turns on bladeRF-cli's help2man page;
# the libbladeRF API manual under it needs doxygen and is off.
mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DINSTALL_UDEV_RULES=OFF \
	-DTREAT_WARNINGS_AS_ERRORS=OFF \
	-DVERSION_INFO_OVERRIDE=$version \
	-DBUILD_BLADERF_FSK=OFF \
	-DBUILD_DOCUMENTATION=ON \
	-DBUILD_LIBBLADERF_DOCUMENTATION=OFF \
	-DENABLE_LIBEDIT=ON \
	-DENABLE_LIBTECLA=OFF
make
make DESTDIR=$PKG install

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

rmdir thirdparty/analogdevicesinc/no-OS host/utilities/bladeRF-fsk
ln -s "$SRC_ROOT/no-OS-$_noos" thirdparty/analogdevicesinc/no-OS
ln -s "$SRC_ROOT/bladeRF-fsk-$_fsk" host/utilities/bladeRF-fsk

mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DINSTALL_UDEV_RULES=OFF \
	-DTREAT_WARNINGS_AS_ERRORS=OFF \
	-DVERSION_INFO_OVERRIDE=$version \
	-DLIBBLADERF_SEARCH_PREFIX_OVERRIDE=/ \
	-DBUILD_BLADERF_FSK=OFF \
	-DBUILD_DOCUMENTATION=ON \
	-DBUILD_LIBBLADERF_DOCUMENTATION=OFF \
	-DENABLE_LIBEDIT=ON \
	-DENABLE_LIBTECLA=OFF
make
make DESTDIR=$PKG install

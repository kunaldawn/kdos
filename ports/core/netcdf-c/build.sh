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

mkdir -p build && cd build
# NCZarr and the DAP remote protocols are network clients; on a distro that
# builds offline they are the half of netcdf nobody here can reach, and each
# drags in another dependency to answer a URL.
cmake .. -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DNETCDF_ENABLE_DAP=OFF -DNETCDF_ENABLE_NCZARR=OFF -DNETCDF_ENABLE_TESTS=OFF \
	-DNETCDF_PLUGIN_INSTALL=OFF -DNETCDF_BUILD_UTILITIES=ON
make
make DESTDIR=$PKG install

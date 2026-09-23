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
# Every remote and codec feature below is found by a find_package() that turns
# the feature off in silence when its library is missing, so each is spelled ON
# and its library is in depends: DAP, byte-range reads and the internal S3
# client need curl (S3 also openssl), NCZarr zip stores need libzip, and the
# szip, zstd and bzip2 filters need libaec, zstd and bzip2 — without bzip2 a
# bundled copy is compiled in instead. blosc stays off: c-blosc is not a port.
#
# The HDF5 filters are plugins loaded at run time, so a zstd- or bzip2-
# compressed netCDF-4 variable is unreadable unless they are installed.
# NETCDF_WITH_PLUGIN_DIR is both the install directory and the head of
# libnetcdf's own search path; hdf5's compiled-in default is under /usr/local.
cmake .. -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DNETCDF_ENABLE_DAP=ON -DNETCDF_ENABLE_BYTERANGE=ON \
	-DNETCDF_ENABLE_NCZARR=ON -DNETCDF_ENABLE_NCZARR_ZIP=ON \
	-DNETCDF_ENABLE_S3=ON -DNETCDF_ENABLE_S3_INTERNAL=ON -DNETCDF_ENABLE_S3_AWS=OFF \
	-DNETCDF_ENABLE_LIBXML2=ON \
	-DNETCDF_ENABLE_FILTER_SZIP=ON -DNETCDF_ENABLE_FILTER_ZSTD=ON \
	-DNETCDF_ENABLE_FILTER_BZ2=ON -DNETCDF_ENABLE_FILTER_BLOSC=OFF \
	-DNETCDF_ENABLE_HDF4=OFF -DNETCDF_ENABLE_PNETCDF=OFF \
	-DNETCDF_PLUGIN_INSTALL=ON -DNETCDF_WITH_PLUGIN_DIR=/usr/lib/hdf5/plugin \
	-DNETCDF_ENABLE_TESTS=OFF -DNETCDF_ENABLE_EXAMPLES=OFF \
	-DNETCDF_BUILD_UTILITIES=ON
make
make DESTDIR=$PKG install

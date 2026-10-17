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
cmake .. -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DNETCDF_ENABLE_DAP=ON -DNETCDF_ENABLE_BYTERANGE=ON \
	-DNETCDF_ENABLE_NCZARR=ON -DNETCDF_ENABLE_NCZARR_ZIP=ON \
	-DNETCDF_ENABLE_S3=ON -DNETCDF_ENABLE_S3_INTERNAL=ON -DNETCDF_ENABLE_S3_AWS=OFF \
	-DNETCDF_ENABLE_LIBXML2=ON \
	-DNETCDF_ENABLE_FILTER_SZIP=ON -DNETCDF_ENABLE_FILTER_ZSTD=ON \
	-DNETCDF_ENABLE_FILTER_BZ2=ON -DNETCDF_ENABLE_FILTER_BLOSC=ON \
	-DNETCDF_ENABLE_HDF4=OFF -DNETCDF_ENABLE_PNETCDF=OFF \
	-DNETCDF_PLUGIN_INSTALL=ON -DNETCDF_WITH_PLUGIN_DIR=/usr/lib/hdf5/plugin \
	-DNETCDF_ENABLE_TESTS=OFF -DNETCDF_ENABLE_EXAMPLES=OFF \
	-DNETCDF_BUILD_UTILITIES=ON
make
make DESTDIR=$PKG install
test -e "$PKG/usr/lib/hdf5/plugin/lib__nch5blosc.so"

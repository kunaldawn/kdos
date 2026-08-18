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

# Four codecs, one pair per format:
#   libde265 decodes HEIC   x265 encodes it
#   dav1d    decodes AVIF   svt-av1 encodes it
#
# AOM is deliberately absent: svt-av1 is this tree's AV1 encoder, and a second
# one earns nothing.
mkdir -p build && cd build
cmake .. \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_TESTING=OFF \
	-DWITH_EXAMPLES=ON \
	-DWITH_LIBDE265=ON \
	-DWITH_X265=ON \
	-DWITH_DAV1D=ON \
	-DWITH_SvtEnc=ON \
	-DWITH_AOM_DECODER=OFF \
	-DWITH_AOM_ENCODER=OFF
make
make DESTDIR=$PKG install

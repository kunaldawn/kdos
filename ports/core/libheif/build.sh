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

# One decoder and one encoder per format:
#   libde265      decodes HEIC     x265     encodes it
#   dav1d         decodes AVIF     svt-av1  encodes it
#   libjpeg-turbo decodes and encodes JPEG-in-HEIF
#   openjpeg      decodes and encodes JPEG 2000 in HEIF (HEJ2)
# The uncompressed codec (ISO 23001-17) and header compression are internal,
# and handle their deflate and brotli variants through zlib and brotli.
#
# AOM is deliberately absent: svt-av1 is this tree's AV1 encoder, and a second
# one earns nothing.
#
# EVERY WITH_ IS NAMED so the codec set is written here, not inherited from
# upstream defaults. Each is a find_package that drops its codec without an
# error when the library is missing, so every one of them is in depends.
# heif-view is off because it builds only when SDL2 happens to be installed,
# and the doxygen reference because it builds only when doxygen does.
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
	-DWITH_AOM_ENCODER=OFF \
	-DWITH_X264=OFF \
	-DWITH_OpenH264_DECODER=OFF \
	-DWITH_JPEG_DECODER=ON \
	-DWITH_JPEG_ENCODER=ON \
	-DWITH_OpenJPEG_DECODER=ON \
	-DWITH_OpenJPEG_ENCODER=ON \
	-DWITH_UNCOMPRESSED_CODEC=ON \
	-DWITH_HEADER_COMPRESSION=ON \
	-DWITH_LIBSHARPYUV=ON \
	-DWITH_GDK_PIXBUF=ON \
	-DWITH_EXAMPLE_HEIF_VIEW=OFF \
	-DBUILD_DOCUMENTATION=OFF
make
make DESTDIR=$PKG install

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
	-DWITH_OpenH264_DECODER=ON \
	-DWITH_JPEG_DECODER=ON \
	-DWITH_JPEG_ENCODER=ON \
	-DWITH_OpenJPEG_DECODER=ON \
	-DWITH_OpenJPEG_ENCODER=ON \
	-DWITH_RAV1E=OFF \
	-DWITH_KVAZAAR=OFF \
	-DWITH_UVG266=OFF \
	-DWITH_VVDEC=OFF \
	-DWITH_VVENC=OFF \
	-DWITH_OPENJPH_ENCODER=OFF \
	-DWITH_FFMPEG_DECODER=OFF \
	-DWITH_WEBCODECS=OFF \
	-DWITH_UNCOMPRESSED_CODEC=ON \
	-DWITH_HEADER_COMPRESSION=ON \
	-DWITH_LIBSHARPYUV=ON \
	-DWITH_GDK_PIXBUF=ON \
	-DWITH_EXAMPLE_HEIF_THUMB=ON \
	-DWITH_EXAMPLE_HEIF_VIEW=OFF \
	-DBUILD_DOCUMENTATION=OFF
make
make DESTDIR=$PKG install

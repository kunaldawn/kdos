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
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DFETCHCONTENT_FULLY_DISCONNECTED=ON \
	-DFETCHCONTENT_SOURCE_DIR_LIBARGPARSE="$SRC_ROOT/libargparse-$_argparse" \
	-DAVIF_CODEC_DAV1D=SYSTEM \
	-DAVIF_CODEC_SVT=SYSTEM \
	-DAVIF_CODEC_AOM=OFF \
	-DAVIF_CODEC_LIBGAV1=OFF \
	-DAVIF_CODEC_RAV1E=OFF \
	-DAVIF_CODEC_AVM=OFF \
	-DAVIF_LIBYUV=SYSTEM \
	-DAVIF_LIBSHARPYUV=SYSTEM \
	-DAVIF_LIBXML2=SYSTEM \
	-DAVIF_JPEG=SYSTEM \
	-DAVIF_ZLIBPNG=SYSTEM \
	-DAVIF_GTEST=OFF \
	-DAVIF_FUZZTEST=OFF \
	-DAVIF_BUILD_APPS=ON \
	-DAVIF_BUILD_TESTS=OFF \
	-DAVIF_BUILD_EXAMPLES=OFF \
	-DAVIF_BUILD_GDK_PIXBUF=ON \
	-DAVIF_BUILD_MAN_PAGES=OFF
make
make DESTDIR=$PKG install

install -d "$PKG/usr/share/man/man1"
for page in avifenc avifdec; do
	lowdown -s -Tman \
		-M title=${page^^} -M section=1 -M source="libavif $version" \
		-o "$PKG/usr/share/man/man1/$page.1" ../doc/$page.1.md
done

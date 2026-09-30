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

# Two consumers: imagemagick's --with-openjp2, and poppler, which without it
# draws nothing where a JPEG 2000 image inside a PDF should be.
#
# THE CODEC TOOLS' IMAGE FORMATS ARE REQUIRED, NOT PROBED. opj_compress and
# opj_decompress read and write PNG and TIFF and apply ICC profiles through
# lcms2, and each library is looked up with a find_package that quietly
# drops the format when it is missing. CMAKE_REQUIRE_FIND_PACKAGE_* turns a
# missing one into a configure error; BUILD_THIRDPARTY=OFF keeps the bundled
# copies of those libraries out.
mkdir -p build && cd build
cmake .. \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_STATIC_LIBS=OFF \
	-DBUILD_TESTING=OFF \
	-DBUILD_CODEC=ON \
	-DBUILD_THIRDPARTY=OFF \
	-DCMAKE_REQUIRE_FIND_PACKAGE_ZLIB=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_PNG=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_TIFF=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_LCMS2=ON
make
make DESTDIR=$PKG install

install -Dm644 ../doc/man/man1/*.1 -t $PKG/usr/share/man/man1
install -Dm644 ../doc/man/man3/*.3 -t $PKG/usr/share/man/man3

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

patch -p1 -i "$PORT_SRC/dont-export-executables.patch"

patch -p1 -i "$PORT_SRC/openssl4.patch"

cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_INSTALL_SYSCONFDIR=/etc \
	-DBUILD_SHARED_LIBS=ON \
	-DDCMTK_ENABLE_LFS=lfs \
	-DDCMTK_ENABLE_STL=ON \
	-DDCMTK_ENABLE_PRIVATE_TAGS=ON \
	-DDCMTK_ENABLE_MANPAGES=ON \
	-DDCMTK_ENABLE_CHARSET_CONVERSION=oficonv \
	-DDCMTK_WITH_OPENSSL=ON \
	-DDCMTK_WITH_OPENJPEG=ON \
	-DDCMTK_WITH_PNG=ON \
	-DDCMTK_WITH_TIFF=ON \
	-DDCMTK_WITH_XML=ON \
	-DDCMTK_WITH_ZLIB=ON \
	-DDCMTK_WITH_THREADS=ON \
	-DDCMTK_WITH_ICONV=OFF \
	-DDCMTK_WITH_SNDFILE=OFF \
	-DDCMTK_WITH_WRAP=OFF \
	-DDCMTK_WITH_DOXYGEN=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

rm -rf "$PKG/usr/share/doc"
rm -f "$PKG/usr/share/dcmtk-$version/SC.dump" "$PKG/usr/share/dcmtk-$version/VLP.dump"

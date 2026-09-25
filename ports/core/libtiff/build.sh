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

# Every codec option defaults to whether its library was found, and naming it ON
# does not change that: the option is honoured only when the find succeeded. The
# CMAKE_REQUIRE_FIND_PACKAGE_* switches are what turn a missing codec library
# into a configure error instead of a libtiff that quietly reads fewer files.
#
# webp stays OFF because libwebp depends on libtiff for its tools, and a codec
# here would close that into a cycle. jbig and lerc have no port; tiff-opengl
# needs GLUT for the tiffgt toy viewer; sphinx would rebuild the manual that
# doc/man-prebuilt already ships. Each is named so a library on the build host
# cannot switch it on.
cmake -S . -B build -G Ninja \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_INSTALL_LIBDIR=lib \
		-DCMAKE_INSTALL_LIBEXECDIR=lib \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_C_FLAGS_RELEASE="$CFLAGS" \
		-DCMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS" \
		-DCMAKE_INSTALL_DOCDIR=/usr/share/doc/libtiff \
		-Dzlib=ON -DCMAKE_REQUIRE_FIND_PACKAGE_ZLIB=ON \
		-Dpixarlog=ON \
		-Dlibdeflate=ON -DCMAKE_REQUIRE_FIND_PACKAGE_Deflate=ON \
		-Djpeg=ON -Dold-jpeg=ON -DCMAKE_REQUIRE_FIND_PACKAGE_libjpeg-turbo=ON \
		-Dlzma=ON -DCMAKE_REQUIRE_FIND_PACKAGE_liblzma=ON \
		-Dzstd=ON -DCMAKE_REQUIRE_FIND_PACKAGE_ZSTD=ON \
		-Dwebp=OFF \
		-Djbig=OFF \
		-Dlerc=OFF \
		-Dtiff-opengl=OFF \
		-Dsphinx=OFF \
		-Dtiff-tests=OFF \
		-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
for m in doc/man-prebuilt/*.1; do
	b=${m##*/}
	if [ -e "$PKG/usr/bin/${b%.1}" ]; then
		install -Dm644 "$m" -t "$PKG/usr/share/man/man1"
	fi
done
install -Dm644 doc/man-prebuilt/*.3tiff -t "$PKG/usr/share/man/man3"

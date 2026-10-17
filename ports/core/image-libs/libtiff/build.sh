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

cmake -S . -B build -G Ninja \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DCMAKE_INSTALL_LIBDIR=lib \
		-DCMAKE_INSTALL_LIBEXECDIR=lib \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_C_FLAGS_RELEASE="$CFLAGS -DNDEBUG" \
		-DCMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS -DNDEBUG" \
		-DCMAKE_INSTALL_DOCDIR=/usr/share/doc/libtiff \
		-Dzlib=ON -DCMAKE_REQUIRE_FIND_PACKAGE_ZLIB=ON \
		-Dpixarlog=ON \
		-Dlibdeflate=ON -DCMAKE_REQUIRE_FIND_PACKAGE_Deflate=ON \
		-Djpeg=ON -Dold-jpeg=ON -DCMAKE_REQUIRE_FIND_PACKAGE_libjpeg-turbo=ON \
		-Dlzma=ON -DCMAKE_REQUIRE_FIND_PACKAGE_liblzma=ON \
		-Dzstd=ON -DCMAKE_REQUIRE_FIND_PACKAGE_ZSTD=ON \
		-Dwebp=OFF \
		-Djbig=ON -DCMAKE_REQUIRE_FIND_PACKAGE_JBIG=ON \
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

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

export CXXFLAGS="$CXXFLAGS -ffat-lto-objects -include cstdint"

cmake -B build-shared -G Ninja \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_INSTALL_LIBEXECDIR=lib \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_C_FLAGS_RELEASE="$CFLAGS" \
	-DCMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS" \
	-DBUILD_SHARED_LIBS=ON \
	-DALLOW_EXTERNAL_SPIRV_TOOLS=ON \
	-Wno-dev
ninja -C build-shared

cmake -B build-static -G Ninja \
	-DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_INSTALL_LIBEXECDIR=lib \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_C_FLAGS_RELEASE="$CFLAGS" \
	-DCMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS" \
	-DBUILD_SHARED_LIBS=OFF \
	-DALLOW_EXTERNAL_SPIRV_TOOLS=ON \
	-Wno-dev
ninja -C build-static

DESTDIR=$PKG ninja -C build-shared install
DESTDIR=$PKG ninja -C build-static install

cd $PKG/usr/lib
for lib in *.so; do
	ln -sf "${lib}" "${lib}.0"
done

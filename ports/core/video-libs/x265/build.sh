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

export CXXFLAGS="$CXXFLAGS -include cstdint -frandom-seed=x265"
export CFLAGS="$CFLAGS -frandom-seed=x265"
common=(
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5
	-DCMAKE_BUILD_TYPE=Release
	-DCMAKE_INSTALL_PREFIX=/usr
	-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
	-DENABLE_ASSEMBLY=ON
	-DENABLE_HDR10_PLUS=ON
	-DENABLE_LIBNUMA=OFF
	-DENABLE_TESTS=OFF
)

mkdir -p build-12 build-10 build-8

cd build-12
cmake ../source "${common[@]}" \
	-DHIGH_BIT_DEPTH=ON -DMAIN12=ON \
	-DEXPORT_C_API=OFF -DENABLE_SHARED=OFF -DENABLE_CLI=OFF
make
cd ..

cd build-10
cmake ../source "${common[@]}" \
	-DHIGH_BIT_DEPTH=ON \
	-DEXPORT_C_API=OFF -DENABLE_SHARED=OFF -DENABLE_CLI=OFF
make
cd ..

cd build-8
ln -sf ../build-10/libx265.a libx265_main10.a
ln -sf ../build-12/libx265.a libx265_main12.a
cmake ../source "${common[@]}" \
	-DENABLE_SHARED=ON \
	-DENABLE_CLI=ON \
	-DEXTRA_LIB="x265_main10.a;x265_main12.a" \
	-DEXTRA_LINK_FLAGS=-L. \
	-DLINKED_10BIT=ON -DLINKED_12BIT=ON
make
make DESTDIR=$PKG install
cd ..

rm -f "$PKG/usr/lib/libx265.a" "$PKG/usr/lib/libhdr10plus.a"

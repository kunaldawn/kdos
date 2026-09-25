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

# GPL-2+, same as x264 — see ports/core/x264/build.sh for what that does to the
# shipped ffmpeg.
#
# ONE libx265.so CARRIES ALL THREE DEPTHS. Main10 is what HDR and most
# consumer HEVC archives are, and an 8-bit-only library makes ffmpeg's libx265
# refuse every 10-bit pixel format. x265 builds a depth per configure, so the
# 12- and 10-bit encoders are built first as static archives with no C API of
# their own, and the 8-bit build links both in behind LINKED_10BIT/12BIT; the
# public API then dispatches on the requested depth. This is upstream's
# build/linux/multilib.sh, with the shared library as the product.
#
# HDR10+ is on in all three: its dynamic metadata is a 10-bit feature, and the
# depth that encodes it is the one that has to be compiled with it. Its
# vendored json11 uses uint8_t without including <cstdint>, and none of the
# libstdc++ headers it does include pulls that in, so every depth fails to
# compile unless the header is forced.
#
# libnuma is OFF: it is not a port, the probe is automatic, and it only pays
# on multi-socket machines.
export CXXFLAGS="$CXXFLAGS -include cstdint"
common=(
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5
	-DCMAKE_BUILD_TYPE=Release
	-DCMAKE_INSTALL_PREFIX=/usr
	-DCMAKE_INSTALL_LIBDIR=lib
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

# Nothing here links x265 or the HDR10+ parser statically.
rm -f "$PKG/usr/lib/libx265.a" "$PKG/usr/lib/libhdr10plus.a"

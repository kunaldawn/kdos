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

rmdir external/flatbuffers
mv "$SRC_ROOT/flatbuffers-$_fb" external/flatbuffers

mkdir -p external/json/include/nlohmann

export CXXFLAGS="$CXXFLAGS -DFLATBUFFERS_LOCALE_INDEPENDENT=0"

cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DENABLE_EXAMPLES=OFF \
	-DUSE_SYSTEM_JSON=ON \
	-DUSE_SYSTEM_FLATBUFFERS=OFF \
	-DFLATBUFFERS_BUILD_TESTS=OFF \
	-DFLATBUFFERS_INSTALL=OFF \
	-DFLATBUFFERS_BUILD_FLATLIB=OFF \
	-DFLATBUFFERS_BUILD_FLATHASH=OFF \
	-DFLATBUFFERS_BUILD_FLATC=ON
ninja -C build
DESTDIR=$PKG ninja -C build install

test -f "$PKG/usr/lib/liblibsigmf.so"
test -f "$PKG/usr/include/libsigmf/sigmf_sdrangel_generated.h"
test -f "$PKG/usr/include/flatbuffers/flatbuffers.h"
test ! -e "$PKG/usr/bin/flatc"

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

# The new-namespaces branch is the one SDRangel's SigMF file input and sink
# build against: it adds the sdrangel namespace. Its two submodules arrive
# empty in a forge archive. JSON comes from the nlohmann-json port.
# FlatBuffers is carried as a second source and put where the submodule
# was: the build compiles flatc from it to generate the protocol headers,
# and those headers need the FlatBuffers headers of the same release, which
# are installed beside them. Nothing else of FlatBuffers is installed.
rmdir external/flatbuffers
mv "$SRC_ROOT/flatbuffers-$_fb" external/flatbuffers

# The install copies external/json/include/nlohmann whether or not the
# system copy is used; an empty directory makes that copy a no-op instead
# of an error.
mkdir -p external/json/include/nlohmann

# FlatBuffers takes _XOPEN_VERSION 700 to mean strtoll_l and strtoull_l
# exist; musl has neither. A consumer that includes minireflect.h or util.h
# needs the same definition.
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

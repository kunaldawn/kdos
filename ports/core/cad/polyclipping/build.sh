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

# The release is a flat zip (cpp/, C#/, Delphi/ side by side), so it arrives
# in $SRC whole. Only cpp/ is the library. VERSION fills the Version: line of
# polyclipping.pc, which is empty otherwise and fails every versioned
# pkg-config check. The header installs under include/polyclipping/ because
# clipper.hpp is too generic a name for include/.
unzip -q "$name-$version.zip"
mkdir cpp/build && cd cpp/build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DBUILD_SHARED_LIBS=ON \
	-DVERSION=$version
make
make DESTDIR=$PKG install
test -f "$PKG/usr/include/polyclipping/clipper.hpp"
test -f "$PKG/usr/lib/libpolyclipping.so"

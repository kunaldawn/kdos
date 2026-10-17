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

cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DCMAKE_BUILD_WITH_INSTALL_RPATH=ON \
	-DLPCNET=OFF \
	-DUNITTEST=OFF
ninja -C build
DESTDIR=$PKG ninja -C build install

for t in c2enc c2dec freedv_tx freedv_rx freedv_data_raw_tx freedv_data_raw_rx; do
	install -Dm755 "build/src/$t" "$PKG/usr/bin/$t"
done

test -e "$PKG/usr/lib/libcodec2.so"
test -e "$PKG/usr/lib/pkgconfig/codec2.pc"

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

# TLS is off: its transports need an engine chosen at build time, and nothing
# here speaks tls+tcp or wss. The manual pages are generated with asciidoctor;
# the same step renders every page to HTML as well, which is removed.
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DNNG_TESTS=OFF \
	-DNNG_TOOLS=ON \
	-DNNG_ENABLE_NNGCAT=ON \
	-DNNG_ENABLE_TLS=OFF \
	-DNNG_ENABLE_HTTP=ON \
	-DNNG_ENABLE_DOC=ON
ninja -C build
DESTDIR=$PKG ninja -C build install
rm -rf "$PKG/usr/share/doc"

test -f "$PKG/usr/lib/libnng.so"
test -f "$PKG/usr/include/nng/protocol/pubsub0/pub.h"

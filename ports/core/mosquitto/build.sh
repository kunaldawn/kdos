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

mkdir -p build && cd build
# WITH_DOCS, not DOCUMENTATION: cmake reports an unknown -D as a WARNING and
# carries on, so a misspelt option is a setting that silently does nothing.
# WITH_TESTS wants gtest, which is not a port. cJSON is found by pkg-config
# with no option of its own.
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_INSTALL_SYSCONFDIR=/etc \
	-DWITH_WEBSOCKETS=OFF -DWITH_DOCS=ON -DWITH_TESTS=OFF \
	-DWITH_TLS=ON
make
make DESTDIR=$PKG install

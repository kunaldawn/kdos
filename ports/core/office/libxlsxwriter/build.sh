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

# The vendored minizip and the built-in MD5 are the defaults and are kept.
# USE_SYSTEM_MINIZIP wants a minizip this tree does not build, and
# USE_OPENSSL_MD5 would put libcrypto behind a spreadsheet to deduplicate
# images nothing here embeds.
#
# USE_DTOA_LIBRARY formats doubles with the vendored emyg_dtoa instead of
# printf, so a comma-decimal locale cannot write "1,5" into a cell's XML and
# produce a file Excel rejects.
#
# SHARED, because the one consumer is sc-im and a static archive would put a
# copy of the writer inside it — the same rule every other library here
# follows. The installed `xlsxwriter.pc` is what sc-im's Makefile probes for;
# without it the build silently produces a spreadsheet that cannot export.
cmake .. -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=OFF \
	-DUSE_DTOA_LIBRARY=ON
make
make DESTDIR=$PKG install

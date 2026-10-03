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


# Header-only. `make` and `make install` build its tests and a timing tool,
# none of them installed, and the tests do not compile as C++20: they hand the
# containers std::allocator, whose rebind and member types C++20 removed. The
# containers' own default allocator defines both, so the headers serve C++20
# code. install-data installs the headers and the pkg-config file and builds
# nothing; the one generated header is made first.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib
make src/sparsehash/internal/sparseconfig.h
make DESTDIR=$PKG install-data

rm -rf "$PKG/usr/share/doc"

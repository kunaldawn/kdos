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

# Header-only: the headers and asio.pc are installed by their own targets,
# because the top-level `make install` first builds every example program
# under src/. Boost stays out, so the headers are the standalone `asio::`
# namespace.
./autogen.sh
./configure --prefix=/usr --without-boost --without-openssl
make -C include DESTDIR=$PKG install
make DESTDIR=$PKG install-noarch_pkgconfigDATA
install -Dm644 LICENSE_1_0.txt "$PKG/usr/share/licenses/asio/LICENSE_1_0.txt"

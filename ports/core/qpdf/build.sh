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

# IT IS NOT A RENDERER AND THAT IS THE DIVISION OF LABOUR. poppler turns a PDF
# into pixels or text; qpdf works on the FILE — splitting, merging, removing a
# password you own, rewriting a damaged xref so something else can open it at
# all. Between them they cover what people actually need to do to a PDF without
# starting a GUI in a container.
#
# --enable-crypto-gnutls: qpdf can use gnutls, openssl or its own bundled
# implementation. gnutls is already here for wireshark and samba, and a
# bundled crypto is a third copy nobody patches.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_STATIC_LIBS=OFF \
	-DUSE_IMPLICIT_CRYPTO=OFF \
	-DREQUIRE_CRYPTO_GNUTLS=ON \
	-DBUILD_DOC=OFF
ninja
DESTDIR=$PKG ninja install
